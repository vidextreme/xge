#include "pch.h"
#include "ScriptHostNative.h"
#include "ScriptModuleNative.h"
#include "xgModules.h"

namespace xg
{
    using InitFunc = ScriptModuleNative::InitFunc;
    using UpdateFunc = ScriptModuleNative::UpdateFunc;
    using ShutdownFunc = ScriptModuleNative::ShutdownFunc;
    using ScriptModuleFunc = ScriptModuleNative::ScriptModuleFunc;

    ScriptModule* ScriptHostNative::LoadModule(const char* id, const char* path)
    {
        if (!path)
            return nullptr;

        // Load the native DLL
        ModuleHandle lib = xg::LoadModule(path);
        if (!lib)
            return nullptr;

        // First try the modern NativeAOT-style factory
        auto moduleFn = reinterpret_cast<ScriptModuleFunc>(
            xg::GetSymbol(lib, "CreateScriptModule"));

        if (moduleFn)
        {
            // Module is responsible for unloading lib
            ScriptModule* module = moduleFn(id);

            if (!module || !module->IsValid())
            {
                delete module;
                xg::UnloadModule(lib);
                return nullptr;
            }

            // ScriptModuleNative must store lib internally
            return module;
        }

        // Fallback: legacy C ABI triple-function module
        InitFunc initFn = reinterpret_cast<InitFunc>(
            xg::GetSymbol(lib, "ScriptModule_Init"));
        UpdateFunc updateFn = reinterpret_cast<UpdateFunc>(
            xg::GetSymbol(lib, "ScriptModule_Update"));
        ShutdownFunc shutdownFn = reinterpret_cast<ShutdownFunc>(
            xg::GetSymbol(lib, "ScriptModule_Shutdown"));

        if (!initFn || !updateFn || !shutdownFn)
        {
            xg::UnloadModule(lib);
            return nullptr;
        }

        // Create ScriptModuleNative wrapper
        ScriptModule* module =
            new ScriptModuleNative(id, lib, initFn, updateFn, shutdownFn);

        if (!module->IsValid())
        {
            delete module;
            xg::UnloadModule(lib);
            return nullptr;
        }

        return module;
    }
}
