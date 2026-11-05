// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgModules.h"
#include "xgScriptEngine.h"
#include "ScriptHostCoreCLR.h"
#include "ScriptModuleCoreCLR.h"

XG_MODULE_EXPORT xg::ScriptHost* CreateScriptHostCoreCLR(xg::ScriptEngine* engine,const char* path)
{
    if (engine)
        xg::SetLogger(engine->GetLogger());
    return new xg::ScriptHostCoreCLR(engine);
}