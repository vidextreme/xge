#include "pch.h"
#include "public/xgScriptModuleCoreCLR.h"
#include "xgHostCoreCLR.h"
#include "xgModules.h"

namespace xg
{
    xgScriptModuleCoreCLR::xgScriptModuleCoreCLR(xgHostCoreCLR* host)
        : ScriptModule("coreclr"), _host(host)
    {
    }

    xgScriptModuleCoreCLR::~xgScriptModuleCoreCLR()
    {
        Shutdown();
    }

    bool xgScriptModuleCoreCLR::Load(const char* path)
    {
        // TODO: bind managed entrypoints
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
            ((void(*)(float))_managedUpdate)(dt);
    }

    void xgScriptModuleCoreCLR::Shutdown()
    {
        if (_managedShutdown)
            ((void(*)())_managedShutdown)();

        _valid = false;
    }
}
