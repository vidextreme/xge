#pragma once

#include "xgScriptModule.h"

namespace xg
{
	XG_ENUM(inherit = byte)
    enum class ScriptBackendType : uint8_t
    {
        CoreCLR,
        Native,
		//Squirrel, //very very soon!
        //Lua,
        //Python,
        //JavaScript,
	};

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
        XG_ABSTRACT_REFCOUNTED();
        virtual ~ScriptHost() = default;

        // Load a script module from the given path and return a ScriptModule instance.
        // The engine will store and later destroy the returned ScriptModule.
        virtual ScriptModule* LoadModule(const char* id,
            const char* path,
            const char* group) = 0;
    };

    XG_DECLARE_MODULE_FUNCTION(CreateScriptHostCoreCLR, ScriptHost*, const char*);
    XG_API ScriptHost* CreateScriptHostCoreCLR(const char* path);


    XG_DECLARE_MODULE_FUNCTION(CreateScriptHostNative, ScriptHost*, const char*);
    XG_API ScriptHost* CreateScriptHostNative(const char* path);
}

XG_DECLARE_MODULE(xg, ScriptCoreCLR)
XG_DECLARE_MODULE(xg, ScriptNative)