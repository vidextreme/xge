// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgSceneNode.h"
#include "xgSystem.h"

namespace xg
{
    class SceneGraph;

    struct GraphMembership
    {
        SceneGraph* Graph;   // which graph
        SceneNode* Node;    // graph-specific node
    };

    class ActorRegistry : public System
    {
        XG_DECLARE_BASE_SYSTEM(ActorRegistry)

    public:
        virtual ~ActorRegistry() = default;

        // Generate a new global ActorID
        virtual SceneNode::ActorID GenerateID() = 0;

        // Create identity node (SceneNode base)
        virtual SceneNode* CreateIdentity(SceneNode::ActorID id) = 0;

        // Register graph-specific node
        virtual void RegisterGraphNode(SceneNode::ActorID id,
            SceneGraph* graph,
            SceneNode* node) = 0;

        // Unregister graph-specific node
        virtual void UnregisterGraphNode(SceneNode::ActorID id,
            SceneGraph* graph) = 0;

        // Retrieve identity node
        virtual SceneNode* GetIdentity(SceneNode::ActorID id) const = 0;

        // Retrieve graph-specific node
        virtual SceneNode* GetGraphNode(SceneNode::ActorID id,
            SceneGraph* graph) const = 0;

        // Retrieve all memberships for an actor
        virtual int GetMembershipCount(SceneNode::ActorID id) const = 0;
        virtual bool GetMembership(SceneNode::ActorID id,
            int index,
            GraphMembership& outMembership) const = 0;
    };
}