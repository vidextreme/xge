#pragma once

#include "xgScriptModule.h"

namespace xg
{
    //
    // ScriptHost
    //
    // Backend-agnostic interface for script backends (CoreCLR, NativeAOT, Lua, etc.).
    // Implementations are responsible for:
    //  - loading the script/module at `path`
    //  - creating and owning any backend-specific runtime/context
    //  - returning a ScriptModule* that the engine will manage
    //
    class ScriptHost
    {
    public:
        virtual ~ScriptHost() = default;

        // Load a script module from the given path and return a ScriptModule instance.
        // The engine will store and later destroy the returned ScriptModule.
        virtual ScriptModule* LoadModule(const char* path) = 0;
    };
}
