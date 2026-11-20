// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgComponentData.h"
#include "ComponentPool.h"

namespace xg
{
    class ComponentDataRegistryImpl : public ComponentDataRegistry
    {
        XG_DECLARE_SUBSYSTEM(ComponentDataRegistryImpl, ComponentDataRegistry)
    public:
        ComponentDataRegistryImpl() = default;
        ~ComponentDataRegistryImpl() override;

        void Shutdown() override;

        DataTypeID RegisterType(uint32_t moduleId, const DataTypeDesc* desc) override;
        DataTypeID FindType(const char* key) const override;
        bool UnregisterType(uint32_t moduleId, DataTypeID type) override;
        bool GetTypeDesc(DataTypeID type, DataTypeDesc* desc) const override;

        void* Add(DataTypeID type, ActorID actor) override;
        bool Release(DataTypeID type, ActorID actor) override;
        bool Remove(DataTypeID type, ActorID actor) override;
        void RemoveActor(ActorID actor) override;

        void* Get(DataTypeID type, ActorID actor) override;
        int32_t GetSlot(DataTypeID type, ActorID actor) const override;
        uint32_t GetCount(DataTypeID type) const override;
        void* GetDense(DataTypeID type, const ActorID** owners, uint32_t* count) override;

        uint64_t GetCurrentVersion() const override;
        uint64_t GetVersion(DataTypeID type, uint32_t slot) const override;
        void MarkChanged(DataTypeID type, uint32_t slot) override;
        void MarkAllChanged(DataTypeID type) override;
        int32_t CollectChanged(DataTypeID type, uint64_t since, ActorID* out, int32_t capacity) const override;

        bool AddListener(DataTypeID type, const DataPoolListener* listener) override;
        void RemoveListener(DataTypeID type, const DataPoolListener* listener) override;

        void ReleaseModule(uint32_t moduleId) override;

    private:
        struct TypeEntry
        {
            std::string Key;
            DataTypeDesc Desc = {};
            uint32_t ModuleId = 0;
            std::unique_ptr<ComponentPool> Pool;
        };

        TypeEntry* Find(DataTypeID type);
        const TypeEntry* Find(DataTypeID type) const;
        uint64_t NextVersion() { return ++_version; }

        std::vector<std::unique_ptr<TypeEntry>> _types;
        std::unordered_map<std::string, DataTypeID> _keys;
        uint64_t _version = 0;
    };
}
