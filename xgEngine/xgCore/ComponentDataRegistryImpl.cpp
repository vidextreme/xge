// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ComponentDataRegistryImpl.h"
#include "xgLog.h"

namespace xg
{
    ComponentDataRegistryImpl::~ComponentDataRegistryImpl()
    {
        Shutdown();
    }

    void ComponentDataRegistryImpl::Shutdown()
    {
        _types.clear();
        _keys.clear();
    }

    ComponentDataRegistryImpl::TypeEntry* ComponentDataRegistryImpl::Find(DataTypeID type)
    {
        return type > 0 && type <= _types.size() ? _types[type - 1].get() : nullptr;
    }

    const ComponentDataRegistryImpl::TypeEntry* ComponentDataRegistryImpl::Find(DataTypeID type) const
    {
        return type > 0 && type <= _types.size() ? _types[type - 1].get() : nullptr;
    }

    DataTypeID ComponentDataRegistryImpl::RegisterType(uint32_t moduleId, const DataTypeDesc* desc)
    {
        if (!desc || !desc->Key || !desc->Key[0] || desc->Size == 0 || desc->Align == 0 || desc->Align > 64 || (desc->Align & (desc->Align - 1)) != 0)
            return 0;
        auto existing = _keys.find(desc->Key);
        if (existing != _keys.end())
        {
            TypeEntry* entry = Find(existing->second);
            if (entry->Desc.Size != desc->Size || entry->Desc.Align != desc->Align || entry->Desc.LayoutHash != desc->LayoutHash)
            {
                xg::Log(xg::MessageType::Warning, "[ComponentData] '%s' is already registered with another layout", desc->Key);
                return 0;
            }
            entry->ModuleId = moduleId;
            return existing->second;
        }
        auto entry = std::make_unique<TypeEntry>();
        entry->Key = desc->Key;
        entry->Desc = *desc;
        entry->Desc.Key = entry->Key.c_str();
        entry->ModuleId = moduleId;
        entry->Pool = std::make_unique<ComponentPool>(desc->Size, desc->Align);
        _types.push_back(std::move(entry));
        const DataTypeID id = static_cast<DataTypeID>(_types.size());
        _keys.emplace(desc->Key, id);
        return id;
    }

    DataTypeID ComponentDataRegistryImpl::FindType(const char* key) const
    {
        if (!key)
            return 0;
        auto it = _keys.find(key);
        return it != _keys.end() ? it->second : 0;
    }

    bool ComponentDataRegistryImpl::UnregisterType(uint32_t moduleId, DataTypeID type)
    {
        TypeEntry* entry = Find(type);
        if (!entry || entry->ModuleId != moduleId)
            return false;
        entry->Pool->Clear();
        _keys.erase(entry->Key);
        _types[type - 1].reset();
        return true;
    }

    bool ComponentDataRegistryImpl::GetTypeDesc(DataTypeID type, DataTypeDesc* desc) const
    {
        const TypeEntry* entry = Find(type);
        if (!entry || !desc)
            return false;
        *desc = entry->Desc;
        return true;
    }

    void* ComponentDataRegistryImpl::Add(DataTypeID type, ActorID actor)
    {
        TypeEntry* entry = Find(type);
        if (!entry || actor == 0)
            return nullptr;
        const uint32_t slot = entry->Pool->Add(actor, NextVersion(), nullptr);
        ++entry->Pool->RefCount(slot);
        return entry->Pool->Data(slot);
    }

    bool ComponentDataRegistryImpl::Release(DataTypeID type, ActorID actor)
    {
        TypeEntry* entry = Find(type);
        const int32_t slot = entry ? entry->Pool->Find(actor) : -1;
        if (slot < 0)
            return false;
        uint32_t& refs = entry->Pool->RefCount(static_cast<uint32_t>(slot));
        if (refs > 1)
        {
            --refs;
            return false;
        }
        entry->Pool->RemoveAt(static_cast<uint32_t>(slot));
        return true;
    }

    bool ComponentDataRegistryImpl::Remove(DataTypeID type, ActorID actor)
    {
        TypeEntry* entry = Find(type);
        const int32_t slot = entry ? entry->Pool->Find(actor) : -1;
        if (slot < 0)
            return false;
        entry->Pool->RemoveAt(static_cast<uint32_t>(slot));
        return true;
    }

    void ComponentDataRegistryImpl::RemoveActor(ActorID actor)
    {
        for (auto& entry : _types)
        {
            if (!entry)
                continue;
            const int32_t slot = entry->Pool->Find(actor);
            if (slot >= 0)
                entry->Pool->RemoveAt(static_cast<uint32_t>(slot));
        }
    }

    void* ComponentDataRegistryImpl::Get(DataTypeID type, ActorID actor)
    {
        TypeEntry* entry = Find(type);
        const int32_t slot = entry ? entry->Pool->Find(actor) : -1;
        return slot >= 0 ? entry->Pool->Data(static_cast<uint32_t>(slot)) : nullptr;
    }

    int32_t ComponentDataRegistryImpl::GetSlot(DataTypeID type, ActorID actor) const
    {
        const TypeEntry* entry = Find(type);
        return entry ? entry->Pool->Find(actor) : -1;
    }

    uint32_t ComponentDataRegistryImpl::GetCount(DataTypeID type) const
    {
        const TypeEntry* entry = Find(type);
        return entry ? entry->Pool->Count() : 0;
    }

    void* ComponentDataRegistryImpl::GetDense(DataTypeID type, const ActorID** owners, uint32_t* count)
    {
        TypeEntry* entry = Find(type);
        if (owners)
            *owners = entry ? entry->Pool->Owners() : nullptr;
        if (count)
            *count = entry ? entry->Pool->Count() : 0;
        return entry ? entry->Pool->Dense() : nullptr;
    }

    uint64_t ComponentDataRegistryImpl::GetCurrentVersion() const
    {
        return _version;
    }

    uint64_t ComponentDataRegistryImpl::GetVersion(DataTypeID type, uint32_t slot) const
    {
        const TypeEntry* entry = Find(type);
        return entry && slot < entry->Pool->Count() ? entry->Pool->Version(slot) : 0;
    }

    void ComponentDataRegistryImpl::MarkChanged(DataTypeID type, uint32_t slot)
    {
        TypeEntry* entry = Find(type);
        if (entry && slot < entry->Pool->Count())
            entry->Pool->Stamp(slot, NextVersion());
    }

    void ComponentDataRegistryImpl::MarkAllChanged(DataTypeID type)
    {
        if (TypeEntry* entry = Find(type))
            entry->Pool->StampAll(NextVersion());
    }

    int32_t ComponentDataRegistryImpl::CollectChanged(DataTypeID type, uint64_t since, ActorID* out, int32_t capacity) const
    {
        const TypeEntry* entry = Find(type);
        if (!entry)
            return 0;
        int32_t total = 0;
        const uint32_t count = entry->Pool->Count();
        for (uint32_t slot = 0; slot < count; ++slot)
        {
            if (entry->Pool->Version(slot) <= since)
                continue;
            if (out && total < capacity)
                out[total] = entry->Pool->Owner(slot);
            ++total;
        }
        return total;
    }

    bool ComponentDataRegistryImpl::AddListener(DataTypeID type, const DataPoolListener* listener)
    {
        TypeEntry* entry = Find(type);
        if (!entry || !listener)
            return false;
        entry->Pool->AddListener(*listener);
        return true;
    }

    void ComponentDataRegistryImpl::RemoveListener(DataTypeID type, const DataPoolListener* listener)
    {
        if (TypeEntry* entry = Find(type); entry && listener)
            entry->Pool->RemoveListener(*listener);
    }

    void ComponentDataRegistryImpl::ReleaseModule(uint32_t moduleId)
    {
        for (size_t i = 0; i < _types.size(); ++i)
        {
            if (_types[i] && _types[i]->ModuleId == moduleId)
                UnregisterType(moduleId, static_cast<DataTypeID>(i + 1));
        }
    }
}
