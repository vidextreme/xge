#include "pch.h"
#include "TreeSceneGraph.h"

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
        auto* child = childNode->Get<SceneNodeHierarchy>();
        auto* parent = parentNode->Get<SceneNodeHierarchy>();

        RemoveChild(child);
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

    SceneNode* TreeSceneGraph::GetNode(SceneNode::ActorID id) const
    {
        auto it = _nodes.find(id);
        return (it != _nodes.end()) ? it->second->Owner : nullptr;
    }

    void TreeSceneGraph::AddChild(SceneNodeHierarchy* parent,
        SceneNodeHierarchy* child)
    {
        child->Parent = parent;
        child->NextSibling = parent->FirstChild;
        parent->FirstChild = child;
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
