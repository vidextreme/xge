// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ComponentPool.h"
#include <cstring>
#include <new>

namespace xg
{
    ComponentPool::ComponentPool(uint32_t size, uint32_t align)
        : _size(size)
        , _align(align ? align : 1)
        , _stride(DataStride(size, align ? align : 1))
    {}

    ComponentPool::~ComponentPool()
    {
        if (_data)
            ::operator delete(_data, std::align_val_t(_align));
    }

    int32_t ComponentPool::Find(ActorID actor) const
    {
        auto it = _slots.find(actor);
        return it != _slots.end() ? static_cast<int32_t>(it->second) : -1;
    }

    uint32_t ComponentPool::Add(ActorID actor, uint64_t version, bool* created)
    {
        auto it = _slots.find(actor);
        if (it != _slots.end())
        {
            if (created)
                *created = false;
            return it->second;
        }
        const uint32_t slot = Count();
        if (slot == _capacity)
            Reserve(_capacity ? _capacity * 2 : 16);
        std::memset(Data(slot), 0, _stride);
        _owners.push_back(actor);
        _refCounts.push_back(0);
        _versions.push_back(version);
        _slots.emplace(actor, slot);
        if (created)
            *created = true;
        const std::vector<DataPoolListener> listeners = _listeners;
        for (const DataPoolListener& l : listeners)
        {
            if (l.Added)
                l.Added(l.User, actor, slot);
        }
        return slot;
    }

    void ComponentPool::RemoveAt(uint32_t slot)
    {
        if (slot >= Count())
            return;
        const ActorID actor = _owners[slot];
        const std::vector<DataPoolListener> listeners = _listeners;
        for (const DataPoolListener& l : listeners)
        {
            if (l.Removed)
                l.Removed(l.User, actor, slot);
        }
        const uint32_t last = Count() - 1;
        _slots.erase(actor);
        if (slot != last)
        {
            std::memcpy(Data(slot), Data(last), _stride);
            const ActorID moved = _owners[last];
            _owners[slot] = moved;
            _refCounts[slot] = _refCounts[last];
            _versions[slot] = _versions[last];
            _slots[moved] = slot;
            _owners.pop_back();
            _refCounts.pop_back();
            _versions.pop_back();
            for (const DataPoolListener& l : listeners)
            {
                if (l.Moved)
                    l.Moved(l.User, moved, last, slot);
            }
            return;
        }
        _owners.pop_back();
        _refCounts.pop_back();
        _versions.pop_back();
    }

    void ComponentPool::Clear()
    {
        while (Count() > 0)
            RemoveAt(Count() - 1);
    }

    void ComponentPool::StampAll(uint64_t version)
    {
        std::fill(_versions.begin(), _versions.end(), version);
    }

    void ComponentPool::AddListener(const DataPoolListener& listener)
    {
        RemoveListener(listener);
        _listeners.push_back(listener);
    }

    void ComponentPool::RemoveListener(const DataPoolListener& listener)
    {
        std::erase_if(_listeners, [&](const DataPoolListener& l)
            {
                return l.Added == listener.Added && l.Removed == listener.Removed && l.Moved == listener.Moved && l.User == listener.User;
            });
    }

    void ComponentPool::Reserve(uint32_t capacity)
    {
        if (capacity <= _capacity)
            return;
        auto* data = static_cast<uint8_t*>(::operator new(static_cast<size_t>(capacity) * _stride, std::align_val_t(_align)));
        if (_data)
        {
            std::memcpy(data, _data, static_cast<size_t>(Count()) * _stride);
            ::operator delete(_data, std::align_val_t(_align));
        }
        _data = data;
        _capacity = capacity;
    }
}
