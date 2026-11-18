// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>
#include "xgResources.h"

namespace xg
{
    // Binary container for generated .xr* and .xd* files. All values are little-endian.
    //   file:  ChunkFileHeader, then chunks
    //   chunk: ChunkHeader, Size payload bytes, then padding to the next 8-byte boundary
    //   item:  a chunk tagged ITEM whose payload is a 16-byte GUID (Hi, Lo) followed by typed data chunks
    constexpr uint32_t MakeChunkTag(char a, char b, char c, char d)
    {
        return static_cast<uint32_t>(static_cast<uint8_t>(a))
            | (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8)
            | (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16)
            | (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
    }

    inline constexpr uint32_t ChunkFileMagic = MakeChunkTag('X', 'G', 'C', 'F');
    inline constexpr uint32_t ChunkFileVersion = 1;
    inline constexpr uint32_t ChunkTagItem = MakeChunkTag('I', 'T', 'E', 'M');

    struct ChunkFileHeader
    {
        uint32_t Magic;
        uint32_t Version;
        uint32_t Kind;
        uint32_t Flags;
        ResourceGUID File;
    };
    static_assert(sizeof(ChunkFileHeader) == 32, "ChunkFileHeader is a file format");

    struct ChunkHeader
    {
        uint32_t Type;
        uint32_t Flags;
        uint64_t Size;
        uint64_t Extra;
    };
    static_assert(sizeof(ChunkHeader) == 24, "ChunkHeader is a file format");

    enum class ChunkFileStatus : int32_t
    {
        Valid = 0,
        TooSmall = 1,
        BadMagic = 2,
        UnsupportedVersion = 3,
        Truncated = 4,
        BadItem = 5,
    };

    // A chunk's header values and the position of its payload in the file.
    struct ChunkView
    {
        uint32_t Type;
        uint32_t Flags;
        uint64_t Extra;
        uint64_t Offset;
        uint64_t Size;
    };

    namespace chunkfile
    {
        inline bool ReadChunk(const uint8_t* data, uint64_t end, uint64_t& cursor, ChunkView& out)
        {
            if (cursor > end || end - cursor < sizeof(ChunkHeader))
                return false;
            ChunkHeader header;
            std::memcpy(&header, data + cursor, sizeof(header));
            const uint64_t payload = cursor + sizeof(ChunkHeader);
            const uint64_t available = end - payload;
            if (header.Size > available)
                return false;
            const uint64_t padding = (8 - (header.Size % 8)) % 8;
            if (padding > available - header.Size)
                return false;
            out = { header.Type, header.Flags, header.Extra, payload, header.Size };
            cursor = payload + header.Size + padding;
            return true;
        }

        inline bool ReadItemGUID(const uint8_t* data, const ChunkView& item, ResourceGUID& out)
        {
            if (item.Size < sizeof(ResourceGUID))
                return false;
            std::memcpy(&out, data + item.Offset, sizeof(out));
            return !IsZero(out);
        }
    }

    inline ChunkFileStatus ValidateChunkFile(const void* bytes, uint64_t size, ChunkFileHeader* outHeader = nullptr)
    {
        if (!bytes || size < sizeof(ChunkFileHeader))
            return ChunkFileStatus::TooSmall;
        const uint8_t* data = static_cast<const uint8_t*>(bytes);
        ChunkFileHeader header;
        std::memcpy(&header, data, sizeof(header));
        if (header.Magic != ChunkFileMagic)
            return ChunkFileStatus::BadMagic;
        if (header.Version == 0 || header.Version > ChunkFileVersion)
            return ChunkFileStatus::UnsupportedVersion;

        std::vector<std::pair<uint64_t, uint64_t>> items;
        uint64_t cursor = sizeof(ChunkFileHeader);
        while (cursor < size)
        {
            ChunkView chunk;
            if (!chunkfile::ReadChunk(data, size, cursor, chunk))
                return ChunkFileStatus::Truncated;
            if (chunk.Type != ChunkTagItem)
                continue;
            ResourceGUID guid;
            if (!chunkfile::ReadItemGUID(data, chunk, guid))
                return ChunkFileStatus::BadItem;
            items.emplace_back(guid.Hi, guid.Lo);
            uint64_t inner = chunk.Offset + sizeof(ResourceGUID);
            const uint64_t innerEnd = chunk.Offset + chunk.Size;
            while (inner < innerEnd)
            {
                ChunkView child;
                if (!chunkfile::ReadChunk(data, innerEnd, inner, child))
                    return ChunkFileStatus::Truncated;
                if (child.Type == ChunkTagItem)
                    return ChunkFileStatus::BadItem;
            }
        }
        std::sort(items.begin(), items.end());
        if (std::adjacent_find(items.begin(), items.end()) != items.end())
            return ChunkFileStatus::BadItem;
        if (outHeader)
            *outHeader = header;
        return ChunkFileStatus::Valid;
    }

    // Finds the first chunk of a type inside an item, or among the file-level chunks when item is null or zero.
    // Call ValidateChunkFile first; malformed data returns false.
    inline bool FindChunk(const void* bytes, uint64_t size, const ResourceGUID* item, uint32_t type, ChunkView* out)
    {
        if (!bytes || !out || size < sizeof(ChunkFileHeader))
            return false;
        const uint8_t* data = static_cast<const uint8_t*>(bytes);
        const bool fileLevel = !item || IsZero(*item);
        uint64_t cursor = sizeof(ChunkFileHeader);
        while (cursor < size)
        {
            ChunkView chunk;
            if (!chunkfile::ReadChunk(data, size, cursor, chunk))
                return false;
            if (fileLevel)
            {
                if (chunk.Type == type)
                {
                    *out = chunk;
                    return true;
                }
                continue;
            }
            ResourceGUID guid;
            if (chunk.Type != ChunkTagItem || !chunkfile::ReadItemGUID(data, chunk, guid) || guid.Hi != item->Hi || guid.Lo != item->Lo)
                continue;
            uint64_t inner = chunk.Offset + sizeof(ResourceGUID);
            const uint64_t innerEnd = chunk.Offset + chunk.Size;
            while (inner < innerEnd)
            {
                ChunkView child;
                if (!chunkfile::ReadChunk(data, innerEnd, inner, child))
                    return false;
                if (child.Type == type)
                {
                    *out = child;
                    return true;
                }
            }
            return false;
        }
        return false;
    }
}
