#include "pch.h"
#include "public/xgScriptModuleCoreCLR.h"
#include "xgModules.h"

namespace xg
{
    xgScriptModuleCoreCLR::xgScriptModuleCoreCLR(ScriptHostCoreCLR* host)
        : ScriptModule("coreclr"), _host(host)
    {
    }

    xgScriptModuleCoreCLR::~xgScriptModuleCoreCLR()
    {
        Shutdown();
    }

    bool xgScriptModuleCoreCLR::Load(const char* path)
    {
        //
        // Your host currently has no API for loading assemblies or resolving
        // managed entrypoints. So for now, Load() simply marks the module valid.
        //
        // Once xgHostCoreCLR exposes:
        //   - LoadAssembly(path)
        //   - GetFunctionPointer(...)
        //   - InitializeRuntime(configPath)
        //
        // …this function will bind Script_Init / Script_Update / Script_Shutdown.
        //

        if (!path || !path[0] || !_host)
        {
            _valid = false;
            return false;
        }

        // Placeholder until host is implemented
        _managedInit = nullptr;
        _managedUpdate = nullptr;
        _managedShutdown = nullptr;

        _valid = true;
        return true;
    }

    bool xgScriptModuleCoreCLR::Init(Engine* engine)
    {
        if (_managedInit)
        {
            using InitFunc = bool(*)(Engine*);
            return ((InitFunc)_managedInit)(engine);
        }
        return false;
    }

    bool xgScriptModuleCoreCLR::IsValid() const
    {
        return _valid;
    }

    void xgScriptModuleCoreCLR::Update(float dt)
    {
        if (_managedUpdate)
        {
            using UpdateFunc = void(*)(float);
            ((UpdateFunc)_managedUpdate)(dt);
        }
    }

    void xgScriptModuleCoreCLR::Shutdown()
    {
        if (_managedShutdown)
        {
            using ShutdownFunc = void(*)();
            ((ShutdownFunc)_managedShutdown)();
        }

        _valid = false;
    }
}
