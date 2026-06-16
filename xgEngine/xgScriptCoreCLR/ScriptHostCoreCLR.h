#pragma once
#include "xgScriptHost.h"

namespace xg
{
    class ScriptHostCoreCLR : public ScriptHost
    {
    public:
        ScriptHostCoreCLR();
        ~ScriptHostCoreCLR() override;

        ScriptModule* LoadModule(const char* id, const char* path) override;

        bool GetEntryPoints(void** initFn, void** updateFn, void** shutdownFn);

    private:
        bool InitializeRuntime(const char* engineRoot);
        void ShutdownRuntime();

    private:
        void* _coreclrLib = nullptr;
        void* _hostHandle = nullptr;
        unsigned int _domainId = 0;
        bool         _initialized = false;

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

        coreclr_initialize_fn       _coreclrInitialize = nullptr;
        coreclr_create_delegate_fn  _coreclrCreateDelegate = nullptr;
        coreclr_shutdown_fn         _coreclrShutdown = nullptr;

        void* _init = nullptr;
        void* _update = nullptr;
        void* _shutdown = nullptr;
    };
}
