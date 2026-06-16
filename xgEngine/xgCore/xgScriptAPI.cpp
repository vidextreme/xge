#include "pch.h"
#include "public/xgScriptModule.h"


XG_DEFINE_MODULE_LOADER(xg, ScriptCoreCLR);
XG_IMPLEMENT_MODULE(xg, ScriptCoreCLR)

XG_DEFINE_MODULE_LOADER(xg, ScriptNative);
XG_IMPLEMENT_MODULE(xg, ScriptNative)

namespace xg
{
    ScriptHost* CreateScriptHostCoreCLR(const char* path)
    {
        EnsureScriptCoreCLRLoaded();

        XG_MODULE_PROCEDURE(xg, ScriptCoreCLR, CreateScriptHostCoreCLR);
        return XG_MODULE_CALL(CreateScriptHostCoreCLR)(path);
    }

    ScriptHost* CreateScriptHostNative(const char* path)
    {
        EnsureScriptNativeLoaded();

        XG_MODULE_PROCEDURE(xg, ScriptNative, CreateScriptHostNative);
        return XG_MODULE_CALL(CreateScriptHostNative)(path);
    }
}

#pragma message("Compiling xgRendererAPI.cpp with XG_API = " XG_STRINGIFY(XG_API))
