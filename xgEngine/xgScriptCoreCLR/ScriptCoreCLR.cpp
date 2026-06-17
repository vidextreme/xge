#include "pch.h"
#include "xgModules.h"
#include "ScriptHostCoreCLR.h"
#include "ScriptModuleCoreCLR.h"


XG_MODULE_EXPORT xg::ScriptHost* CreateScriptHostCoreCLR(const char* path)
{
    return new xg::ScriptHostCoreCLR();
}