#pragma once

#include "xgScriptModule.h"

namespace xg
{
    class ScriptEngine;

    //
    // ScriptModuleNative
    //
    // Wraps a native DLL that exposes:
    //
    //   extern "C" bool ScriptModule_Init(ScriptEngine* engine);
    //   extern "C" void ScriptModule_Update(float dt);
    //   extern "C" void ScriptModule_Shutdown();
    //
    class ScriptModuleNative : public ScriptModule
    {
    public:
        using InitFunc = bool (*)(ScriptEngine*);
        using UpdateFunc = void (*)(float);
        using ShutdownFunc = void (*)();
        using ScriptModuleFunc = ScriptModule * (*)(const char*);

        ScriptModuleNative(
            const char* id,
            void* lib,
            InitFunc init,
            UpdateFunc update,
            ShutdownFunc shutdown);

        ~ScriptModuleNative() override;

        bool Init(ScriptEngine* engine) override;
        void Update(float dt) override;
        void Shutdown() override;
        bool IsValid() const override;

    private:
        void* _lib = nullptr;
        InitFunc     _init = nullptr;
        UpdateFunc   _update = nullptr;
        ShutdownFunc _shutdown = nullptr;
        bool         _isValid = false;
        bool         _initialized = false;
    };
}
