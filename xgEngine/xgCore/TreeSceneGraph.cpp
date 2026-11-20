#include "pch.h"
#include "TreeSceneGraph.h"
#include <algorithm>
#include <vector>

namespace xg
{
    TreeSceneGraph::TreeSceneGraph(ActorRegistry& registry)
    {
        _registry = &registry;
    }

    TreeSceneGraph::~TreeSceneGraph()
    {
        // Graph-specific nodes are owned by whoever created them.
        // We only unregister them.
        for (auto& kv : _nodes)
            _registry->UnregisterGraphNode(kv.first, this);
    }

    void TreeSceneGraph::RegisterNode(SceneNode* node)
    {
        // Must have hierarchy mixin
        if (!(node->TypeMask & SceneNode_Hierarchy))
            return;

        auto h = node->Get<SceneNodeHierarchy>();

        h->Owner = node;

        _nodes[node->Id] = h;

        // Register membership
        _registry->RegisterGraphNode(node->Id, this, node);

        // Default parent = none (user must call SetParent)
    }

    void TreeSceneGraph::DestroyNode(SceneNode* node)
    {
        if (!node || _nodes.find(node->Id) == _nodes.end())
            return;

        std::vector<SceneNodeHierarchy*> order;
        std::vector<SceneNodeHierarchy*> stack{ node->Get<SceneNodeHierarchy>() };
        while (!stack.empty())
        {
            SceneNodeHierarchy* h = stack.back();
            stack.pop_back();
            order.push_back(h);
            for (SceneNodeHierarchy* child = h->FirstChild; child; child = child->NextSibling)
                stack.push_back(child);
        }

        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            SceneNodeHierarchy* h = *it;
            SceneNode* owner = h->Owner;
            const ActorID id = owner->Id;
            _registry->NotifyDestroyed(id);
            RemoveChild(h);
            _registry->UnregisterGraphNode(id, this);
            _nodes.erase(id);
            delete owner;
        }
    }

    void TreeSceneGraph::SetParent(SceneNode* childNode, SceneNode* parentNode)
    {
        auto* child = childNode ? childNode->Get<SceneNodeHierarchy>() : nullptr;
        if (!child)
            return;
        auto* parent = parentNode ? parentNode->Get<SceneNodeHierarchy>() : nullptr;

        RemoveChild(child);
        if (parent)
            AddChild(parent, child);
    }

    void TreeSceneGraph::Traverse(SceneNode* rootNode,
        void(*visitor)(SceneNode*))
    {
        auto* root = rootNode->Get<SceneNodeHierarchy>();
        if (!root)
            return;

        std::vector<SceneNodeHierarchy*> stack{ root };

        while (!stack.empty())
        {
            SceneNodeHierarchy* node = stack.back();
            stack.pop_back();
            SceneNode* snode = node->Owner;
            visitor(snode);

            for (auto* child = node->FirstChild;
                child != nullptr;
                child = child->NextSibling)
            {
                stack.push_back(child);
            }
        }
    }

    SceneNode* TreeSceneGraph::GetNode(ActorID id) const
    {
        auto it = _nodes.find(id);
        return (it != _nodes.end()) ? it->second->Owner : nullptr;
    }

    int TreeSceneGraph::CopyRootIds(ActorID* out, int capacity) const
    {
        std::vector<ActorID> roots;
        roots.reserve(_nodes.size());
        for (const auto& [id, node] : _nodes)
        {
            if (!node->Parent)
                roots.push_back(id);
        }
        std::sort(roots.begin(), roots.end());
        if (out)
        {
            const int count = (std::min)(capacity, static_cast<int>(roots.size()));
            for (int i = 0; i < count; ++i)
                out[i] = roots[i];
        }
        return static_cast<int>(roots.size());
    }

    void TreeSceneGraph::AddChild(SceneNodeHierarchy* parent,
        SceneNodeHierarchy* child)
    {
        child->Parent = parent;
        child->NextSibling = nullptr;
        SceneNodeHierarchy** link = &parent->FirstChild;
        while (*link)
            link = &(*link)->NextSibling;
        *link = child;
    }

    void TreeSceneGraph::RemoveChild(SceneNodeHierarchy* child)
    {
        auto* parent = child->Parent;
        if (!parent)
            return;

        SceneNodeHierarchy** link = &parent->FirstChild;

        while (*link)
        {
            if (*link == child)
            {
                *link = child->NextSibling;
                break;
            }
            link = &(*link)->NextSibling;
        }

        child->Parent = nullptr;
        child->NextSibling = nullptr;
    }
}
