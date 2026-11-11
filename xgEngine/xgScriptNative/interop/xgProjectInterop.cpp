// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgScriptEngine.h"
#include "xgEngineDef.h"

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

XG_MODULE_EXPORT const char* XG_GetNewProjectPath(xg::ScriptEngine* engine)
{
    const xg::EngineConfig* config = engine ? engine->GetEngineConfig() : nullptr;
    return config && config->NewProjectPath ? config->NewProjectPath : "";
}
