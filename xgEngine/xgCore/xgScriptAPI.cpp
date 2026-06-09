#include "pch.h"
#include "public/xgScriptModule.h"


XG_DEFINE_MODULE_LOADER(xg, ScriptCoreCLR);
XG_IMPLEMENT_MODULE(xg, ScriptCoreCLR)

namespace xg
{
    ScriptHost* CreateScriptHostCoreCLR(const char* path)
    {
        EnsureScriptCoreCLRLoaded();

        XG_MODULE_PROCEDURE(xg, ScriptCoreCLR, CreateScriptHostCoreCLR);
        return XG_MODULE_CALL(CreateScriptHostCoreCLR)(path);
    }

}

#pragma message("Compiling xgRendererAPI.cpp with XG_API = " XG_STRINGIFY(XG_API))
