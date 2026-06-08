#include "pch.h"
#include "xgScriptCoreCLR.h"
#include "public/xgScriptModuleCoreCLR.h"

// ⭐ REQUIRED — this is what you're missing
XG_IMPLEMENT_MODULE(xg, ScriptCoreCLR)

namespace xg
{
    extern "C"
    {
        XG_API ScriptModule* CreateScriptModule(xgHostCoreCLR* host, const char* path)
        {
            auto* module = new xgScriptModuleCoreCLR(host);
            if (!module->Load(path))
            {
                delete module;
                return nullptr;
            }
            return module;
        }

        XG_API void DestroyScriptModule(ScriptModule* module)
        {
            delete module;
        }
    }
}
