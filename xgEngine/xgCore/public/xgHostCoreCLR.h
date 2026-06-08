#pragma once

#include "xgScriptHost.h"

namespace xg
{
    class xgHostCoreCLR : public ScriptHost
    {
    public:
        xgHostCoreCLR();
        ~xgHostCoreCLR() override;

        ScriptModule* LoadModule(const char* path) override;

    private:
        bool InitializeRuntime();
        void ShutdownRuntime();

    private:
        void* _hostfxr = nullptr;
        void* _runtime = nullptr;
        bool  _initialized = false;
    };

    XG_DECLARE_MODULE_FUNCTION(CreateScriptModule, ScriptModule*, xgHostCoreCLR*, const char*);

}
