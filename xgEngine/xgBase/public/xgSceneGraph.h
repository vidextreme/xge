// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgSceneNode.h"
#include "xgSystem.h"
#include "xgActorRegistry.h"

namespace xg
{
	class ActorRegistry;
    class SceneGraph : public System
    {
        XG_DECLARE_BASE_SYSTEM(SceneGraph)
    public:

        virtual ~SceneGraph() = default;

        virtual void RegisterNode(SceneNode* node) = 0;
        virtual void DestroyNode(SceneNode* node) = 0;

        virtual void SetParent(SceneNode* child, SceneNode* parent) = 0;

        virtual void Traverse(SceneNode* root,
            void(*visitor)(SceneNode*)) = 0;

        virtual SceneNode* GetNode(SceneNode::ActorID id) const = 0;
        inline ActorRegistry& GetRegistry() const { return *_registry; }
    protected:
        ActorRegistry* _registry;
    };

    template<typename T>
    T* CreateNode(SceneGraph& graph, SceneNode::ActorID id)
    {
        T* node = new T(id);
        graph.RegisterNode(node);
        return node;
    }

    template<typename T>
    T* CreateNode(SceneGraph& graph)
    {
		auto id = graph.GetRegistry().GenerateID();
        T* node = new T(id);
        graph.RegisterNode(node);
        return node;
    }

} // namespace xg