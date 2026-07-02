#include "pch.h"
#include "xgModules.h"
#include "ScriptHostNative.h"

XG_MODULE_EXPORT xg::ScriptHost* CreateScriptHostNative(xg::ScriptEngine* engine, const char* path)
{
    return new xg::ScriptHostNative(engine);
}