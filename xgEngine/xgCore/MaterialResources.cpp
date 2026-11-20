// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "MaterialResources.h"
#include <cstring>
#include <vector>
#include "xgChunkFile.h"
#include "xgLog.h"

namespace xg
{
    namespace
    {
        constexpr uint32_t TagHeader = MakeChunkTag('M', 'T', 'H', 'D');
        constexpr uint32_t TagParams = MakeChunkTag('M', 'T', 'P', 'R');
        constexpr uint32_t TagTextures = MakeChunkTag('M', 'T', 'T', 'X');
        constexpr uint32_t LayoutVersion = 1;
        constexpr uint64_t ParamRecordSize = 48;
        constexpr uint64_t TextureRecordSize = 64;

        struct MaterialContext
        {
            ResourceSystem* Resources = nullptr;
            Renderer* Target = nullptr;
            uint32_t Module = 0;
            ResourceTypeID ShaderType = 0;
            ResourceTypeID TextureType = 0;
        };

        struct ParamValue
        {
            char Name[32] = {};
            float Value[4] = {};
        };

        struct TextureRef
        {
            char Name[32] = {};
            ResourceID Id = {};
        };

        // Info first: GetData returns this object as a MaterialResourceInfo.
        struct MaterialData
        {
            MaterialResourceInfo Info = {};
            ResourceGUID Shader = {};
            std::vector<ParamValue> Params;
            std::vector<TextureRef> Textures;
            ResourceHandle ShaderHandle = 0;
            ResourceHandle TextureHandles[MaterialTextureSlots] = {};
            bool TexturesRequested = false;
            bool Dirty = true;
        };

        MaterialContext _context;

        uint32_t ReadU32(const uint8_t* data, uint64_t offset)
        {
            uint32_t value;
            std::memcpy(&value, data + offset, sizeof(value));
            return value;
        }

        bool ReadMaterial(const uint8_t* data, uint64_t size, MaterialData& material)
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
                uint32_t paramCount = 0;
                uint32_t textureCount = 0;
                ChunkView params = {};
                ChunkView textures = {};
                uint64_t inner = item.Offset + sizeof(ResourceGUID);
                const uint64_t end = item.Offset + item.Size;
                while (inner < end)
                {
                    ChunkView child;
                    if (!chunkfile::ReadChunk(data, end, inner, child))
                        return false;
                    if (child.Type == TagHeader && child.Size >= 28)
                    {
                        header = ReadU32(data, child.Offset) == LayoutVersion;
                        paramCount = ReadU32(data, child.Offset + 4);
                        textureCount = ReadU32(data, child.Offset + 8);
                        std::memcpy(&material.Shader, data + child.Offset + 12, sizeof(ResourceGUID));
                    }
                    else if (child.Type == TagParams)
                    {
                        params = child;
                    }
                    else if (child.Type == TagTextures)
                    {
                        textures = child;
                    }
                }
                if (!header || params.Size < paramCount * ParamRecordSize || textures.Size < textureCount * TextureRecordSize)
                    return false;
                for (uint32_t i = 0; i < paramCount; ++i)
                {
                    ParamValue value;
                    std::memcpy(value.Name, data + params.Offset + i * ParamRecordSize, 32);
                    std::memcpy(value.Value, data + params.Offset + i * ParamRecordSize + 32, sizeof(value.Value));
                    value.Name[31] = '\0';
                    material.Params.push_back(value);
                }
                for (uint32_t i = 0; i < textureCount; ++i)
                {
                    TextureRef texture;
                    std::memcpy(texture.Name, data + textures.Offset + i * TextureRecordSize, 32);
                    std::memcpy(&texture.Id, data + textures.Offset + i * TextureRecordSize + 32, sizeof(ResourceID));
                    texture.Name[31] = '\0';
                    material.Textures.push_back(texture);
                }
                material.Info.ShaderHi = material.Shader.Hi;
                material.Info.ShaderLo = material.Shader.Lo;
                material.Info.ParamCount = static_cast<int32_t>(paramCount);
                material.Info.TextureCount = static_cast<int32_t>(textureCount);
                return true;
            }
            return false;
        }

        bool LoadMaterial(const ResourceID* id, void* user, void** outData)
        {
            auto* context = static_cast<MaterialContext*>(user);
            char text[ResourceIDTextCapacity] = {};
            FormatResourceID(id, text, ResourceIDTextCapacity);
            ResourceBlob blob = {};
            if (!context->Resources->ReadFile(&id->File, "xdm", &blob))
            {
                xg::Log(xg::MessageType::Warning, "[Materials] %s: no .xdm in the resource sources", text);
                return false;
            }
            auto* material = new MaterialData();
            const auto* data = static_cast<const uint8_t*>(blob.Data);
            const bool valid = ValidateChunkFile(data, blob.Size) == ChunkFileStatus::Valid && ReadMaterial(data, blob.Size, *material);
            context->Resources->FreeBlob(&blob);
            if (!valid)
            {
                xg::Log(xg::MessageType::Warning, "[Materials] %s: not a valid material", text);
                delete material;
                return false;
            }
            *outData = material;
            return true;
        }

        bool FinalizeMaterial(void* data, void* user, void** outData)
        {
            *outData = data;
            return data != nullptr;
        }

        void ReleaseTextures(MaterialContext& context, MaterialData& material)
        {
            for (ResourceHandle& handle : material.TextureHandles)
            {
                if (handle != 0)
                    context.Resources->Release(context.Module, handle);
                handle = 0;
            }
            material.TexturesRequested = false;
        }

        void UnloadMaterial(void* data, void* user)
        {
            auto* context = static_cast<MaterialContext*>(user);
            auto* material = static_cast<MaterialData*>(data);
            ReleaseTextures(*context, *material);
            if (material->ShaderHandle != 0)
                context->Resources->Release(context->Module, material->ShaderHandle);
            if (context->Target && material->Info.MaterialId != 0)
                context->Target->DestroyMaterial(material->Info.MaterialId);
            delete material;
        }

        const ParamValue* FindParam(const MaterialData& material, const char* name)
        {
            for (const ParamValue& value : material.Params)
                if (std::strncmp(value.Name, name, 32) == 0)
                    return &value;
            return nullptr;
        }

        const TextureRef* FindTextureRef(const MaterialData& material, const char* name)
        {
            for (const TextureRef& texture : material.Textures)
                if (std::strncmp(texture.Name, name, 32) == 0)
                    return &texture;
            return nullptr;
        }
    }

    ResourceTypeID RegisterMaterialResources(ResourceSystem& resources, uint32_t moduleId, Renderer* renderer, ResourceTypeID shaderType,
        ResourceTypeID textureType)
    {
        if (!renderer || shaderType == 0)
            return 0;
        _context.Resources = &resources;
        _context.Target = renderer;
        _context.Module = moduleId;
        _context.ShaderType = shaderType;
        _context.TextureType = textureType;
        const ResourceTypeDesc desc{ MaterialResourceType, &LoadMaterial, &UnloadMaterial, &_context, &FinalizeMaterial, ResourceTypeFlags::Async };
        return resources.RegisterType(moduleId, &desc);
    }

    uint64_t ResolveMaterial(ResourceHandle handle)
    {
        MaterialContext& context = _context;
        if (!context.Resources || !context.Target)
            return 0;
        auto* material = static_cast<MaterialData*>(context.Resources->GetData(handle));
        if (!material || IsZero(material->Shader))
            return 0;

        if (material->ShaderHandle != 0 && context.Resources->IsStale(material->ShaderHandle))
        {
            context.Resources->Release(context.Module, material->ShaderHandle);
            material->ShaderHandle = 0;
            ReleaseTextures(context, *material);
        }
        if (material->ShaderHandle == 0)
        {
            const ResourceID shaderId{ material->Shader, {} };
            material->ShaderHandle = context.Resources->Acquire(context.Module, context.ShaderType, &shaderId);
            material->Dirty = true;
            if (material->ShaderHandle == 0)
                return 0;
        }
        const ResourceState shaderState = context.Resources->GetState(material->ShaderHandle);
        if (shaderState != ResourceState::Ready)
            return shaderState == ResourceState::Pending ? material->Info.MaterialId : 0;
        const auto* shader = static_cast<const ShaderResourceInfo*>(context.Resources->GetData(material->ShaderHandle));

        if (!material->TexturesRequested)
        {
            for (int32_t i = 0; i < shader->TextureCount; ++i)
            {
                const ShaderTextureInfo& slot = shader->Textures[i];
                const TextureRef* texture = FindTextureRef(*material, slot.Name);
                if (texture && context.TextureType != 0 && slot.Slot >= 0 && slot.Slot < MaterialTextureSlots && !IsZero(texture->Id.File))
                    material->TextureHandles[slot.Slot] = context.Resources->Acquire(context.Module, context.TextureType, &texture->Id);
            }
            material->TexturesRequested = true;
            material->Dirty = true;
        }
        for (int32_t i = 0; i < shader->TextureCount; ++i)
        {
            const ShaderTextureInfo& slot = shader->Textures[i];
            if (slot.Slot < 0 || slot.Slot >= MaterialTextureSlots)
                continue;
            ResourceHandle& texture = material->TextureHandles[slot.Slot];
            if (texture == 0)
                continue;
            if (context.Resources->IsStale(texture))
            {
                const TextureRef* ref = FindTextureRef(*material, slot.Name);
                context.Resources->Release(context.Module, texture);
                texture = ref ? context.Resources->Acquire(context.Module, context.TextureType, &ref->Id) : 0;
                material->Dirty = true;
            }
            if (texture != 0 && context.Resources->GetState(texture) == ResourceState::Pending)
                return material->Info.MaterialId;
        }
        if (!material->Dirty && material->Info.MaterialId != 0)
            return material->Info.MaterialId;

        MaterialDesc desc = {};
        desc.Shader = shader->ShaderId;
        for (int32_t i = 0; i < shader->ParamCount; ++i)
        {
            const ShaderParamInfo& param = shader->Params[i];
            if (param.Slot < 0 || param.Slot >= MaterialParamSlots)
                continue;
            const ParamValue* value = FindParam(*material, param.Name);
            std::memcpy(&desc.Params[param.Slot * 4], value ? value->Value : param.Default, sizeof(float) * 4);
        }
        for (int32_t i = 0; i < MaterialTextureSlots; ++i)
        {
            const ResourceHandle texture = material->TextureHandles[i];
            const auto* info = texture != 0 ? static_cast<const TextureResourceInfo*>(context.Resources->GetData(texture)) : nullptr;
            desc.Textures[i] = info ? info->TextureId : 0;
        }
        if (material->Info.MaterialId != 0)
        {
            if (!context.Target->UpdateMaterial(material->Info.MaterialId, &desc))
                material->Info.MaterialId = 0;
        }
        if (material->Info.MaterialId == 0)
            material->Info.MaterialId = context.Target->CreateMaterial(&desc);
        material->Dirty = false;
        return material->Info.MaterialId;
    }
}
