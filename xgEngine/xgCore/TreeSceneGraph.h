#pragma once
#include "xgSceneGraph.h"
#include "xgActorRegistry.h"

namespace xg
{
    //
    // TreeSceneNode — graph-specific node type
    //
    class TreeSceneNode : public SceneNode
    {
    public:
        explicit TreeSceneNode(ActorID id)
            : SceneNode(id, SceneNode_Hierarchy)
        {}

        TreeSceneNode* Parent = nullptr;
        TreeSceneNode* FirstChild = nullptr;
        TreeSceneNode* NextSibling = nullptr;
    };

    //
    // TreeSceneGraph — hierarchy graph implementation
    //
    class TreeSceneGraph : public SceneGraph
    {
    public:
        explicit TreeSceneGraph(ActorRegistry& registry);

        ~TreeSceneGraph() override;

        void RegisterNode(SceneNode* identity) override;
        void DestroyNode(SceneNode* identity) override;

        void SetParent(SceneNode* child, SceneNode* parent) override;
        void UpdateTransforms() override;

        void Traverse(SceneNode* root,
            void(*visitor)(SceneNode*)) override;

        SceneNode* GetNode(SceneNode::ActorID id) const override;

    private:
        ActorRegistry& _registry;

        // Root of the hierarchy
        TreeSceneNode* _root = nullptr;

        // Local storage for graph-specific nodes
        // (ActorRegistry tracks membership; we track actual nodes)
        std::unordered_map<SceneNode::ActorID, TreeSceneNode*> _nodes;

        void AddChild(TreeSceneNode* parent, TreeSceneNode* child);
        void RemoveChild(TreeSceneNode* child);
    };
}
