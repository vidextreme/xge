// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgComponentData.h"
#include "xgResources.h"

namespace xg
{
    enum class ObjectDriverFlags : uint32_t
    {
        None = 0,
        Hidden = 1u << 0,
    };

    // An object driver: a composition object (.xco asset, built to .xdo) drawn at its actor. Lives in the
    // "xg.ObjectDriver" pool, so components of any module (CoreCLR, NativeAOT, native) can drive objects.
    struct ObjectDriverData
    {
        ResourceGUID Object;
        ObjectDriverFlags Flags;
        uint32_t Reserved;
    };
    static_assert(sizeof(ObjectDriverData) == 24, "ObjectDriverData is mirrored in managed code");

    inline constexpr const char* ObjectDriverDataKey = "xg.ObjectDriver";
    inline constexpr const char* ObjectResourceType = "object";

    // Draws every element of the "xg.ObjectDriver" pool: it acquires each element's object resource and the models and
    // materials its parts name (following changes and rebuilt assets), places every part with the node's baked pose and
    // the actor's transform (identity without one), and adds the draws to the frame's scene draws, in edit mode too.
    // Main thread only.
    class ObjectDriveSystem : public Subsystem
    {
        XG_DECLARE_BASE_SUBSYSTEM(ObjectDriveSystem)

    public:
        virtual ~ObjectDriveSystem() = default;

        virtual DataTypeID GetDataType() const = 0;
        // Draws added last frame, and how many object instances were ready.
        virtual int32_t GetDrawCount() const = 0;
        virtual int32_t GetReadyCount() const = 0;

        // World matrix (16 floats, row-major for row vectors) of the named node of an actor's object. False when the
        // actor has no driver, its object isn't loaded or no node has that name. Exposed proxies are found first.
        virtual bool GetNodeWorld(ActorID actor, const char* name, float* outWorld) = 0;

        // The submesh item GUID of a part of a Ready "model" resource (index into ModelResourceInfo::Parts). False for a
        // bad handle or index.
        virtual bool GetModelPartItem(ResourceHandle model, int32_t index, ResourceGUID* outItem) const = 0;

        // Composition Actors: the Objects exposed as actors, spawned as children of the driver's actor (or of their
        // nearest exposed ancestor) once the object loads, named after their Object, with a transform the system keeps
        // at the Jointer pose (scripts can read it; writes are put back). They're destroyed with the driver or when
        // the object changes.
        // The Composition Actor of a driver by Object name (0 when none).
        virtual ActorID GetCompositionActor(ActorID driver, const char* name) const = 0;
        // The driver actor that owns a Composition Actor (0 when the actor isn't one).
        virtual ActorID GetCompositionOwner(ActorID actor) const = 0;
        // Takes the oldest spawned Composition Actor whose authored components haven't been added yet (every spawned
        // actor is queued; "[]" when it has none): writes its id
        // and returns the byte count of its components JSON array, copying it into json when capacity allows (call
        // again with a larger buffer: the entry stays until it fits). -1 when none is waiting.
        virtual int32_t TakeSpawned(ActorID* outActor, char* json, int32_t capacity) = 0;
    };
}
