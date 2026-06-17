#pragma once

#include "xgScriptModule.h"

namespace xg
{
    //
    // ScriptEngine
    //
    // Backend-agnostic interface implemented by the Engine.
    // ScriptHosts call into this to load/unload ScriptModules.
    //
    class ScriptEngine
    {
    public:
        virtual ~ScriptEngine() = default;

        virtual ScriptHost* AddScriptModule(const char* id,
            const char* path,
            ScriptHost* hostOverride = nullptr) = 0;

        virtual ScriptModule* GetScriptModule(const char* id) = 0;
        virtual void RemoveScriptModule(const char* id) = 0;
    };
}
