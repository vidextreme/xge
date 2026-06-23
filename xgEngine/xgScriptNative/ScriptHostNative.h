#pragma once

#include "xgScriptHost.h"
#include "xgScriptModule.h"
#include "xgModules.h"

namespace xg
{
    //
    // ScriptHostNative
    //
    class ScriptHostNative : public ScriptHost
    {
    public:
        XG_IMPL_REFCOUNTED()
        ScriptHostNative() = default;
        ~ScriptHostNative() override = default;

        // Load a native script module from the given path.
        ScriptModule* LoadModule(const char* id, const char* path, const char* group) override;
    };
}
