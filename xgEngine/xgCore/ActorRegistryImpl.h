#pragma once
#include "xgActorRegistry.h"


namespace xg
{
    class ActorRegistryImpl : public ActorRegistry
    {
        XG_DECLARE_SYSTEM(ActorRegistryImpl, ActorRegistry)
    public:
        ActorRegistryImpl(uint16_t machineID = 1);
        ~ActorRegistryImpl() override;

        ActorID GenerateID() override;
        SceneNode* CreateIdentity(ActorID id) override;

        void RegisterGraphNode(ActorID id,
            SceneGraph* graph,
            SceneNode* node) override;

        void UnregisterGraphNode(ActorID id,
            SceneGraph* graph) override;

        SceneNode* GetIdentity(ActorID id) const override;

        SceneNode* GetGraphNode(ActorID id,
            SceneGraph* graph) const override;

        int GetMembershipCount(ActorID id) const override;
        bool GetMembership(ActorID id,
            int index,
            GraphMembership& outMembership) const override;

    private:
        struct ActorRecord
        {
            SceneNode* Identity = nullptr;
            std::vector<GraphMembership> Memberships;
        };

        xgIDGenerator _idGen;
        std::unordered_map<ActorID, ActorRecord> _actors;
    };
}
