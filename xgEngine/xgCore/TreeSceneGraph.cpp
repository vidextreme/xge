#include "pch.h"
#include "TreeSceneGraph.h"

namespace xg
{
    TreeSceneGraph::TreeSceneGraph(ActorRegistry& registry)
        : _registry(registry)
    {
        // Create root identity if needed
        SceneNode::ActorID rootId = _registry.GenerateID();
        SceneNode* identity = _registry.CreateIdentity(rootId);

        _root = new TreeSceneNode(rootId);
        _nodes[rootId] = _root;

        _registry.RegisterGraphNode(rootId, this, _root);
    }

    TreeSceneGraph::~TreeSceneGraph()
    {
        // Destroy all graph-specific nodes
        for (auto& kv : _nodes)
            delete kv.second;
    }

    void TreeSceneGraph::RegisterNode(SceneNode* identity)
    {
        SceneNode::ActorID id = identity->Id;

        auto* node = new TreeSceneNode(id);
        _nodes[id] = node;

        // Register membership with ActorRegistry
        _registry.RegisterGraphNode(id, this, node);

        // Default parent = root
        AddChild(_root, node);
    }

    void TreeSceneGraph::DestroyNode(SceneNode* identity)
    {
        SceneNode::ActorID id = identity->Id;

        auto it = _nodes.find(id);
        if (it == _nodes.end())
            return;

        TreeSceneNode* node = it->second;

        // Remove from hierarchy
        RemoveChild(node);

        // Unregister membership
        _registry.UnregisterGraphNode(id, this);

        delete node;
        _nodes.erase(it);
    }

    void TreeSceneGraph::SetParent(SceneNode* childIdentity,
        SceneNode* parentIdentity)
    {
        auto* child = static_cast<TreeSceneNode*>(GetNode(childIdentity->Id));
        auto* parent = static_cast<TreeSceneNode*>(GetNode(parentIdentity->Id));

        if (!child || !parent)
            return;

        RemoveChild(child);
        AddChild(parent, child);
    }

    void TreeSceneGraph::UpdateTransforms()
    {
        // TreeSceneGraph does not compute transforms.
        // TransformSceneGraph handles TRS.
    }

    void TreeSceneGraph::Traverse(SceneNode* rootIdentity,
        void(*visitor)(SceneNode*))
    {
        TreeSceneNode* root = static_cast<TreeSceneNode*>(GetNode(rootIdentity->Id));
        if (!root)
            return;

        TreeSceneNode* stack[256];
        int sp = 0;

        stack[sp++] = root;

        while (sp > 0)
        {
            TreeSceneNode* node = stack[--sp];
            visitor(node);

            for (TreeSceneNode* child = node->FirstChild;
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
        return (it != _nodes.end()) ? it->second : nullptr;
    }

    void TreeSceneGraph::AddChild(TreeSceneNode* parent, TreeSceneNode* child)
    {
        child->Parent = parent;

        child->NextSibling = parent->FirstChild;
        parent->FirstChild = child;
    }

    void TreeSceneGraph::RemoveChild(TreeSceneNode* child)
    {
        TreeSceneNode* parent = child->Parent;
        if (!parent)
            return;

        TreeSceneNode** link = &parent->FirstChild;

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
