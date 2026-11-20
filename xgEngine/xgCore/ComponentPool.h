// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgComponentData.h"

namespace xg
{
    // Dense, swap-and-pop pool of fixed-stride elements keyed by ActorID, with a reference count and change version per
    // element. Listeners hear about adds, removals and moves so systems can keep parallel columns.
    class ComponentPool
    {
    public:
        ComponentPool(uint32_t size, uint32_t align);
        ~ComponentPool();

        ComponentPool(const ComponentPool&) = delete;
        ComponentPool& operator=(const ComponentPool&) = delete;

        int32_t Find(ActorID actor) const;
        // Returns the actor's slot, creating a zero-filled element (version stamped) when it has none.
        uint32_t Add(ActorID actor, uint64_t version, bool* created);
        void RemoveAt(uint32_t slot);
        void Clear();

        inline uint32_t Count() const { return static_cast<uint32_t>(_owners.size()); }
        inline uint32_t Size() const { return _size; }
        inline uint32_t Align() const { return _align; }
        inline uint32_t Stride() const { return _stride; }
        inline void* Data(uint32_t slot) { return _data + static_cast<size_t>(slot) * _stride; }
        inline void* Dense() { return _data; }
        inline const ActorID* Owners() const { return _owners.data(); }
        inline ActorID Owner(uint32_t slot) const { return _owners[slot]; }
        inline uint32_t& RefCount(uint32_t slot) { return _refCounts[slot]; }
        inline uint64_t Version(uint32_t slot) const { return _versions[slot]; }
        inline void Stamp(uint32_t slot, uint64_t version) { _versions[slot] = version; }
        void StampAll(uint64_t version);

        void AddListener(const DataPoolListener& listener);
        void RemoveListener(const DataPoolListener& listener);

    private:
        void Reserve(uint32_t capacity);

        uint32_t _size = 0;
        uint32_t _align = 0;
        uint32_t _stride = 0;
        uint32_t _capacity = 0;
        uint8_t* _data = nullptr;
        std::vector<ActorID> _owners;
        std::vector<uint32_t> _refCounts;
        std::vector<uint64_t> _versions;
        std::unordered_map<ActorID, uint32_t> _slots;
        std::vector<DataPoolListener> _listeners;
    };
}
