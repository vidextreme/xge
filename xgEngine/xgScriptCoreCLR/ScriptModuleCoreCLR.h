#pragma once
#include "xgScriptModule.h"

namespace xg
{
    class ScriptHostCoreCLR;

    class ScriptModuleCoreCLR : public ScriptModule
    {
    public:
        ScriptModuleCoreCLR(const char* id, ScriptHostCoreCLR* host);
        ~ScriptModuleCoreCLR() override;

        bool Load(const char* path);
        bool Init(ScriptEngine* engine) override;
        void Update(float dt) override;
        void Shutdown() override;
        bool IsValid() const override;

    private:
        ScriptHostCoreCLR* _host = nullptr;

        void* _managedInit = nullptr;
        void* _managedUpdate = nullptr;
        void* _managedShutdown = nullptr;

        bool _valid = false;
    };
}
