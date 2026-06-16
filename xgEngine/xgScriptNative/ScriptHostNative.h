#pragma once

#include "xgScriptHost.h"
#include "xgScriptModule.h"
#include "xgModules.h"

namespace xg
{
    //
    // ScriptHostNative
    //
    // ScriptHost implementation for native (C++/DLL) script modules.
    // Convention:
    //   Each script DLL must export:
    //
    //       extern "C" __declspec(dllexport)
    //       ScriptModule* CreateScriptModule();
    //
    class ScriptHostNative : public ScriptHost
    {
    public:
        ScriptHostNative() = default;
        ~ScriptHostNative() override = default;

        // Load a native script module from the given path.
        ScriptModule* LoadModule(const char* path) override;
    };
}
