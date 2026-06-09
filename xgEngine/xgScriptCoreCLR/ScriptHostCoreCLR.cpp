#include "pch.h"
#include "ScriptHostCoreCLR.h"
#include "public/xgScriptModuleCoreCLR.h"
#include "xgModules.h"


namespace xg
{
    ScriptHostCoreCLR::ScriptHostCoreCLR()
    {
        InitializeRuntime();
    }

    ScriptHostCoreCLR::~ScriptHostCoreCLR()
    {
        ShutdownRuntime();
    }

    ScriptModule* ScriptHostCoreCLR::LoadModule(const char* path)
    {
        if (!_initialized)
            return nullptr;

        auto* module = new xgScriptModuleCoreCLR(this);
        if (!module->Load(path))
        {
            delete module;
            return nullptr;
        }
        return module;
    }

    bool ScriptHostCoreCLR::InitializeRuntime()
    {
        _initialized = true;
        return true;
    }

    void ScriptHostCoreCLR::ShutdownRuntime()
    {
        _initialized = false;
    }
}

#pragma message("Compiling xgHostCoreCLR.cpp with XG_API = " XG_STRINGIFY(XG_API))