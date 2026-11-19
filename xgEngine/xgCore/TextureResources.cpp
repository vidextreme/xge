// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "TextureResources.h"
#include <cstring>
#include "xgChunkFile.h"
#include "xgLog.h"

namespace xg
{
    namespace
    {
        constexpr uint32_t TagHeader = MakeChunkTag('T', 'X', 'H', 'D');
        constexpr uint32_t TagPixels = MakeChunkTag('T', 'X', 'P', 'X');
        constexpr uint32_t HeaderLayoutVersion = 2;
        constexpr uint32_t HeaderFlagSRGB = 1;

        struct TextureContext
        {
            ResourceSystem* Resources = nullptr;
            Renderer* Target = nullptr;
        };

        struct LoadedTexture
        {
            ResourceBlob Blob = {};
            TextureDesc Desc = {};
            uint32_t Usage = 0;
            std::vector<TextureMip> Mips;
        };

        TextureContext _context;

        uint32_t ReadU32(const uint8_t* data, uint64_t offset)
        {
            uint32_t value;
            std::memcpy(&value, data + offset, sizeof(value));
            return value;
        }

        uint32_t BlockBytes(TextureFormat format)
        {
            switch (format)
            {
            case TextureFormat::BC1:
            case TextureFormat::BC4:
                return 8;
            case TextureFormat::BC3:
            case TextureFormat::BC5:
                return 16;
            default:
                return 0;
            }
        }

        bool IsKnownFormat(uint64_t value)
        {
            return value >= static_cast<uint64_t>(TextureFormat::RGBA8) && value <= static_cast<uint64_t>(TextureFormat::BC5);
        }

        // Finds the item (the breadcrumb's chunk, or the first item for a whole-file ID), its TXHD and its first TXPX in
        // a supported format.
        bool FindTexture(const uint8_t* data, uint64_t size, const ResourceGUID& itemGuid, ChunkView& header, ChunkView& pixels, Renderer* renderer)
        {
            const bool anyItem = IsZero(itemGuid);
            uint64_t cursor = sizeof(ChunkFileHeader);
            while (cursor < size)
            {
                ChunkView item;
                if (!chunkfile::ReadChunk(data, size, cursor, item))
                    return false;
                ResourceGUID guid;
                if (item.Type != ChunkTagItem || !chunkfile::ReadItemGUID(data, item, guid))
                    continue;
                if (!anyItem && (guid.Hi != itemGuid.Hi || guid.Lo != itemGuid.Lo))
                    continue;
                bool hasHeader = false;
                bool hasPixels = false;
                uint64_t inner = item.Offset + sizeof(ResourceGUID);
                const uint64_t innerEnd = item.Offset + item.Size;
                while (inner < innerEnd)
                {
                    ChunkView child;
                    if (!chunkfile::ReadChunk(data, innerEnd, inner, child))
                        return false;
                    if (child.Type == TagHeader && !hasHeader)
                    {
                        header = child;
                        hasHeader = true;
                    }
                    else if (child.Type == TagPixels && !hasPixels && IsKnownFormat(child.Extra)
                        && renderer->SupportsTextureFormat(static_cast<TextureFormat>(child.Extra)))
                    {
                        pixels = child;
                        hasPixels = true;
                    }
                }
                if (!hasHeader)
                    return false;
                if (!hasPixels)
                {
                    xg::Log(xg::MessageType::Warning, "[Textures] no pixel format in the file is supported by the renderer");
                    return false;
                }
                return true;
            }
            return false;
        }

        bool SplitMips(const uint8_t* data, const ChunkView& pixels, LoadedTexture& texture)
        {
            const TextureDesc& desc = texture.Desc;
            const uint32_t block = BlockBytes(desc.Format);
            int32_t width = desc.Width;
            int32_t height = desc.Height;
            uint64_t offset = 0;
            for (int32_t mip = 0; mip < desc.MipCount; ++mip)
            {
                uint64_t rowPitch;
                uint64_t rows;
                if (block == 0)
                {
                    rowPitch = static_cast<uint64_t>(width) * 4;
                    rows = static_cast<uint64_t>(height);
                }
                else
                {
                    rowPitch = static_cast<uint64_t>((width + 3) / 4) * block;
                    rows = static_cast<uint64_t>((height + 3) / 4);
                }
                const uint64_t mipSize = rowPitch * rows;
                if (offset + mipSize > pixels.Size)
                    return false;
                texture.Mips.push_back({ data + pixels.Offset + offset, mipSize, static_cast<uint32_t>(rowPitch) });
                offset += mipSize;
                width = (std::max)(1, width / 2);
                height = (std::max)(1, height / 2);
            }
            return true;
        }

        bool LoadTexture(const ResourceID* id, void* user, void** outData)
        {
            auto* context = static_cast<TextureContext*>(user);
            auto* texture = new LoadedTexture();
            char text[ResourceIDTextCapacity] = {};
            FormatResourceID(id, text, ResourceIDTextCapacity);
            if (!context->Resources->ReadFile(&id->File, "xdt", &texture->Blob))
            {
                xg::Log(xg::MessageType::Warning, "[Textures] %s: no .xdt in the resource sources", text);
                delete texture;
                return false;
            }
            const auto* data = static_cast<const uint8_t*>(texture->Blob.Data);
            const uint64_t size = texture->Blob.Size;
            ChunkView header;
            ChunkView pixels;
            bool valid = ValidateChunkFile(data, size) == ChunkFileStatus::Valid
                && FindTexture(data, size, id->Chunk, header, pixels, context->Target)
                && header.Size >= 24;
            if (valid)
            {
                TextureDesc& desc = texture->Desc;
                desc.Width = static_cast<int32_t>(ReadU32(data, header.Offset));
                desc.Height = static_cast<int32_t>(ReadU32(data, header.Offset + 4));
                desc.MipCount = static_cast<int32_t>(ReadU32(data, header.Offset + 8));
                texture->Usage = ReadU32(data, header.Offset + 12);
                const uint32_t flags = ReadU32(data, header.Offset + 16);
                const uint32_t layout = ReadU32(data, header.Offset + 20);
                desc.Format = static_cast<TextureFormat>(pixels.Extra);
                desc.Flags = (flags & HeaderFlagSRGB) != 0 ? TextureFlags::SRGB : TextureFlags::None;
                valid = layout == HeaderLayoutVersion && desc.Width > 0 && desc.Height > 0 && desc.MipCount > 0 && desc.MipCount <= 16
                    && SplitMips(data, pixels, *texture);
            }
            if (!valid)
            {
                xg::Log(xg::MessageType::Warning, "[Textures] %s: not a valid texture", text);
                context->Resources->FreeBlob(&texture->Blob);
                delete texture;
                return false;
            }
            *outData = texture;
            return true;
        }

        bool FinalizeTexture(void* data, void* user, void** outData)
        {
            auto* context = static_cast<TextureContext*>(user);
            auto* texture = static_cast<LoadedTexture*>(data);
            const uint64_t id = context->Target->CreateTexture(&texture->Desc, texture->Mips.data());
            TextureResourceInfo* info = nullptr;
            if (id != 0)
            {
                info = new TextureResourceInfo{ id, texture->Desc.Width, texture->Desc.Height, texture->Desc.MipCount,
                    texture->Desc.Format, texture->Usage, texture->Desc.Flags };
            }
            context->Resources->FreeBlob(&texture->Blob);
            delete texture;
            *outData = info;
            return info != nullptr;
        }

        void UnloadTexture(void* data, void* user)
        {
            auto* context = static_cast<TextureContext*>(user);
            auto* info = static_cast<TextureResourceInfo*>(data);
            if (context->Target && info->TextureId != 0)
                context->Target->DestroyTexture(info->TextureId);
            delete info;
        }
    }

    ResourceTypeID RegisterTextureResources(ResourceSystem& resources, uint32_t moduleId, Renderer* renderer)
    {
        if (!renderer)
            return 0;
        _context.Resources = &resources;
        _context.Target = renderer;
        const ResourceTypeDesc desc{ TextureResourceType, &LoadTexture, &UnloadTexture, &_context, &FinalizeTexture, ResourceTypeFlags::Async };
        return resources.RegisterType(moduleId, &desc);
    }
}
