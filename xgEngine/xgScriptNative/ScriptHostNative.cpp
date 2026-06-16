#include "pch.h"
#include "ScriptHostNative.h"

namespace xg
{
    using CreateScriptModuleFunc = ScriptModule * (*)();

    ScriptModule* ScriptHostNative::LoadModule(const char* path)
    {
        if (!path)
            return nullptr;

        void* lib = xg::LoadModule(path);
        if (!lib)
            return nullptr;

        auto createModule =
            (CreateScriptModuleFunc)xg::GetSymbol(lib, "CreateScriptModule");

        if (!createModule)
        {
            xg::UnloadModule(lib);
            return nullptr;
        }

        ScriptModule* module = createModule();
        if (!module || !module->IsValid())
        {
            if (module)
                delete module;

            xg::UnloadModule(lib);
            return nullptr;
        }

        return module;
    }
}
