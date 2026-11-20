// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgBase.h"
#include "xgSubsystem.h"
#include "xgSceneNode.h"

namespace xg
{
    // Native component data: one tightly packed pool per data type (a blittable struct), keyed by ActorID.
    // Each actor has at most one element per type; Add is reference-counted so several components can share it.
    // Elements move when others are removed (swap-and-pop), so never keep a pointer or slot across Add/Remove.
    // Main thread only.
    using DataTypeID = uint32_t;

    enum class DataTypeFlags : uint32_t
    {
        None = 0,
        // Owned by an engine system (e.g. "xg.Transform"): scripts may read it, but writes go through that system.
        EngineOwned = 1u << 0,
    };

    // Key is copied by RegisterType ("Full.Type.Name, Assembly" for managed types). Size > 0, Align a power of two <= 64.
    // LayoutHash identifies the field layout; registering an existing key with the same size and hash returns that type.
    struct DataTypeDesc
    {
        const char* Key;
        uint32_t Size;
        uint32_t Align;
        uint64_t LayoutHash;
        DataTypeFlags Flags;
    };

    // Pool events, for systems that keep columns parallel to a pool. Added runs after the element exists, Removed before
    // the slot is reused, Moved after an element was copied from one slot to another. Any callback may be null.
    struct DataPoolListener
    {
        void (*Added)(void* user, ActorID actor, uint32_t slot);
        void (*Removed)(void* user, ActorID actor, uint32_t slot);
        void (*Moved)(void* user, ActorID actor, uint32_t from, uint32_t to);
        void* User;
    };

    class ComponentDataRegistry : public Subsystem
    {
        XG_DECLARE_BASE_SUBSYSTEM(ComponentDataRegistry)

    public:
        virtual ~ComponentDataRegistry() = default;

        // Returns 0 for an invalid description, or when the key exists with another size, alignment or layout hash.
        virtual DataTypeID RegisterType(uint32_t moduleId, const DataTypeDesc* desc) = 0;
        virtual DataTypeID FindType(const char* key) const = 0;
        // Only the owning module can unregister. The pool and its elements are destroyed.
        virtual bool UnregisterType(uint32_t moduleId, DataTypeID type) = 0;
        // desc->Key points at the registry's copy and stays valid until the type is unregistered.
        virtual bool GetTypeDesc(DataTypeID type, DataTypeDesc* desc) const = 0;

        // Adds one reference to the actor's element, creating it zero-filled. Returns the element, or null.
        virtual void* Add(DataTypeID type, ActorID actor) = 0;
        // Drops one reference; removes the element at zero. Returns true when it was removed.
        virtual bool Release(DataTypeID type, ActorID actor) = 0;
        // Removes the element whatever its reference count. Returns false when there was none.
        virtual bool Remove(DataTypeID type, ActorID actor) = 0;
        // Removes the actor's element from every pool.
        virtual void RemoveActor(ActorID actor) = 0;

        // Null when the actor has no element. Valid until the next Add or Remove on that type.
        virtual void* Get(DataTypeID type, ActorID actor) = 0;
        // -1 when the actor has no element.
        virtual int32_t GetSlot(DataTypeID type, ActorID actor) const = 0;
        virtual uint32_t GetCount(DataTypeID type) const = 0;
        // The packed elements (stride = desc Size rounded up to Align) and their owners, slot order.
        virtual void* GetDense(DataTypeID type, const ActorID** owners, uint32_t* count) = 0;

        // Every change stamps the element with the next global version. Writers that bypass the registry call MarkChanged.
        virtual uint64_t GetCurrentVersion() const = 0;
        virtual uint64_t GetVersion(DataTypeID type, uint32_t slot) const = 0;
        virtual void MarkChanged(DataTypeID type, uint32_t slot) = 0;
        virtual void MarkAllChanged(DataTypeID type) = 0;
        // Writes up to capacity actors whose element changed after since; returns the total count.
        virtual int32_t CollectChanged(DataTypeID type, uint64_t since, ActorID* out, int32_t capacity) const = 0;

        virtual bool AddListener(DataTypeID type, const DataPoolListener* listener) = 0;
        virtual void RemoveListener(DataTypeID type, const DataPoolListener* listener) = 0;

        // Unregisters the module's types. The engine calls it after the module's Shutdown.
        virtual void ReleaseModule(uint32_t moduleId) = 0;
    };

    inline uint32_t DataStride(uint32_t size, uint32_t align)
    {
        return align ? (size + align - 1) / align * align : size;
    }
}
