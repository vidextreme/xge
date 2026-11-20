// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgTransforms.h"
#include "xgMat4x4.h"

namespace xg
{
    class ScriptEngine;
    class SceneGraph;
    struct SceneNodeHierarchy;
    struct HierarchyNode;

    class TransformSystemImpl : public TransformSystem
    {
        XG_DECLARE_SUBSYSTEM(TransformSystemImpl, TransformSystem)
    public:
        TransformSystemImpl(ScriptEngine* engine, ComponentDataRegistry* registry, uint32_t ownerModuleId);
        ~TransformSystemImpl() override;

        void Shutdown() override;

        DataTypeID GetDataType() const override { return _type; }

        bool Add(ActorID actor) override;
        bool Remove(ActorID actor) override;
        bool Has(ActorID actor) const override;

        bool GetLocal(ActorID actor, TransformData* out) const override;
        bool SetLocal(ActorID actor, const TransformData* value) override;
        bool GetWorld(ActorID actor, TransformData* out) override;
        bool SetWorld(ActorID actor, const TransformData* value) override;
        bool GetWorldMatrix(ActorID actor, float* out16) override;

        bool SetParent(ActorID child, ActorID parent, bool keepWorld) override;

        void Update() override;

        int32_t GetLocals(const ActorID* actors, int32_t count, TransformData* out) const override;
        int32_t SetLocals(const ActorID* actors, int32_t count, const TransformData* values) override;
        int32_t GetWorlds(const ActorID* actors, int32_t count, TransformData* out) override;
        int32_t SetWorlds(const ActorID* actors, int32_t count, const TransformData* values) override;

        uint64_t GetCurrentVersion() const override;
        int32_t CollectChanged(uint64_t since, ActorID* out, int32_t capacity) const override;
        int32_t CopyActors(ActorID* out, int32_t capacity) const override;

    private:
        static void OnAdded(void* user, ActorID actor, uint32_t slot);
        static void OnRemoved(void* user, ActorID actor, uint32_t slot);
        static void OnMoved(void* user, ActorID actor, uint32_t from, uint32_t to);

        SceneGraph* Graph() const;
        HierarchyNode* NodeOf(ActorID actor) const;
        int32_t SlotOf(ActorID actor) const;
        TransformData& Local(uint32_t slot) const;
        const Mat4x4& WorldAt(uint32_t slot);
        Mat4x4 ParentWorldOf(SceneNodeHierarchy* node);
        void SetLocalAt(uint32_t slot, const TransformData& value);
        void SetWorldAt(uint32_t slot, const Mat4x4& world);
        void MarkDescendantsDirty(SceneNodeHierarchy* node);

        ScriptEngine* _engine = nullptr;
        ComponentDataRegistry* _registry = nullptr;
        DataTypeID _type = 0;
        uint32_t _ownerModuleId = 0;
        DataPoolListener _listener = {};
        std::vector<Mat4x4> _world;
        std::vector<uint8_t> _dirty;
        bool _anyDirty = false;
        std::vector<ActorID> _roots;
    };
}
