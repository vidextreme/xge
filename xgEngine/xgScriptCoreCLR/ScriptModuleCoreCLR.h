#pragma once
#include "xgScriptModule.h"
#include "ScriptHostCoreCLR.h"
namespace xg
{
    class ScriptModuleCoreCLR : public ScriptModule
    {
    public:
        ScriptModuleCoreCLR(const char* id,
            ScriptHostCoreCLR* host,
            const char* group);

        ~ScriptModuleCoreCLR() override;

        bool Load(const char* path);
        bool Init(ScriptEngine* engine) override;
        void Update(float dt) override;
        void Shutdown() override;
        bool IsValid() const override;

        void OnMessage(const ScriptMessage& msg) override;
    private:
        ScriptHostCoreCLR* _coreclrHost = nullptr;   // typed host

        void* _managedInit = nullptr;
        void* _managedUpdate = nullptr;
        void* _managedShutdown = nullptr;

        bool _valid = false;
    };
}
