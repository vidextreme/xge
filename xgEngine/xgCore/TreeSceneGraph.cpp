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
        auto it = _nodes.find(node->Id);
        if (it == _nodes.end())
            return;

        auto* h = it->second;

        RemoveChild(h);

        _registry->UnregisterGraphNode(node->Id, this);

        _nodes.erase(it);
        delete node;
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

        SceneNodeHierarchy* stack[256];
        int sp = 0;

        stack[sp++] = root;

        while (sp > 0)
        {
            SceneNodeHierarchy* node = stack[--sp];
            SceneNode* snode = node->Owner;
            visitor(snode);

            for (auto* child = node->FirstChild;
                child != nullptr;
                child = child->NextSibling)
            {
                stack[sp++] = child;
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
