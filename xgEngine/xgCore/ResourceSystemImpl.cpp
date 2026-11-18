// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ResourceSystemImpl.h"
#include "xgLog.h"

namespace xg
{
    namespace
    {
        bool SameGUID(const ResourceGUID& a, const ResourceGUID& b)
        {
            return a.Hi == b.Hi && a.Lo == b.Lo;
        }

        ResourceHandle MakeHandle(uint32_t index, uint32_t generation)
        {
            return (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(index + 1);
        }

        struct IDText
        {
            char Text[ResourceIDTextCapacity] = {};
            explicit IDText(const ResourceID& id) { FormatResourceID(&id, Text, ResourceIDTextCapacity); }
        };
    }

    bool ResourceSystemImpl::Key::operator==(const Key& other) const
    {
        return Type == other.Type && SameGUID(Id.File, other.Id.File) && SameGUID(Id.Chunk, other.Id.Chunk);
    }

    size_t ResourceSystemImpl::KeyHash::operator()(const Key& key) const
    {
        uint64_t hash = 1469598103934665603ull;
        const uint64_t parts[5] = { key.Type, key.Id.File.Hi, key.Id.File.Lo, key.Id.Chunk.Hi, key.Id.Chunk.Lo };
        for (uint64_t part : parts)
        {
            hash ^= part;
            hash *= 1099511628211ull;
        }
        return static_cast<size_t>(hash);
    }

    ResourceSystemImpl::ResourceSystemImpl()
        : _mainThread(std::this_thread::get_id())
    {}

    ResourceSystemImpl::~ResourceSystemImpl() = default;

    void ResourceSystemImpl::CheckThread() const
    {
#ifdef _DEBUG
        if (std::this_thread::get_id() != _mainThread)
            xg::Log(xg::MessageType::Error, "[Resources] called from a thread other than the main thread");
#endif
    }

    void ResourceSystemImpl::Shutdown()
    {
        Collect();
        int leaked = 0;
        for (uint32_t index = 0; index < static_cast<uint32_t>(_slots.size()); ++index)
        {
            if (!_slots[index].Used)
                continue;
            ++leaked;
            xg::Log(xg::MessageType::Warning, "[Resources] still alive at shutdown: %s", IDText(_slots[index].Id).Text);
            DestroySlot(index);
        }
        xg::Log(xg::MessageType::Info, "[Resources] shutdown: %d resource(s) still alive", leaked);
    }

    const ResourceSystemImpl::TypeEntry* ResourceSystemImpl::FindActiveType(ResourceTypeID type) const
    {
        if (type == 0 || type > _types.size())
            return nullptr;
        const TypeEntry& entry = _types[type - 1];
        return entry.Active ? &entry : nullptr;
    }

    ResourceTypeID ResourceSystemImpl::RegisterType(uint32_t moduleId, const ResourceTypeDesc* desc)
    {
        CheckThread();
        if (!desc || !desc->Name || !*desc->Name || !desc->Load)
        {
            xg::Log(xg::MessageType::Warning, "[Resources] RegisterType: invalid description");
            return 0;
        }
        if (FindType(desc->Name) != 0)
        {
            xg::Log(xg::MessageType::Warning, "[Resources] RegisterType: '%s' is already registered", desc->Name);
            return 0;
        }
        TypeEntry entry;
        entry.Name = desc->Name;
        entry.Desc = *desc;
        entry.Desc.Name = nullptr;
        entry.Owner = moduleId;
        entry.Active = true;
        _types.push_back(std::move(entry));
        return static_cast<ResourceTypeID>(_types.size());
    }

    ResourceTypeID ResourceSystemImpl::FindType(const char* name) const
    {
        if (!name)
            return 0;
        for (size_t i = 0; i < _types.size(); ++i)
        {
            if (_types[i].Active && _types[i].Name == name)
                return static_cast<ResourceTypeID>(i + 1);
        }
        return 0;
    }

    bool ResourceSystemImpl::UnregisterType(uint32_t moduleId, ResourceTypeID type)
    {
        CheckThread();
        const TypeEntry* entry = FindActiveType(type);
        if (!entry || entry->Owner != moduleId)
        {
            xg::Log(xg::MessageType::Warning, "[Resources] UnregisterType: module %u doesn't own type %u", moduleId, type);
            return false;
        }
        _types[type - 1].Active = false;
        for (uint32_t index = 0; index < static_cast<uint32_t>(_slots.size()); ++index)
        {
            if (_slots[index].Used && _slots[index].Type == type)
                DestroySlot(index);
        }
        return true;
    }

    ResourceSystemImpl::Slot* ResourceSystemImpl::Resolve(ResourceHandle handle)
    {
        return const_cast<Slot*>(static_cast<const ResourceSystemImpl*>(this)->Resolve(handle));
    }

    const ResourceSystemImpl::Slot* ResourceSystemImpl::Resolve(ResourceHandle handle) const
    {
        const uint32_t slotNumber = static_cast<uint32_t>(handle);
        if (slotNumber == 0 || slotNumber > _slots.size())
            return nullptr;
        const Slot& slot = _slots[slotNumber - 1];
        return slot.Used && slot.Generation == static_cast<uint32_t>(handle >> 32) ? &slot : nullptr;
    }

    uint32_t ResourceSystemImpl::AllocateSlot()
    {
        if (!_freeSlots.empty())
        {
            const uint32_t index = _freeSlots.back();
            _freeSlots.pop_back();
            return index;
        }
        _slots.emplace_back();
        return static_cast<uint32_t>(_slots.size() - 1);
    }

    void ResourceSystemImpl::AddReference(Slot& slot, uint32_t moduleId)
    {
        ++slot.TotalReferences;
        for (ModuleReference& reference : slot.References)
        {
            if (reference.Module == moduleId)
            {
                ++reference.Count;
                return;
            }
        }
        slot.References.push_back({ moduleId, 1 });
    }

    ResourceHandle ResourceSystemImpl::Acquire(uint32_t moduleId, ResourceTypeID type, const ResourceID* id)
    {
        CheckThread();
        const TypeEntry* entry = FindActiveType(type);
        if (!entry || !id || IsZero(id->File))
        {
            xg::Log(xg::MessageType::Warning, "[Resources] Acquire: invalid type %u or ID", type);
            return 0;
        }

        const ResourceID resourceId = *id;
        const Key key{ type, resourceId };
        auto found = _lookup.find(key);
        if (found != _lookup.end())
        {
            Slot& slot = _slots[found->second];
            AddReference(slot, moduleId);
            return MakeHandle(found->second, slot.Generation);
        }

        const ResourceTypeDesc desc = entry->Desc;
        const uint32_t index = AllocateSlot();
        Slot& slot = _slots[index];
        slot.Used = true;
        slot.Type = type;
        slot.Id = resourceId;
        slot.State = ResourceState::Pending;
        AddReference(slot, moduleId);
        _lookup.emplace(key, index);
        const ResourceHandle handle = MakeHandle(index, slot.Generation);

        void* data = nullptr;
        const bool loaded = desc.Load(&resourceId, desc.User, &data);

        Slot* current = Resolve(handle);
        if (!current)
        {
            if (loaded && desc.Unload)
                desc.Unload(data, desc.User);
            return 0;
        }
        current->State = loaded ? ResourceState::Ready : ResourceState::Failed;
        current->Data = loaded ? data : nullptr;
        if (!loaded)
            xg::Log(xg::MessageType::Warning, "[Resources] failed to load %s", IDText(resourceId).Text);
        return handle;
    }

    bool ResourceSystemImpl::AddRef(uint32_t moduleId, ResourceHandle handle)
    {
        CheckThread();
        Slot* slot = Resolve(handle);
        if (!slot)
        {
            xg::Log(xg::MessageType::Warning, "[Resources] AddRef: stale handle %llx", static_cast<unsigned long long>(handle));
            return false;
        }
        AddReference(*slot, moduleId);
        return true;
    }

    bool ResourceSystemImpl::Release(uint32_t moduleId, ResourceHandle handle)
    {
        CheckThread();
        Slot* slot = Resolve(handle);
        if (!slot)
        {
            xg::Log(xg::MessageType::Warning, "[Resources] Release: stale handle %llx", static_cast<unsigned long long>(handle));
            return false;
        }
        for (size_t i = 0; i < slot->References.size(); ++i)
        {
            ModuleReference& reference = slot->References[i];
            if (reference.Module != moduleId)
                continue;
            if (--reference.Count == 0)
                slot->References.erase(slot->References.begin() + static_cast<std::ptrdiff_t>(i));
            if (--slot->TotalReferences == 0)
                _unloadQueue.push_back(handle);
            return true;
        }
        xg::Log(xg::MessageType::Warning, "[Resources] Release: module %u holds no reference to %s", moduleId, IDText(slot->Id).Text);
        return false;
    }

    ResourceState ResourceSystemImpl::GetState(ResourceHandle handle) const
    {
        const Slot* slot = Resolve(handle);
        return slot ? slot->State : ResourceState::Invalid;
    }

    void* ResourceSystemImpl::GetData(ResourceHandle handle) const
    {
        const Slot* slot = Resolve(handle);
        return slot && slot->State == ResourceState::Ready ? slot->Data : nullptr;
    }

    void ResourceSystemImpl::ReleaseModule(uint32_t moduleId)
    {
        CheckThread();
        uint32_t dropped = 0;
        for (uint32_t index = 0; index < static_cast<uint32_t>(_slots.size()); ++index)
        {
            Slot& slot = _slots[index];
            if (!slot.Used)
                continue;
            for (size_t i = 0; i < slot.References.size(); ++i)
            {
                if (slot.References[i].Module != moduleId)
                    continue;
                const uint32_t count = slot.References[i].Count;
                slot.References.erase(slot.References.begin() + static_cast<std::ptrdiff_t>(i));
                dropped += count;
                slot.TotalReferences -= count;
                if (slot.TotalReferences == 0)
                    _unloadQueue.push_back(MakeHandle(index, slot.Generation));
                break;
            }
        }
        if (dropped != 0)
            xg::Log(xg::MessageType::Info, "[Resources] module %u released with %u reference(s) still held", moduleId, dropped);

        for (size_t i = 0; i < _types.size(); ++i)
        {
            if (_types[i].Active && _types[i].Owner == moduleId)
                UnregisterType(moduleId, static_cast<ResourceTypeID>(i + 1));
        }
    }

    void ResourceSystemImpl::Collect()
    {
        CheckThread();
        while (!_unloadQueue.empty())
        {
            std::vector<ResourceHandle> queue;
            queue.swap(_unloadQueue);
            for (ResourceHandle handle : queue)
            {
                const Slot* slot = Resolve(handle);
                if (slot && slot->TotalReferences == 0)
                    DestroySlot(static_cast<uint32_t>(handle) - 1);
            }
        }
    }

    void ResourceSystemImpl::DestroySlot(uint32_t index)
    {
        Slot& slot = _slots[index];
        const ResourceTypeDesc desc = _types[slot.Type - 1].Desc;
        const bool unload = slot.State == ResourceState::Ready && desc.Unload;
        void* data = slot.Data;

        _lookup.erase(Key{ slot.Type, slot.Id });
        slot.Used = false;
        slot.Type = 0;
        slot.Id = {};
        slot.State = ResourceState::Invalid;
        slot.Data = nullptr;
        slot.TotalReferences = 0;
        slot.References.clear();
        if (++slot.Generation == 0)
            slot.Generation = 1;
        _freeSlots.push_back(index);

        if (unload)
            desc.Unload(data, desc.User);
    }
}
