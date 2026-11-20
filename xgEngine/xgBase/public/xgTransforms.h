// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgBase.h"
#include "xgSubsystem.h"
#include "xgSceneNode.h"
#include "xgComponentData.h"

namespace xg
{
    // A pose as it crosses module and language boundaries (40 bytes): position, rotation quaternion (x, y, z, w), scale.
    // It is also the element of the "xg.Transform" component data pool.
    struct TransformData
    {
        float Position[3];
        float Rotation[4];
        float Scale[3];
    };
    static_assert(sizeof(TransformData) == 40, "TransformData must stay 40 bytes");

    inline constexpr const char* TransformDataKey = "xg.Transform";

    inline constexpr TransformData IdentityTransformData()
    {
        return { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f } };
    }

    // Optional transforms for actors. The local pose lives in the "xg.Transform" pool (engine-owned: write through this
    // system); world matrices are derived from the root SceneGraph's hierarchy, which stays the only parenting. An actor
    // without a transform passes its parent's world matrix on to its children. Matrices are row-major for row vectors
    // (v * M): world = Scale * Rotation * Translation * parentWorld. Main thread only.
    class TransformSystem : public Subsystem
    {
        XG_DECLARE_BASE_SUBSYSTEM(TransformSystem)

    public:
        virtual ~TransformSystem() = default;

        virtual DataTypeID GetDataType() const = 0;

        // Adds an identity local transform. Returns false when the actor already has one or the id is 0.
        virtual bool Add(ActorID actor) = 0;
        virtual bool Remove(ActorID actor) = 0;
        virtual bool Has(ActorID actor) const = 0;

        virtual bool GetLocal(ActorID actor, TransformData* out) const = 0;
        virtual bool SetLocal(ActorID actor, const TransformData* value) = 0;
        // World pose (decomposed world matrix); computed on demand when stale.
        virtual bool GetWorld(ActorID actor, TransformData* out) = 0;
        virtual bool SetWorld(ActorID actor, const TransformData* value) = 0;
        virtual bool GetWorldMatrix(ActorID actor, float* out16) = 0;

        // Reparents in the root SceneGraph (parent 0 = root). keepWorld recomputes the local pose so the world pose stays.
        // Returns false for unknown ids or when parent is the child or one of its descendants.
        virtual bool SetParent(ActorID child, ActorID parent, bool keepWorld) = 0;

        // Recomputes stale world matrices, parents first. The engine calls it after the actor updates.
        virtual void Update() = 0;

        // Batches: return how many actors had a transform; missing ones read as identity and are skipped on write.
        virtual int32_t GetLocals(const ActorID* actors, int32_t count, TransformData* out) const = 0;
        virtual int32_t SetLocals(const ActorID* actors, int32_t count, const TransformData* values) = 0;
        virtual int32_t GetWorlds(const ActorID* actors, int32_t count, TransformData* out) = 0;
        virtual int32_t SetWorlds(const ActorID* actors, int32_t count, const TransformData* values) = 0;

        // Change versions (the component data registry's counter): local writes and world recomputes stamp the actor.
        virtual uint64_t GetCurrentVersion() const = 0;
        virtual int32_t CollectChanged(uint64_t since, ActorID* out, int32_t capacity) const = 0;

        // Writes up to capacity actors that have a transform (pool order) and returns the total count.
        virtual int32_t CopyActors(ActorID* out, int32_t capacity) const = 0;
    };
}
