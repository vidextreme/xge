#pragma once
#include "xgSceneNode.h"

namespace xg
{
    class SceneGraph
    {
    public:
        virtual ~SceneGraph() = default;

        virtual void RegisterNode(SceneNode* node) = 0;
        virtual void DestroyNode(SceneNode* node) = 0;

        virtual void SetParent(SceneNode* child, SceneNode* parent) = 0;
        virtual void UpdateTransforms() = 0;

        virtual void Traverse(SceneNode* root,
            void(*visitor)(SceneNode*)) = 0;

        virtual SceneNode* GetNode(SceneNode::ActorID id) const = 0;
    };

    template<typename T>
    T* CreateNode(SceneGraph& graph, SceneNode::ActorID id)
    {
        T* node = new T(id);
        graph.RegisterNode(node);
        return node;
    }

} // namespace xg
