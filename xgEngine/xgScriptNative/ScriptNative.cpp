// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgModules.h"
#include "xgScriptEngine.h"
#include "ScriptHostNative.h"

XG_MODULE_EXPORT xg::ScriptHost* CreateScriptHostNative(xg::ScriptEngine* engine, const char* path)
{
    if (engine)
        xg::SetLogger(engine->GetLogger());
    return new xg::ScriptHostNative(engine);
}