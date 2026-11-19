// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgScriptEngine.h"
#include "xgEngineDef.h"
#include "xgScriptModule.h"
#include "xgVersion.h"
#include "../ScriptModuleFunctions.h"

XG_MODULE_EXPORT const char* XG_GetEngineVersion()
{
    return XGE_VERSION;
}

XG_MODULE_EXPORT const char* XG_GetProjectPath(xg::ScriptEngine* engine)
{
    const xg::EngineConfig* config = engine ? engine->GetEngineConfig() : nullptr;
    return config && config->ProjectPath ? config->ProjectPath : "";
}

XG_MODULE_EXPORT bool XG_GetProjectAutoBuild(xg::ScriptEngine* engine)
{
    const xg::EngineConfig* config = engine ? engine->GetEngineConfig() : nullptr;
    return config ? config->ProjectAutoBuild : true;
}

// Startup splash progress (see ScriptEngine::ReportStartup); a no-op once the splash has closed. Main thread only.
XG_MODULE_EXPORT void XG_StartupReport(xg::ScriptEngine* engine, const char* step, const char* detail, float progress)
{
    if (engine)
        engine->ReportStartup(step, detail, progress);
}

XG_MODULE_EXPORT void XG_StartupPump(xg::ScriptEngine* engine)
{
    if (engine)
        engine->PumpStartup();
}

// Loads a script module at runtime (not during a frame update). parentId null = the caller's parent;
// group null = the parent's group. Returns false when the parent is unknown or the module fails to load.
XG_MODULE_EXPORT bool XG_AddScriptModule(xg::ScriptEngine* engine, uint32_t callerModuleId, const char* id, const char* path,
    const char* parentId, const char* group)
{
    if (!engine || !id || !*id || !path || !*path)
        return false;
    xg::ScriptModule* parent = nullptr;
    if (parentId && *parentId)
    {
        parent = engine->GetScriptModule(parentId);
        if (!parent)
            return false;
    }
    else if (xg::ScriptModule* caller = engine->GetScriptModule(callerModuleId))
    {
        parent = engine->GetParentScriptModule(caller);
    }
    const char* moduleGroup = (group && *group) ? group : (parent ? parent->GetGroup() : nullptr);
    return engine->AddScriptModule(id, path, parent, moduleGroup) != nullptr;
}

XG_MODULE_EXPORT bool XG_RemoveScriptModule(xg::ScriptEngine* engine, const char* id)
{
    if (!engine || !id || !engine->GetScriptModule(id))
        return false;
    engine->RemoveScriptModule(id);
    return true;
}

// Adds a module whose entry points the caller loaded itself (a collectible managed load context, for example).
// parentId null = the caller's parent; group null = the parent's group. Not during a module update: call it from
// XG_RunBetweenFrames or a module's Init. Returns false when the parent is unknown, the id is taken or Init fails.
XG_MODULE_EXPORT bool XG_AddFunctionModule(xg::ScriptEngine* engine, uint32_t callerModuleId, const char* id, const char* parentId,
    const char* group, const XG_ModuleFunctions* functions)
{
    if (!engine || !id || !*id || !functions || functions->Size < sizeof(XG_ModuleFunctions))
        return false;
    xg::ScriptModule* parent = nullptr;
    if (parentId && *parentId)
    {
        parent = engine->GetScriptModule(parentId);
        if (!parent)
            return false;
    }
    else if (xg::ScriptModule* caller = engine->GetScriptModule(callerModuleId))
    {
        parent = engine->GetParentScriptModule(caller);
    }
    const char* moduleGroup = (group && *group) ? group : (parent ? parent->GetGroup() : nullptr);
    auto* module = new xg::ScriptModuleFunctions(id, engine->AllocateModuleId(), moduleGroup ? moduleGroup : "", *functions);
    return engine->AddExternalScriptModule(module, parent);
}

// Runs fn(user) on the main thread after this frame's module and actor updates (see ScriptEngine::RunBetweenFrames).
XG_MODULE_EXPORT void XG_RunBetweenFrames(xg::ScriptEngine* engine, void (*fn)(void* user), void* user)
{
    if (engine)
        engine->RunBetweenFrames(fn, user);
}

XG_MODULE_EXPORT const char* XG_GetNewProjectPath(xg::ScriptEngine* engine)
{
    const xg::EngineConfig* config = engine ? engine->GetEngineConfig() : nullptr;
    return config && config->NewProjectPath ? config->NewProjectPath : "";
}
