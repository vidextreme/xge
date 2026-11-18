// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "xgResources.h"

namespace xg
{
    class ResourceSystemImpl : public ResourceSystem
    {
        XG_DECLARE_SUBSYSTEM(ResourceSystemImpl, ResourceSystem)
    public:
        ResourceSystemImpl();
        ~ResourceSystemImpl() override;

        void Shutdown() override;

        ResourceTypeID RegisterType(uint32_t moduleId, const ResourceTypeDesc* desc) override;
        ResourceTypeID FindType(const char* name) const override;
        bool UnregisterType(uint32_t moduleId, ResourceTypeID type) override;

        ResourceHandle Acquire(uint32_t moduleId, ResourceTypeID type, const ResourceID* id) override;
        bool AddRef(uint32_t moduleId, ResourceHandle handle) override;
        bool Release(uint32_t moduleId, ResourceHandle handle) override;

        ResourceState GetState(ResourceHandle handle) const override;
        void* GetData(ResourceHandle handle) const override;

        void ReleaseModule(uint32_t moduleId) override;
        void Collect() override;

    private:
        struct TypeEntry
        {
            std::string Name;
            ResourceTypeDesc Desc = {};
            uint32_t Owner = 0;
            bool Active = false;
        };

        struct ModuleReference
        {
            uint32_t Module = 0;
            uint32_t Count = 0;
        };

        struct Slot
        {
            uint32_t Generation = 1;
            bool Used = false;
            ResourceTypeID Type = 0;
            ResourceID Id = {};
            ResourceState State = ResourceState::Invalid;
            void* Data = nullptr;
            uint32_t TotalReferences = 0;
            std::vector<ModuleReference> References;
        };

        struct Key
        {
            ResourceTypeID Type;
            ResourceID Id;
            bool operator==(const Key& other) const;
        };

        struct KeyHash
        {
            size_t operator()(const Key& key) const;
        };

        const TypeEntry* FindActiveType(ResourceTypeID type) const;
        Slot* Resolve(ResourceHandle handle);
        const Slot* Resolve(ResourceHandle handle) const;
        uint32_t AllocateSlot();
        void AddReference(Slot& slot, uint32_t moduleId);
        void DestroySlot(uint32_t index);
        void CheckThread() const;

        std::vector<TypeEntry> _types;
        std::vector<Slot> _slots;
        std::vector<uint32_t> _freeSlots;
        std::vector<ResourceHandle> _unloadQueue;
        std::unordered_map<Key, uint32_t, KeyHash> _lookup;
        std::thread::id _mainThread;
    };
}
