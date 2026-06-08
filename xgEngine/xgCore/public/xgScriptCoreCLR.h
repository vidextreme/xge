#pragma once

#include "xgScriptModule.h"

namespace xg
{
    class xgHostCoreCLR;

    //
    // Exported functions from xgScriptCoreCLR.dll
    //
    extern "C"
    {
        // Create a CoreCLR-backed ScriptModule instance
        XG_API ScriptModule* CreateScriptModule(xgHostCoreCLR* host, const char* path);

        // Destroy a ScriptModule instance created by this backend
        XG_API void DestroyScriptModule(ScriptModule* module);
    }
}

// Declare the module so the loader system generates:
//   xg::ScriptCoreCLRDLL
//   xg::ScriptCoreCLRLib
//   xg::ScriptCoreCLRProcs
//   xg::LoadScriptCoreCLRLib()
//   xg::FreeScriptCoreCLRLib()
//
XG_DECLARE_MODULE(xg, ScriptCoreCLR)
