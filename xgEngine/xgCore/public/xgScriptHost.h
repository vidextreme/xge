#pragma once

#include "xgScriptModule.h"
#include "xgScriptMessage.h"
namespace xg
{
	class ScriptEngine;
	class CodecRegistry;
    class ScriptModule;

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
            uint32_t moduleID,
            const char* path,
            const char* group) = 0;

		virtual ScriptEngine* GetEngine() const = 0;

        //
        // Encode a native struct into a ScriptMessage payload.
        // Host decides which codec to use based on engine payload mode.
        //
        virtual bool Encode(const void* object,
            const TypeSchema* schema,
            ScriptMessage& outMessage) = 0;

        //
        // Decode a ScriptMessage payload into a native struct.
        //
        virtual bool Decode(const ScriptMessage& message,
            const TypeSchema* schema,
            void* outObject) = 0;

        //
        // Host-specific codec registry (JSON/BINARY).
        //
        virtual CodecRegistry* GetCodecRegistry() const = 0;
		virtual PayloadMode GetPayloadMode() const = 0;
    };

    XG_DECLARE_MODULE_FUNCTION(CreateScriptHostCoreCLR, ScriptHost*, ScriptEngine*, const char*);
    XG_API ScriptHost* CreateScriptHostCoreCLR(ScriptEngine* engine, const char* path);


    XG_DECLARE_MODULE_FUNCTION(CreateScriptHostNative, ScriptHost*, ScriptEngine*, const char*);
    XG_API ScriptHost* CreateScriptHostNative(ScriptEngine* engine, const char* path);
}

XG_DECLARE_MODULE(xg, ScriptCoreCLR)
XG_DECLARE_MODULE(xg, ScriptNative)