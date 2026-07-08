#pragma once
#include "xgScriptHost.h"
#include "xgModules.h"

namespace xg
{
    class ScriptEngine;
    class CodecRegistry;
    struct TypeSchema;
    struct ScriptMessage;

    using InitializeRuntimeConfigFunc = hostfxr_initialize_for_runtime_config_fn;
	using RuntimeDelegateFunc = hostfxr_get_runtime_delegate_fn;
	using CloseFunc = hostfxr_close_fn;
    class ScriptHostCoreCLR : public ScriptHost
    {
    public:
        XG_IMPL_REFCOUNTED()

            explicit ScriptHostCoreCLR(ScriptEngine* engine);
        ~ScriptHostCoreCLR() override;

        ScriptModule* LoadModule(const char* id,
            const char* path,
            const char* group) override;

        ScriptEngine* GetEngine() const override;
        CodecRegistry* GetCodecRegistry() const override;
        PayloadMode GetPayloadMode() const override;

        bool Encode(const void* object,
            const TypeSchema* schema,
            ScriptMessage& outMessage) override;

        bool Decode(const ScriptMessage& message,
            const TypeSchema* schema,
            void* outObject) override;

		InitializeRuntimeConfigFunc GetInitializeRuntimeConfigFunc() const { return _hostfxrInitializeForRuntimeConfig; }
		RuntimeDelegateFunc GetRuntimeDelegateFunc() const { return _hostfxrGetRuntimeDelegate; }
		CloseFunc GetCloseFunc() const { return _hostfxrClose; }
    private:
        bool InitializeRuntime(const char* engineRoot);
        void ShutdownRuntime();

    private:
        ScriptEngine* _engine = nullptr;

        // hostfxr.dll module handle
        ModuleHandle _hostfxrLib = nullptr;

        // hostfxr function pointers
        InitializeRuntimeConfigFunc _hostfxrInitializeForRuntimeConfig = nullptr;
        RuntimeDelegateFunc          _hostfxrGetRuntimeDelegate = nullptr;
        CloseFunc                         _hostfxrClose = nullptr;

        bool _initialized = false;
    };

    static std::wstring ToWide(const std::string& s);
    
}
