// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgComponentData.h"
#include "xgScriptEngine.h"

namespace
{
    xg::ComponentDataRegistry* RegistryOf(xg::ScriptEngine* engine)
    {
        return engine ? engine->GetSubsystem<xg::ComponentDataRegistry>() : nullptr;
    }
}

XG_MODULE_EXPORT uint32_t XG_ComponentDataRegisterType(xg::ScriptEngine* engine, uint32_t moduleId, const char* key, uint32_t size, uint32_t align,
    uint64_t layoutHash, uint32_t flags, const char* layout)
{
    xg::ComponentDataRegistry* registry = RegistryOf(engine);
    if (!registry)
        return 0;
    const xg::DataTypeDesc desc = { key, size, align, layoutHash, static_cast<xg::DataTypeFlags>(flags) };
    return registry->RegisterTypeWithLayout(moduleId, &desc, layout);
}

XG_MODULE_EXPORT uint32_t XG_ComponentDataFindType(xg::ScriptEngine* engine, const char* key)
{
    xg::ComponentDataRegistry* registry = RegistryOf(engine);
    return registry ? registry->FindType(key) : 0;
}

// Returns false for unknown types. key points at the registry's copy.
XG_MODULE_EXPORT bool XG_ComponentDataGetTypeInfo(xg::ScriptEngine* engine, uint32_t type, const char** key, uint32_t* size, uint32_t* align, uint32_t* flags)
{
    xg::ComponentDataRegistry* registry = RegistryOf(engine);
    xg::DataTypeDesc desc = {};
    if (!registry || !registry->GetTypeDesc(type, &desc))
        return false;
    if (key)
        *key = desc.Key;
    if (size)
        *size = desc.Size;
    if (align)
        *align = desc.Align;
    if (flags)
        *flags = static_cast<uint32_t>(desc.Flags);
    return true;
}

XG_MODULE_EXPORT int XG_ComponentDataGetTypes(xg::ScriptEngine* engine, uint32_t* out, int capacity)
{
    xg::ComponentDataRegistry* registry = RegistryOf(engine);
    return registry ? registry->CopyTypes(out, capacity) : 0;
}

XG_MODULE_EXPORT void* XG_ComponentDataAdd(xg::ScriptEngine* engine, uint32_t type, xg::ActorID actor)
{
    xg::ComponentDataRegistry* registry = RegistryOf(engine);
    return registry ? registry->Add(type, actor) : nullptr;
}

XG_MODULE_EXPORT bool XG_ComponentDataRelease(xg::ScriptEngine* engine, uint32_t type, xg::ActorID actor)
{
    xg::ComponentDataRegistry* registry = RegistryOf(engine);
    return registry && registry->Release(type, actor);
}

XG_MODULE_EXPORT bool XG_ComponentDataRemove(xg::ScriptEngine* engine, uint32_t type, xg::ActorID actor)
{
    xg::ComponentDataRegistry* registry = RegistryOf(engine);
    return registry && registry->Remove(type, actor);
}

XG_MODULE_EXPORT void* XG_ComponentDataGet(xg::ScriptEngine* engine, uint32_t type, xg::ActorID actor)
{
    xg::ComponentDataRegistry* registry = RegistryOf(engine);
    return registry ? registry->Get(type, actor) : nullptr;
}

XG_MODULE_EXPORT int XG_ComponentDataGetSlot(xg::ScriptEngine* engine, uint32_t type, xg::ActorID actor)
{
    xg::ComponentDataRegistry* registry = RegistryOf(engine);
    return registry ? registry->GetSlot(type, actor) : -1;
}

XG_MODULE_EXPORT void* XG_ComponentDataGetDense(xg::ScriptEngine* engine, uint32_t type, const xg::ActorID** owners, uint32_t* count)
{
    xg::ComponentDataRegistry* registry = RegistryOf(engine);
    if (!registry)
    {
        if (owners)
            *owners = nullptr;
        if (count)
            *count = 0;
        return nullptr;
    }
    return registry->GetDense(type, owners, count);
}

XG_MODULE_EXPORT void XG_ComponentDataMarkChanged(xg::ScriptEngine* engine, uint32_t type, uint32_t slot)
{
    if (xg::ComponentDataRegistry* registry = RegistryOf(engine))
        registry->MarkChanged(type, slot);
}

XG_MODULE_EXPORT void XG_ComponentDataMarkAllChanged(xg::ScriptEngine* engine, uint32_t type)
{
    if (xg::ComponentDataRegistry* registry = RegistryOf(engine))
        registry->MarkAllChanged(type);
}

XG_MODULE_EXPORT int XG_ComponentDataJoin(xg::ScriptEngine* engine, uint32_t type, uint32_t other, int* outSlots, int capacity)
{
    xg::ComponentDataRegistry* registry = RegistryOf(engine);
    return registry ? registry->Join(type, other, outSlots, capacity) : 0;
}

XG_MODULE_EXPORT bool XG_ComponentDataClaimUpdater(xg::ScriptEngine* engine, uint32_t type, uint32_t moduleId)
{
    xg::ComponentDataRegistry* registry = RegistryOf(engine);
    return registry && registry->ClaimUpdater(type, moduleId);
}

XG_MODULE_EXPORT void XG_ComponentDataReleaseUpdater(xg::ScriptEngine* engine, uint32_t type, uint32_t moduleId)
{
    if (xg::ComponentDataRegistry* registry = RegistryOf(engine))
        registry->ReleaseUpdater(type, moduleId);
}
