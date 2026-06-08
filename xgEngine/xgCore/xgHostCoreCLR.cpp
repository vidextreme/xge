#include "pch.h"
#include "public/xgScriptCoreCLR.h"
#include "public/xgHostCoreCLR.h"
#include "public/xgModules.h"

#define XG_STRINGIFY2(x) #x
#define XG_STRINGIFY(x) XG_STRINGIFY2(x)

XG_DEFINE_MODULE_LOADER(xg, ScriptCoreCLR);
XG_IMPLEMENT_MODULE(xg, ScriptCoreCLR)

namespace xg
{
    xgHostCoreCLR::xgHostCoreCLR()
    {
        InitializeRuntime();
    }

    xgHostCoreCLR::~xgHostCoreCLR()
    {
        ShutdownRuntime();
    }

    ScriptModule* xgHostCoreCLR::LoadModule(const char* path)
    {
        if (!_initialized)
            return nullptr;

        EnsureScriptCoreCLRLoaded();

        // Bind CreateScriptModule(host, path)
        XG_MODULE_PROCEDURE(xg, ScriptCoreCLR, CreateScriptModule);
        return XG_MODULE_CALL(CreateScriptModule)(this, path);
    }

    bool xgHostCoreCLR::InitializeRuntime()
    {
        _initialized = true;
        return true;
    }

    void xgHostCoreCLR::ShutdownRuntime()
    {
        _initialized = false;
    }
}

#pragma message("Compiling xgHostCoreCLR.cpp with XG_API = " XG_STRINGIFY(XG_API))

XG_DECLARE_MODULE(xg, ScriptCoreCLR)