#pragma once
#include "xgScriptHost.h"

namespace xg
{
    class ScriptHostCoreCLR :
        public ScriptHost
    {
    public:
        ScriptHostCoreCLR();
        ~ScriptHostCoreCLR() override;

        ScriptModule* LoadModule(const char* path) override;

    private:
        bool InitializeRuntime();
        void ShutdownRuntime();

    private:
        void* _hostfxr = nullptr;
        void* _runtime = nullptr;
        bool  _initialized = false;
    };

}