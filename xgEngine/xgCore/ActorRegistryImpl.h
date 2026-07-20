#pragma once
#include "xgActorRegistry.h"


namespace xg
{
    class ActorRegistryImpl : public ActorRegistry
    {
        XG_DECLARE_SYSTEM(ActorRegistryImpl, ActorRegistry)
    public:
        ActorRegistryImpl();
        ~ActorRegistryImpl() override;

        SceneNode::ActorID GenerateID() override;
        SceneNode* CreateIdentity(SceneNode::ActorID id) override;

        void RegisterGraphNode(SceneNode::ActorID id,
            SceneGraph* graph,
            SceneNode* node) override;

        void UnregisterGraphNode(SceneNode::ActorID id,
            SceneGraph* graph) override;

        SceneNode* GetIdentity(SceneNode::ActorID id) const override;

        SceneNode* GetGraphNode(SceneNode::ActorID id,
            SceneGraph* graph) const override;

        int GetMembershipCount(SceneNode::ActorID id) const override;
        bool GetMembership(SceneNode::ActorID id,
            int index,
            GraphMembership& outMembership) const override;

    private:
        struct ActorRecord
        {
            SceneNode* Identity = nullptr;
            std::vector<GraphMembership> Memberships;
        };

        SceneNode::ActorID _nextId = 1;
        std::unordered_map<SceneNode::ActorID, ActorRecord> _actors;
    };
}
