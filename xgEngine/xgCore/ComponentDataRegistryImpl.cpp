// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ComponentDataRegistryImpl.h"
#include "xgLog.h"
#include <cstring>
#include <cstdlib>

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
            TypeEntry* entry = _types[i].get();
            if (!entry)
                continue;
            if (entry->UpdaterModule == moduleId)
                entry->UpdaterModule = 0;
            if (entry->ModuleId == moduleId && (static_cast<uint32_t>(entry->Desc.Flags) & static_cast<uint32_t>(DataTypeFlags::Shared)) == 0)
                UnregisterType(moduleId, static_cast<DataTypeID>(i + 1));
        }
    }

    DataTypeID ComponentDataRegistryImpl::RegisterTypeWithLayout(uint32_t moduleId, const DataTypeDesc* desc, const char* layout)
    {
        if (!desc || !desc->Key)
            return 0;
        auto existing = _keys.find(desc->Key);
        if (existing == _keys.end())
        {
            const DataTypeID id = RegisterType(moduleId, desc);
            if (TypeEntry* entry = Find(id); entry && layout)
                entry->Layout = layout;
            return id;
        }
        TypeEntry* entry = Find(existing->second);
        if (entry->Desc.Size == desc->Size && entry->Desc.Align == desc->Align && entry->Desc.LayoutHash == desc->LayoutHash)
        {
            entry->ModuleId = moduleId;
            if (layout && entry->Layout.empty())
                entry->Layout = layout;
            return existing->second;
        }
        if ((static_cast<uint32_t>(entry->Desc.Flags) & static_cast<uint32_t>(DataTypeFlags::EngineOwned)) != 0 || entry->Layout.empty() || !layout || !layout[0]
            || desc->Size == 0 || desc->Align == 0 || desc->Align > 64 || (desc->Align & (desc->Align - 1)) != 0)
        {
            xg::Log(xg::MessageType::Warning, "[ComponentData] '%s' is already registered with another layout", desc->Key);
            return 0;
        }
        Migrate(*entry, *desc, layout);
        entry->ModuleId = moduleId;
        return existing->second;
    }

    namespace
    {
        struct FieldLayout
        {
            std::string Name;
            std::string Type;
            uint32_t Offset = 0;
            uint32_t Size = 0;
        };

        std::vector<FieldLayout> ParseLayout(const std::string& text)
        {
            std::vector<FieldLayout> fields;
            size_t start = 0;
            while (start < text.size())
            {
                size_t end = text.find(';', start);
                if (end == std::string::npos)
                    end = text.size();
                const std::string item = text.substr(start, end - start);
                start = end + 1;
                const size_t a = item.find(':');
                const size_t b = a == std::string::npos ? a : item.find(':', a + 1);
                const size_t c = b == std::string::npos ? b : item.find(':', b + 1);
                if (c == std::string::npos)
                    continue;
                FieldLayout field;
                field.Name = item.substr(0, a);
                field.Type = item.substr(a + 1, b - a - 1);
                field.Offset = static_cast<uint32_t>(std::strtoul(item.c_str() + b + 1, nullptr, 10));
                field.Size = static_cast<uint32_t>(std::strtoul(item.c_str() + c + 1, nullptr, 10));
                fields.push_back(std::move(field));
            }
            return fields;
        }

        bool SameField(const FieldLayout& a, const FieldLayout& b)
        {
            return a.Name == b.Name && a.Type == b.Type && a.Size == b.Size;
        }
    }

    void ComponentDataRegistryImpl::Migrate(TypeEntry& entry, const DataTypeDesc& desc, const char* layout)
    {
        const std::vector<FieldLayout> oldFields = ParseLayout(entry.Layout);
        const std::vector<FieldLayout> newFields = ParseLayout(layout);
        struct Copy
        {
            uint32_t From;
            uint32_t To;
            uint32_t Size;
        };
        std::vector<Copy> copies;
        std::string dropped;
        for (const FieldLayout& field : newFields)
        {
            for (const FieldLayout& match : oldFields)
            {
                if (SameField(match, field) && match.Offset + match.Size <= entry.Desc.Size && field.Offset + field.Size <= desc.Size)
                {
                    copies.push_back({ match.Offset, field.Offset, field.Size });
                    break;
                }
            }
        }
        for (const FieldLayout& field : oldFields)
        {
            bool kept = false;
            for (const FieldLayout& f : newFields)
                kept = kept || SameField(f, field);
            if (!kept)
            {
                if (!dropped.empty())
                    dropped += ", ";
                dropped += field.Name;
            }
        }

        ComponentPool& old = *entry.Pool;
        auto pool = std::make_unique<ComponentPool>(desc.Size, desc.Align);
        const uint32_t count = old.Count();
        for (uint32_t slot = 0; slot < count; ++slot)
        {
            const uint32_t to = pool->AppendRaw(old.Owner(slot), old.RefCount(slot), NextVersion());
            auto* src = static_cast<const uint8_t*>(old.Data(slot));
            auto* dst = static_cast<uint8_t*>(pool->Data(to));
            for (const Copy& copy : copies)
                std::memcpy(dst + copy.To, src + copy.From, copy.Size);
        }
        old.MoveListenersTo(*pool);
        entry.Pool = std::move(pool);
        entry.Desc = desc;
        entry.Desc.Key = entry.Key.c_str();
        entry.Layout = layout;
        xg::Log(xg::MessageType::Warning, "[ComponentData] '%s' changed layout: migrated %u element(s)%s%s", entry.Key.c_str(), count,
            dropped.empty() ? "" : ", dropped ", dropped.c_str());
    }

    bool ComponentDataRegistryImpl::ClaimUpdater(DataTypeID type, uint32_t moduleId)
    {
        TypeEntry* entry = Find(type);
        if (!entry)
            return false;
        if (entry->UpdaterModule == 0 || entry->UpdaterModule == moduleId)
        {
            entry->UpdaterModule = moduleId;
            return true;
        }
        return false;
    }

    void ComponentDataRegistryImpl::ReleaseUpdater(DataTypeID type, uint32_t moduleId)
    {
        if (TypeEntry* entry = Find(type); entry && entry->UpdaterModule == moduleId)
            entry->UpdaterModule = 0;
    }

    int32_t ComponentDataRegistryImpl::CopyTypes(DataTypeID* out, int32_t capacity) const
    {
        int32_t total = 0;
        for (size_t i = 0; i < _types.size(); ++i)
        {
            if (!_types[i])
                continue;
            if (out && total < capacity)
                out[total] = static_cast<DataTypeID>(i + 1);
            ++total;
        }
        return total;
    }

    int32_t ComponentDataRegistryImpl::Join(DataTypeID type, DataTypeID other, int32_t* outSlots, int32_t capacity) const
    {
        const TypeEntry* entry = Find(type);
        const TypeEntry* with = Find(other);
        if (!entry)
            return 0;
        const uint32_t count = entry->Pool->Count();
        for (uint32_t slot = 0; outSlots && slot < count && static_cast<int32_t>(slot) < capacity; ++slot)
            outSlots[slot] = with ? with->Pool->Find(entry->Pool->Owner(slot)) : -1;
        return static_cast<int32_t>(count);
    }
}
