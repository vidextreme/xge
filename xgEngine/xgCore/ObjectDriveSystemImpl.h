// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include "xgObjectDriver.h"
#include "xgRenderer.h"

namespace xg
{
    class TransformSystem;
    class ScriptEngine;
    class SceneGraph;
    struct ObjectResourceData;

    class ObjectDriveSystemImpl : public ObjectDriveSystem
    {
        XG_DECLARE_SUBSYSTEM(ObjectDriveSystemImpl, ObjectDriveSystem)
    public:
        ObjectDriveSystemImpl(ScriptEngine* engine, ComponentDataRegistry* registry, ResourceSystem* resources, TransformSystem* transforms, uint32_t ownerModuleId);
        ~ObjectDriveSystemImpl() override;

        void Shutdown() override;
        // Releases every resource and stops listening (the engine calls it before subsystems shut down in any order).
        void Detach();

        DataTypeID GetDataType() const override { return _type; }
        int32_t GetDrawCount() const override { return _drawCount; }
        int32_t GetReadyCount() const override { return _readyCount; }
        bool GetNodeWorld(ActorID actor, const char* name, float* outWorld) override;
        bool GetModelPartItem(ResourceHandle model, int32_t index, ResourceGUID* outItem) const override;
        ActorID GetCompositionActor(ActorID driver, const char* name) const override;
        ActorID GetCompositionOwner(ActorID actor) const override;
        int32_t TakeSpawned(ActorID* outActor, char* json, int32_t capacity) override;

        // Appends the frame's object draws (the engine calls it after the transform update, before the mesh renderers).
        void Update(Renderer* renderer, std::vector<SceneDraw>& draws);

    private:
        struct PartState
        {
            ResourceHandle Model = 0;
            ResourceHandle Material = 0;
        };

        struct SpawnedActor
        {
            ActorID Id = 0;
            int32_t Node = -1;
        };

        struct SlotState
        {
            ResourceGUID ObjectGuid = {};
            ResourceHandle Object = 0;
            std::vector<PartState> Parts;
            bool PartsAcquired = false;
            ActorID Owner = 0;
            std::vector<SpawnedActor> Actors;
            bool ActorsSpawned = false;
        };

        struct PendingComponents
        {
            ActorID Actor = 0;
            std::string JSON;
        };

        static void OnRemoved(void* user, ActorID actor, uint32_t slot);
        static void OnMoved(void* user, ActorID actor, uint32_t from, uint32_t to);

        void ReleaseParts(SlotState& state);
        void DespawnActors(SlotState& state);
        void SpawnActors(SlotState& state, ActorID owner, const ObjectResourceData& object);
        void HoldActors(SlotState& state, const ObjectResourceData& object);
        SceneGraph* Graph() const;
        void ReleaseState(SlotState& state);
        void Track(SlotState& state, const ObjectDriverData& data);
        void RefreshPart(PartState& part, const ResourceGUID& model, const ResourceGUID& material);
        void ResetAll();
        const ObjectResourceData* ReadyObject(const SlotState& state) const;
        void ActorWorld(ActorID actor, float* out) const;

        ScriptEngine* _engine = nullptr;
        ComponentDataRegistry* _registry = nullptr;
        ResourceSystem* _resources = nullptr;
        TransformSystem* _transforms = nullptr;
        uint32_t _ownerModuleId = 0;
        DataTypeID _type = 0;
        DataPoolListener _listener = {};
        Renderer* _renderer = nullptr;
        ResourceTypeID _objectType = 0;
        ResourceTypeID _modelType = 0;
        ResourceTypeID _materialType = 0;
        std::vector<SlotState> _slots;
        std::unordered_map<ActorID, ActorID> _owners;
        std::deque<PendingComponents> _pending;
        int32_t _drawCount = 0;
        int32_t _readyCount = 0;
    };
}
