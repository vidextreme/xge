// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ObjectResources.h"
#include <cstring>
#include "xgChunkFile.h"
#include "xgLog.h"

namespace xg
{
    namespace
    {
        constexpr uint32_t TagHeader = MakeChunkTag('O', 'B', 'H', 'D');
        constexpr uint32_t TagNodes = MakeChunkTag('O', 'B', 'N', 'D');
        constexpr uint32_t TagParts = MakeChunkTag('O', 'B', 'M', 'S');
        constexpr uint32_t TagNames = MakeChunkTag('O', 'B', 'N', 'M');
        constexpr uint32_t TagComponents = MakeChunkTag('O', 'B', 'C', 'P');
        constexpr uint32_t LayoutVersion = 2;
        constexpr uint64_t NodeRecordSize = 184;
        constexpr uint64_t PartRecordSize = 56;

        struct ObjectContext
        {
            ResourceSystem* Resources = nullptr;
        };

        ObjectContext _context;

        uint32_t ReadU32(const uint8_t* data, uint64_t offset)
        {
            uint32_t value;
            std::memcpy(&value, data + offset, sizeof(value));
            return value;
        }

        ResourceGUID ReadGuid(const uint8_t* data, uint64_t offset)
        {
            ResourceGUID guid;
            std::memcpy(&guid, data + offset, sizeof(guid));
            return guid;
        }

        bool ReadObject(const uint8_t* data, uint64_t size, ObjectResourceData& object)
        {
            uint64_t cursor = sizeof(ChunkFileHeader);
            while (cursor < size)
            {
                ChunkView item;
                if (!chunkfile::ReadChunk(data, size, cursor, item))
                    return false;
                if (item.Type != ChunkTagItem)
                    continue;
                ChunkView header = {};
                ChunkView nodes = {};
                ChunkView parts = {};
                ChunkView names = {};
                ChunkView components = {};
                uint64_t inner = item.Offset + sizeof(ResourceGUID);
                const uint64_t end = item.Offset + item.Size;
                while (inner < end)
                {
                    ChunkView child;
                    if (!chunkfile::ReadChunk(data, end, inner, child))
                        return false;
                    if (child.Type == TagHeader)
                        header = child;
                    else if (child.Type == TagNodes)
                        nodes = child;
                    else if (child.Type == TagParts)
                        parts = child;
                    else if (child.Type == TagNames)
                        names = child;
                    else if (child.Type == TagComponents)
                        components = child;
                }
                if (header.Size < 16 || ReadU32(data, header.Offset) != LayoutVersion)
                    return false;
                const uint32_t nodeCount = ReadU32(data, header.Offset + 4);
                const uint32_t partCount = ReadU32(data, header.Offset + 8);
                if (nodes.Size < nodeCount * NodeRecordSize || parts.Size < partCount * PartRecordSize)
                    return false;
                object.Nodes.resize(nodeCount);
                for (uint32_t i = 0; i < nodeCount; ++i)
                {
                    const uint64_t at = nodes.Offset + i * NodeRecordSize;
                    ObjectNode& node = object.Nodes[i];
                    node.Id = ReadGuid(data, at);
                    node.Parent = static_cast<int32_t>(ReadU32(data, at + 16));
                    node.Kind = static_cast<ObjectNodeKind>(ReadU32(data, at + 20));
                    node.Expose = static_cast<ObjectNodeExpose>(ReadU32(data, at + 24));
                    const uint32_t nameOffset = ReadU32(data, at + 28);
                    node.NameHash = ReadU32(data, at + 32);
                    std::memcpy(node.Baked, data + at + 80, sizeof(node.Baked));
                    node.ActorParent = static_cast<int32_t>(ReadU32(data, at + 36));
                    std::memcpy(&node.ActorPose, data + at + 144, sizeof(TransformData));
                    if (node.ActorParent >= static_cast<int32_t>(i))
                        return false;
                    if (node.Parent >= static_cast<int32_t>(i))
                        return false;
                    if (nameOffset < names.Size)
                    {
                        const char* text = reinterpret_cast<const char*>(data + names.Offset + nameOffset);
                        node.Name.assign(text, strnlen(text, static_cast<size_t>(names.Size - nameOffset)));
                    }
                }
                object.Parts.resize(partCount);
                for (uint32_t i = 0; i < partCount; ++i)
                {
                    const uint64_t at = parts.Offset + i * PartRecordSize;
                    ObjectPart& part = object.Parts[i];
                    part.Node = static_cast<int32_t>(ReadU32(data, at));
                    part.Model = ReadGuid(data, at + 8);
                    part.Submesh = ReadGuid(data, at + 24);
                    part.Material = ReadGuid(data, at + 40);
                    if (part.Node < 0 || part.Node >= static_cast<int32_t>(nodeCount))
                        return false;
                }
                uint64_t cursor = 0;
                while (cursor + 8 <= components.Size)
                {
                    const uint32_t node = ReadU32(data, components.Offset + cursor);
                    const uint32_t length = ReadU32(data, components.Offset + cursor + 4);
                    if (node >= nodeCount || cursor + 8 + length > components.Size)
                        return false;
                    object.Nodes[node].Components.assign(reinterpret_cast<const char*>(data + components.Offset + cursor + 8), length);
                    cursor += 8 + ((static_cast<uint64_t>(length) + 3) & ~3ull);
                }
                return true;
            }
            return false;
        }

        bool LoadObject(const ResourceID* id, void* user, void** outData)
        {
            auto* context = static_cast<ObjectContext*>(user);
            char text[ResourceIDTextCapacity] = {};
            FormatResourceID(id, text, ResourceIDTextCapacity);
            ResourceBlob blob = {};
            auto* object = new ObjectResourceData();
            bool valid = context->Resources->ReadFile(&id->File, "xdo", &blob);
            if (valid)
            {
                const auto* data = static_cast<const uint8_t*>(blob.Data);
                valid = ValidateChunkFile(data, blob.Size) == ChunkFileStatus::Valid && ReadObject(data, blob.Size, *object);
            }
            context->Resources->FreeBlob(&blob);
            if (!valid)
            {
                xg::Log(xg::MessageType::Warning, "[Objects] %s: no valid .xdo in the resource sources", text);
                delete object;
                return false;
            }
            *outData = object;
            return true;
        }

        void UnloadObject(void* data, void*)
        {
            delete static_cast<ObjectResourceData*>(data);
        }
    }

    ResourceTypeID RegisterObjectResources(ResourceSystem& resources, uint32_t moduleId)
    {
        _context.Resources = &resources;
        const ResourceTypeDesc desc{ ObjectResourceType, &LoadObject, &UnloadObject, &_context, nullptr, ResourceTypeFlags::Async };
        return resources.RegisterType(moduleId, &desc);
    }

    uint32_t HashObjectName(const char* name)
    {
        uint32_t hash = 2166136261u;
        for (const char* c = name; c && *c; ++c)
        {
            hash ^= static_cast<uint8_t>(*c);
            hash *= 16777619u;
        }
        return hash;
    }
}
