#include "pch.h"
#include "ScriptModuleCoreCLR.h"
#include "ScriptHostCoreCLR.h"

namespace xg
{
    ScriptModuleCoreCLR::ScriptModuleCoreCLR(const char* id, ScriptHostCoreCLR* host)
        : ScriptModule(id), _host(host)
    {
    }

    ScriptModuleCoreCLR::~ScriptModuleCoreCLR()
    {
        Shutdown();
    }

    bool ScriptModuleCoreCLR::Load(const char* /*path*/)
    {
        if (!_host)
            return false;

        if (!_host->GetEntryPoints(&_managedInit, &_managedUpdate, &_managedShutdown))
            return false;

        _valid = true;
        return true;
    }

    bool ScriptModuleCoreCLR::Init(ScriptEngine* engine)
    {
        if (_managedInit)
        {
            using InitFn = int(*)(ScriptEngine*);
            return ((InitFn)_managedInit)(engine) != 0;
        }
        return false;
    }

    void ScriptModuleCoreCLR::Update(float dt)
    {
        if (_managedUpdate)
        {
            using UpdateFn = void(*)(float);
            ((UpdateFn)_managedUpdate)(dt);
        }
    }

    void ScriptModuleCoreCLR::Shutdown()
    {
        if (_managedShutdown)
        {
            using ShutdownFn = void(*)();
            ((ShutdownFn)_managedShutdown)();
        }

        _valid = false;
    }

    bool ScriptModuleCoreCLR::IsValid() const
    {
        return _valid;
    }
}
