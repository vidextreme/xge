// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgChunkFile.h"
#include "xgLog.h"
#include "xgRenderer.h"
#include "xgResources.h"
#include "xgMeshRenderer.h"
#include <cstring>
#include "xgScriptEngine.h"

namespace
{
    xg::ResourceSystem* ResourcesOf(xg::ScriptEngine* engine)
    {
        return engine ? engine->GetSubsystem<xg::ResourceSystem>() : nullptr;
    }

    bool LoadTestResource(const xg::ResourceID* id, void*, void** outData)
    {
        char text[xg::ResourceIDTextCapacity] = {};
        xg::FormatResourceID(id, text, xg::ResourceIDTextCapacity);
        if (id->File.Hi == UINT64_MAX)
            return false;
        xg::Log(xg::MessageType::Info, "[Resources] test load %s", text);
        *outData = new xg::ResourceID(*id);
        return true;
    }

    void UnloadTestResource(void* data, void*)
    {
        auto* id = static_cast<xg::ResourceID*>(data);
        char text[xg::ResourceIDTextCapacity] = {};
        xg::FormatResourceID(id, text, xg::ResourceIDTextCapacity);
        xg::Log(xg::MessageType::Info, "[Resources] test unload %s", text);
        delete id;
    }
}

XG_MODULE_EXPORT bool XG_ResourceParseID(const char* text, xg::ResourceID* outId)
{
    return xg::ParseResourceID(text, outId);
}

XG_MODULE_EXPORT int32_t XG_ResourceFormatID(const xg::ResourceID* id, char* buffer, int32_t capacity)
{
    return xg::FormatResourceID(id, buffer, capacity);
}

XG_MODULE_EXPORT int32_t XG_ChunkFileValidate(const void* data, uint64_t size)
{
    return static_cast<int32_t>(xg::ValidateChunkFile(data, size));
}

XG_MODULE_EXPORT bool XG_ChunkFileFind(const void* data, uint64_t size, const xg::ResourceGUID* item, uint32_t type, uint64_t* outOffset, uint64_t* outSize)
{
    xg::ChunkView chunk;
    if (!outOffset || !outSize || !xg::FindChunk(data, size, item, type, &chunk))
        return false;
    *outOffset = chunk.Offset;
    *outSize = chunk.Size;
    return true;
}

XG_MODULE_EXPORT uint32_t XG_ResourceFindType(xg::ScriptEngine* engine, const char* name)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    return resources ? resources->FindType(name) : 0;
}

XG_MODULE_EXPORT uint64_t XG_ResourceAcquire(xg::ScriptEngine* engine, uint32_t moduleId, uint32_t type, const xg::ResourceID* id)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    return resources ? resources->Acquire(moduleId, type, id) : 0;
}

XG_MODULE_EXPORT bool XG_ResourceAddRef(xg::ScriptEngine* engine, uint32_t moduleId, uint64_t handle)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    return resources && resources->AddRef(moduleId, handle);
}

XG_MODULE_EXPORT bool XG_ResourceRelease(xg::ScriptEngine* engine, uint32_t moduleId, uint64_t handle)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    return resources && resources->Release(moduleId, handle);
}

XG_MODULE_EXPORT int32_t XG_ResourceGetState(xg::ScriptEngine* engine, uint64_t handle)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    return static_cast<int32_t>(resources ? resources->GetState(handle) : xg::ResourceState::Invalid);
}

XG_MODULE_EXPORT int32_t XG_ResourceWait(xg::ScriptEngine* engine, uint64_t handle)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    return static_cast<int32_t>(resources ? resources->Wait(handle) : xg::ResourceState::Invalid);
}

XG_MODULE_EXPORT uint32_t XG_ResourceInvalidate(xg::ScriptEngine* engine, const char* fileGuid)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    xg::ResourceGUID file = {};
    if (!resources || !fileGuid || !xg::ParseResourceGUID(fileGuid, std::strlen(fileGuid), &file))
        return 0;
    return resources->Invalidate(&file);
}

// Draws the mesh render system handed to the renderer last frame, and how many had their material resolved.
XG_MODULE_EXPORT int32_t XG_MeshRenderStats(xg::ScriptEngine* engine, int32_t* outMaterialDraws)
{
    xg::MeshRenderSystem* system = engine ? engine->GetSubsystem<xg::MeshRenderSystem>() : nullptr;
    if (outMaterialDraws)
        *outMaterialDraws = system ? system->GetMaterialDrawCount() : 0;
    return system ? system->GetDrawCount() : 0;
}

// The renderer shader of a Ready "shader" resource (0 otherwise).
XG_MODULE_EXPORT uint64_t XG_ResourceGetShader(xg::ScriptEngine* engine, uint64_t handle)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    if (!resources || resources->FindType(xg::ShaderResourceType) == 0)
        return 0;
    const auto* info = static_cast<const xg::ShaderResourceInfo*>(resources->GetData(handle));
    return info ? info->ShaderId : 0;
}

XG_MODULE_EXPORT bool XG_ResourceIsStale(xg::ScriptEngine* engine, uint64_t handle)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    return !resources || resources->IsStale(handle);
}

XG_MODULE_EXPORT uint32_t XG_ResourceAddDirectorySource(xg::ScriptEngine* engine, uint32_t moduleId, const char* root)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    return resources ? resources->AddDirectorySource(moduleId, root) : 0;
}

XG_MODULE_EXPORT bool XG_ResourceRemoveSource(xg::ScriptEngine* engine, uint32_t moduleId, uint32_t source)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    return resources && resources->RemoveSource(moduleId, source);
}

// Copies the TextureResourceInfo of a Ready "texture" resource. False for other types or states.
XG_MODULE_EXPORT bool XG_ResourceGetTexture(xg::ScriptEngine* engine, uint64_t handle, xg::TextureResourceInfo* outInfo)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    if (!resources || !outInfo)
        return false;
    const xg::ResourceTypeID type = resources->FindType(xg::TextureResourceType);
    const auto* info = static_cast<const xg::TextureResourceInfo*>(resources->GetData(handle));
    if (type == 0 || !info)
        return false;
    *outInfo = *info;
    return true;
}

// Copies the summary of a Ready "model" resource. False for other types or states.
XG_MODULE_EXPORT bool XG_ResourceGetModel(xg::ScriptEngine* engine, uint64_t handle, xg::ModelResourceInfo* outInfo)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    if (!resources || !outInfo || resources->FindType(xg::ModelResourceType) == 0)
        return false;
    const auto* info = static_cast<const xg::ModelResourceInfo*>(resources->GetData(handle));
    if (!info)
        return false;
    *outInfo = *info;
    outInfo->Parts = nullptr;
    return true;
}

XG_MODULE_EXPORT bool XG_ResourceGetModelPart(xg::ScriptEngine* engine, uint64_t handle, int32_t index, xg::ModelPartInfo* outPart)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    if (!resources || !outPart || resources->FindType(xg::ModelResourceType) == 0)
        return false;
    const auto* info = static_cast<const xg::ModelResourceInfo*>(resources->GetData(handle));
    if (!info || index < 0 || index >= info->PartCount || !info->Parts)
        return false;
    *outPart = info->Parts[index];
    return true;
}

// Test hook: an in-memory type that logs its loads and unloads. IDs whose file GUID starts with 16 'f' digits fail to load.
XG_MODULE_EXPORT uint32_t XG_ResourceRegisterTestType(xg::ScriptEngine* engine, uint32_t moduleId, const char* name)
{
    xg::ResourceSystem* resources = ResourcesOf(engine);
    if (!resources)
        return 0;
    xg::SetLogger(engine->GetLogger());
    const xg::ResourceTypeDesc desc{ name, &LoadTestResource, &UnloadTestResource, nullptr };
    return resources->RegisterType(moduleId, &desc);
}
