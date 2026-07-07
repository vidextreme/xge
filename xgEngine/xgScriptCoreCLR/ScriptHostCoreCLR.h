#pragma once
#include "xgScriptHost.h"
#include "xgModules.h"

#include "hostfxr.h"
#include "coreclr_delegates.h"

namespace xg
{
    class ScriptEngine;
    class CodecRegistry;
    struct TypeSchema;
    struct ScriptMessage;

    using hostfxr_handle = void*;

    class ScriptHostCoreCLR : public ScriptHost
    {
    public:
        XG_IMPL_REFCOUNTED()

            explicit ScriptHostCoreCLR(ScriptEngine* engine);
        ~ScriptHostCoreCLR() override;

        ScriptModule* LoadModule(const char* id,
            const char* path,
            const char* group) override;

        bool GetEntryPoints(const char* assemblyName,
            const char* typeName,
            void** initFn,
            void** updateFn,
            void** shutdownFn);

        ScriptEngine* GetEngine() const override;
        CodecRegistry* GetCodecRegistry() const override;
        PayloadMode GetPayloadMode() const override;

        bool Encode(const void* object,
            const TypeSchema* schema,
            ScriptMessage& outMessage) override;

        bool Decode(const ScriptMessage& message,
            const TypeSchema* schema,
            void* outObject) override;

    private:
        bool InitializeRuntime(const char* engineRoot);
        void ShutdownRuntime();

    private:
        ScriptEngine* _engine = nullptr;

        // hostfxr.dll module handle
        ModuleHandle _hostfxrLib = nullptr;

        // hostfxr runtime context
        hostfxr_handle _fxrHandle = nullptr;

        // load_assembly_and_get_function_pointer delegate (stdcall, char_t*)
        load_assembly_and_get_function_pointer_fn _loadAssemblyAndGetFn = nullptr;

        // hostfxr function pointers
        hostfxr_initialize_for_runtime_config_fn _hostfxrInitializeForRuntimeConfig = nullptr;
        hostfxr_get_runtime_delegate_fn          _hostfxrGetRuntimeDelegate = nullptr;
        hostfxr_close_fn                         _hostfxrClose = nullptr;

        bool _initialized = false;
    };
}
