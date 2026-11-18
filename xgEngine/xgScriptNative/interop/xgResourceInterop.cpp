// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgLog.h"
#include "xgResources.h"
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
