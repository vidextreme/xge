// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "public/xgScriptHost.h"
#include "xgLog.h"

XG_DEFINE_MODULE_LOADER(xg, ScriptCoreCLR);
XG_IMPLEMENT_MODULE(xg, ScriptCoreCLR)

XG_DEFINE_MODULE_LOADER(xg, ScriptNative);
XG_IMPLEMENT_MODULE(xg, ScriptNative)

namespace xg
{
    ScriptHost* CreateScriptHostCoreCLR(ScriptEngine* engine, const char* path)
    {
        EnsureScriptCoreCLRLoaded();

        XG_MODULE_PROCEDURE(xg, ScriptCoreCLR, CreateScriptHostCoreCLR);
        return XG_MODULE_CALL(CreateScriptHostCoreCLR)(engine, path);
    }

    ScriptHost* CreateScriptHostNative(ScriptEngine* engine, const char* path)
    {
        EnsureScriptNativeLoaded();

        XG_MODULE_PROCEDURE(xg, ScriptNative, CreateScriptHostNative);
        return XG_MODULE_CALL(CreateScriptHostNative)(engine, path);
    }
}

#pragma message("Compiling xgRendererAPI.cpp with XG_API = " XG_STRINGIFY(XG_API))
