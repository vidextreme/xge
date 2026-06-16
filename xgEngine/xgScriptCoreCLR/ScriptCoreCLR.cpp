#include "pch.h"
#include "xgModules.h"
#include "ScriptHostCoreCLR.h"
#include "ScriptModuleCoreCLR.h"

// ⭐ REQUIRED — this is what you're missing
//XG_IMPLEMENT_MODULE(xg, ScriptCoreCLR)

XG_MODULE_EXPORT xg::ScriptHost* CreateScriptHostCoreCLR(const char* path)
{
    return new xg::ScriptHostCoreCLR();
}