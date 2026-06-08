#pragma once

#include "xgScriptModule.h"

namespace xg
{
    class xgHostCoreCLR;

    class xgScriptModuleCoreCLR : public ScriptModule
    {
    public:
        explicit xgScriptModuleCoreCLR(xgHostCoreCLR* host);
        ~xgScriptModuleCoreCLR() override;

        bool Load(const char* path);

        bool Init(Engine* engine) override;   // ← REQUIRED
        void Update(float dt) override;
        void Shutdown() override;
        bool IsValid() const override;

    private:
        xgHostCoreCLR* _host = nullptr;

        void* _managedInit = nullptr;
        void* _managedUpdate = nullptr;
        void* _managedShutdown = nullptr;

        bool _valid = false;
    };
}
