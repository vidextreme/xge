#include "pch.h"
#include "ScriptHostNative.h"
#include "ScriptModuleNative.h"

namespace xg
{
    using InitFunc = ScriptModuleNative::InitFunc;
    using UpdateFunc = ScriptModuleNative::UpdateFunc;
    using ShutdownFunc = ScriptModuleNative::ShutdownFunc;

    ScriptModule* ScriptHostNative::LoadModule(const char* id, const char* path)
    {
        if (!path)
            return nullptr;

        void* lib = xg::LoadModule(path);
        if (!lib)
            return nullptr;

        auto initFn =
            (InitFunc)xg::GetSymbol(lib, "ScriptModule_Init");
        auto updateFn =
            (UpdateFunc)xg::GetSymbol(lib, "ScriptModule_Update");
        auto shutdownFn =
            (ShutdownFunc)xg::GetSymbol(lib, "ScriptModule_Shutdown");

        if (!initFn || !updateFn || !shutdownFn)
        {
            xg::UnloadModule(lib);
            return nullptr;
        }

        ScriptModuleNative* module =
            new ScriptModuleNative(id, lib, initFn, updateFn, shutdownFn);

        if (!module->IsValid())
        {
            delete module;
            return nullptr;
        }

        return module;
    }
}
