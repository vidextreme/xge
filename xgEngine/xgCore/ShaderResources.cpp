// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ShaderResources.h"
#include <cstring>
#include <vector>
#include "xgChunkFile.h"
#include "xgLog.h"

namespace xg
{
    namespace
    {
        constexpr uint32_t TagHeader = MakeChunkTag('S', 'H', 'H', 'D');
        constexpr uint32_t TagBytecode = MakeChunkTag('S', 'H', 'B', 'C');
        constexpr uint32_t TagParams = MakeChunkTag('S', 'H', 'P', 'R');
        constexpr uint32_t TagTextures = MakeChunkTag('S', 'H', 'T', 'X');
        constexpr uint32_t LayoutVersion = 1;
        constexpr uint64_t StageVertex = 0;
        constexpr uint64_t StagePixel = 1;

        struct ShaderContext
        {
            ResourceSystem* Resources = nullptr;
            Renderer* Target = nullptr;
        };

        struct LoadedShader
        {
            std::vector<uint8_t> VS;
            std::vector<uint8_t> PS;
            ShaderResourceInfo Info = {};
        };

        ShaderContext _context;

        uint32_t ReadU32(const uint8_t* data, uint64_t offset)
        {
            uint32_t value;
            std::memcpy(&value, data + offset, sizeof(value));
            return value;
        }

        bool ReadShader(const uint8_t* data, uint64_t size, uint64_t backend, LoadedShader& shader)
        {
            uint64_t cursor = sizeof(ChunkFileHeader);
            while (cursor < size)
            {
                ChunkView item;
                if (!chunkfile::ReadChunk(data, size, cursor, item))
                    return false;
                if (item.Type != ChunkTagItem)
                    continue;
                bool header = false;
                uint64_t inner = item.Offset + sizeof(ResourceGUID);
                const uint64_t end = item.Offset + item.Size;
                while (inner < end)
                {
                    ChunkView child;
                    if (!chunkfile::ReadChunk(data, end, inner, child))
                        return false;
                    const uint8_t* payload = data + child.Offset;
                    if (child.Type == TagHeader && child.Size >= 12)
                    {
                        header = ReadU32(data, child.Offset) == LayoutVersion;
                        shader.Info.ParamCount = static_cast<int32_t>(ReadU32(data, child.Offset + 4));
                        shader.Info.TextureCount = static_cast<int32_t>(ReadU32(data, child.Offset + 8));
                    }
                    else if (child.Type == TagBytecode && (child.Extra >> 8) == backend)
                    {
                        std::vector<uint8_t>& target = (child.Extra & 0xFF) == StageVertex ? shader.VS : shader.PS;
                        if ((child.Extra & 0xFF) == StageVertex || (child.Extra & 0xFF) == StagePixel)
                            target.assign(payload, payload + child.Size);
                    }
                    else if (child.Type == TagParams)
                    {
                        const uint64_t count = (std::min)(child.Size / sizeof(ShaderParamInfo), static_cast<uint64_t>(MaterialParamSlots));
                        std::memcpy(shader.Info.Params, payload, count * sizeof(ShaderParamInfo));
                    }
                    else if (child.Type == TagTextures)
                    {
                        const uint64_t count = (std::min)(child.Size / sizeof(ShaderTextureInfo), static_cast<uint64_t>(MaterialTextureSlots));
                        std::memcpy(shader.Info.Textures, payload, count * sizeof(ShaderTextureInfo));
                    }
                }
                shader.Info.ParamCount = (std::min)((std::max)(shader.Info.ParamCount, 0), MaterialParamSlots);
                shader.Info.TextureCount = (std::min)((std::max)(shader.Info.TextureCount, 0), MaterialTextureSlots);
                for (ShaderParamInfo& param : shader.Info.Params)
                    param.Name[31] = '\0';
                for (ShaderTextureInfo& texture : shader.Info.Textures)
                    texture.Name[31] = '\0';
                return header && !shader.VS.empty() && !shader.PS.empty();
            }
            return false;
        }

        bool LoadShader(const ResourceID* id, void* user, void** outData)
        {
            auto* context = static_cast<ShaderContext*>(user);
            char text[ResourceIDTextCapacity] = {};
            FormatResourceID(id, text, ResourceIDTextCapacity);
            const ShaderFormat format = context->Target->GetShaderFormat();
            if (format == ShaderFormat::None)
                return false;
            ResourceBlob blob = {};
            if (!context->Resources->ReadFile(&id->File, "xds", &blob))
            {
                xg::Log(xg::MessageType::Warning, "[Shaders] %s: no .xds in the resource sources", text);
                return false;
            }
            auto* shader = new LoadedShader();
            const auto* data = static_cast<const uint8_t*>(blob.Data);
            const bool valid = ValidateChunkFile(data, blob.Size) == ChunkFileStatus::Valid
                && ReadShader(data, blob.Size, static_cast<uint64_t>(format), *shader);
            context->Resources->FreeBlob(&blob);
            if (!valid)
            {
                xg::Log(xg::MessageType::Warning, "[Shaders] %s: not a valid shader for this renderer", text);
                delete shader;
                return false;
            }
            *outData = shader;
            return true;
        }

        bool FinalizeShader(void* data, void* user, void** outData)
        {
            auto* context = static_cast<ShaderContext*>(user);
            auto* shader = static_cast<LoadedShader*>(data);
            const ShaderDesc desc{ shader->VS.data(), shader->VS.size(), shader->PS.data(), shader->PS.size() };
            shader->Info.ShaderId = context->Target->CreateShader(&desc);
            ShaderResourceInfo* info = nullptr;
            if (shader->Info.ShaderId != 0)
                info = new ShaderResourceInfo(shader->Info);
            delete shader;
            *outData = info;
            return info != nullptr;
        }

        void UnloadShader(void* data, void* user)
        {
            auto* context = static_cast<ShaderContext*>(user);
            auto* info = static_cast<ShaderResourceInfo*>(data);
            if (context->Target && info->ShaderId != 0)
                context->Target->DestroyShader(info->ShaderId);
            delete info;
        }
    }

    ResourceTypeID RegisterShaderResources(ResourceSystem& resources, uint32_t moduleId, Renderer* renderer)
    {
        if (!renderer)
            return 0;
        _context.Resources = &resources;
        _context.Target = renderer;
        const ResourceTypeDesc desc{ ShaderResourceType, &LoadShader, &UnloadShader, &_context, &FinalizeShader, ResourceTypeFlags::Async };
        return resources.RegisterType(moduleId, &desc);
    }
}
