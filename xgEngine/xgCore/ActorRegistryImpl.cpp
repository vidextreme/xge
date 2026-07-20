#include "pch.h"
#include "ActorRegistryImpl.h"

namespace xg
{
    ActorRegistryImpl::ActorRegistryImpl()
    {}

    ActorRegistryImpl::~ActorRegistryImpl()
    {
        // Destroy identity nodes
        for (auto& [id, rec] : _actors)
            delete rec.Identity;
    }

    SceneNode::ActorID ActorRegistryImpl::GenerateID()
    {
        return _nextId++;
    }

    SceneNode* ActorRegistryImpl::CreateIdentity(SceneNode::ActorID id)
    {
        auto* node = new SceneNode(id, SceneNode_None);
        _actors[id].Identity = node;
        return node;
    }

    void ActorRegistryImpl::RegisterGraphNode(SceneNode::ActorID id,
        SceneGraph* graph,
        SceneNode* node)
    {
        _actors[id].Memberships.push_back({ graph, node });
    }

    void ActorRegistryImpl::UnregisterGraphNode(SceneNode::ActorID id,
        SceneGraph* graph)
    {
        auto it = _actors.find(id);
        if (it == _actors.end())
            return;

        auto& list = it->second.Memberships;

        list.erase(
            std::remove_if(list.begin(), list.end(),
                [graph](const GraphMembership& m)
                {
                    return m.Graph == graph;
                }),
            list.end()
        );
    }

    SceneNode* ActorRegistryImpl::GetIdentity(SceneNode::ActorID id) const
    {
        auto it = _actors.find(id);
        return (it != _actors.end()) ? it->second.Identity : nullptr;
    }

    SceneNode* ActorRegistryImpl::GetGraphNode(SceneNode::ActorID id,
        SceneGraph* graph) const
    {
        auto it = _actors.find(id);
        if (it == _actors.end())
            return nullptr;

        for (auto& m : it->second.Memberships)
            if (m.Graph == graph)
                return m.Node;

        return nullptr;
    }

    int ActorRegistryImpl::GetMembershipCount(SceneNode::ActorID id) const
    {
        auto it = _actors.find(id);
        return (it != _actors.end()) ? (int)it->second.Memberships.size() : 0;
    }

    bool ActorRegistryImpl::GetMembership(SceneNode::ActorID id,
        int index,
        GraphMembership& outMembership) const
    {
        auto it = _actors.find(id);
        if (it == _actors.end())
            return false;

        auto& list = it->second.Memberships;
        if (index < 0 || index >= (int)list.size())
            return false;

        outMembership = list[index];
        return true;
    }
}
