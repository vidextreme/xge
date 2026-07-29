#pragma once
#include "xgSceneGraph.h"
#include "xgActorRegistry.h"
#include "xgSceneNodeEngine.h"   // mixin-based concrete node

namespace xg
{
    class TreeSceneGraph : public SceneGraph
    {
        XG_DECLARE_SYSTEM(SceneGraph, SceneGraph)
    public:
        explicit TreeSceneGraph(ActorRegistry& registry);

        ~TreeSceneGraph() override;

        void RegisterNode(SceneNode* node) override;
        void DestroyNode(SceneNode* node) override;

        void SetParent(SceneNode* child, SceneNode* parent) override;
        void Traverse(SceneNode* root, void(*visitor)(SceneNode*)) override;

        SceneNode* GetNode(SceneNode::ActorID id) const override;

    private:

        // Graph-specific storage (SceneNode* → SceneNodeHierarchy*)
        std::unordered_map<SceneNode::ActorID, SceneNodeHierarchy*> _nodes;

        void AddChild(SceneNodeHierarchy* parent, SceneNodeHierarchy* child);
        void RemoveChild(SceneNodeHierarchy* child);
    };
}
