#include "pch.h"
#include "ScriptModuleCoreCLR.h"
#include "ScriptHostCoreCLR.h"
#include <filesystem>

namespace xg
{
    ScriptModuleCoreCLR::ScriptModuleCoreCLR(const char* id,
        ScriptHostCoreCLR* host,
        const char* group)
        : ScriptModule(id, host, group)
        , _coreclrHost(host)
    {
    }

    ScriptModuleCoreCLR::~ScriptModuleCoreCLR()
    {
        Shutdown();
    }

    bool ScriptModuleCoreCLR::Load(const char* path)
    {
        if (!_coreclrHost)
            return false;

        // Derive assembly + type name from the DLL path
        std::string assemblyName = std::filesystem::path(path).stem().string();
        std::string typeName = assemblyName + ".ScriptEntry";

        if (!_coreclrHost->GetEntryPoints(
            assemblyName.c_str(),
            typeName.c_str(),
            &_managedInit,
            &_managedUpdate,
            &_managedShutdown))
        {
            return false;
        }

        _valid = true;
        return true;
    }

    bool ScriptModuleCoreCLR::Init(ScriptEngine* engine)
    {
        if (_managedInit)
        {
            using InitFn = int(*)(ScriptEngine*);
            int result = ((InitFn)_managedInit)(engine);
            return result != 0;
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
    void ScriptModuleCoreCLR::OnMessage(const ScriptMessage& msg)
    {}
}
