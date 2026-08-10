// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgSceneNode.h"

namespace xg
{
    //
    // StridePool
    //
    // Fixed-stride, dense, swap-and-pop memory pool.
    // Used internally by CompositeSceneGraph, ArchetypeSceneGraph,
    // and DynamicComponentSceneGraph.
    //
    class StridePool
    {
    public:
        explicit StridePool(std::size_t stride);

        // Add a new actor's data block.
        int Add(ActorID actorId);
        int AddCopy(ActorID actorId, const void* src);

        // Remove an actor at index using swap-and-pop.
        void RemoveAt(int index);

        // Get pointer to the block at index.
        void* GetPtr(int index);

        // Lookup index by ActorID (linear for now).
        int Find(ActorID actorId) const;

        inline std::size_t GetStride() const { return _stride; }
        inline std::size_t GetCount() const { return _actorIds.size(); }

    private:
        std::vector<std::uint8_t> _buffer;       // raw bytes
        std::vector<std::uint64_t> _actorIds;    // parallel array
        std::size_t _stride = 0;
    };

} // namespace xg
