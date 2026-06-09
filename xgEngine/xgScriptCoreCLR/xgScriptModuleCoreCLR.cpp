#include "pch.h"
#include "public/xgScriptModuleCoreCLR.h"
#include "ScriptHostCoreCLR.h"

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

    bool xgScriptModuleCoreCLR::Load(const char* /*path*/)
    {
        if (!_host)
            return false;

        if (!_host->GetEntryPoints(&_managedInit, &_managedUpdate, &_managedShutdown))
            return false;

        _valid = true;
        return true;
    }

    bool xgScriptModuleCoreCLR::Init(Engine* engine)
    {
        if (_managedInit)
        {
            using InitFn = void(*)();
            ((InitFn)_managedInit)();
            return true;
        }
        return false;
    }

    void xgScriptModuleCoreCLR::Update(float dt)
    {
        if (_managedUpdate)
        {
            using UpdateFn = void(*)(float);
            ((UpdateFn)_managedUpdate)(dt);
        }
    }

    void xgScriptModuleCoreCLR::Shutdown()
    {
        if (_managedShutdown)
        {
            using ShutdownFn = void(*)();
            ((ShutdownFn)_managedShutdown)();
        }

        _valid = false;
    }

    bool xgScriptModuleCoreCLR::IsValid() const
    {
        return _valid;
    }
}
