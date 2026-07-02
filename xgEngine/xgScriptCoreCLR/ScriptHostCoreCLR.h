#pragma once
#include "xgScriptHost.h"

namespace xg
{
    class ScriptEngine;
    class ScriptHostCoreCLR : public ScriptHost
    {
    public:
        XG_IMPL_REFCOUNTED()
        ScriptHostCoreCLR(ScriptEngine* engine);
        ~ScriptHostCoreCLR() override;

        ScriptModule* LoadModule(const char* id, const char* path, const char* group) override;

        // Multi‑module: resolve entry points for a specific assembly/type
        bool GetEntryPoints(
            const char* assemblyName,
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
        
        virtual bool Decode(const ScriptMessage& message,
            const TypeSchema* schema,
            void* outObject) override;
    private:
        bool InitializeRuntime(const char* engineRoot);
        void ShutdownRuntime();

    private:
        ScriptEngine* _engine = nullptr;
        void* _coreclrLib = nullptr;
        void* _hostHandle = nullptr;
        unsigned int _domainId = 0;
        bool _initialized = false;

        using coreclr_initialize_fn = int(*)(const char* exePath,
            const char* appDomainFriendlyName,
            int propertyCount,
            const char** propertyKeys,
            const char** propertyValues,
            void** hostHandle,
            unsigned int* domainId);

        using coreclr_create_delegate_fn = int(*)(void* hostHandle,
            unsigned int domainId,
            const char* assemblyName,
            const char* typeName,
            const char* methodName,
            void** delegate);

        using coreclr_shutdown_fn = int(*)(void* hostHandle,
            unsigned int domainId);

        coreclr_initialize_fn      _coreclrInitialize = nullptr;
        coreclr_create_delegate_fn _coreclrCreateDelegate = nullptr;
        coreclr_shutdown_fn        _coreclrShutdown = nullptr;
    };
}
