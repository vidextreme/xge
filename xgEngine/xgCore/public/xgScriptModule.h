#pragma once
#include "xgModules.h"
namespace xg
{
    class ScriptEngine;
	class ScriptHost;
    class ScriptModule
    {
    public:
        explicit ScriptModule(const char* id)
            : _id(id)
        {
        }

        virtual ~ScriptModule() = default;

        const char* GetId() const { return _id; }

        virtual bool Init(ScriptEngine* engine) = 0;
        virtual void Update(float dt) = 0;
        virtual void Shutdown() = 0;
        virtual bool IsValid() const = 0;

    private:
        const char* _id; // user-defined stable C-string (e.g., "editor", "game")
    };

    XG_DECLARE_MODULE_FUNCTION(CreateScriptHostCoreCLR, ScriptHost*, const char*);
    XG_API ScriptHost* CreateScriptHostCoreCLR(const char* path);


    XG_DECLARE_MODULE_FUNCTION(CreateScriptHostNative, ScriptHost*, const char*);
    XG_API ScriptHost* CreateScriptHostNative(const char* path);
}

XG_DECLARE_MODULE(xg, ScriptCoreCLR)
XG_DECLARE_MODULE(xg, ScriptNative)
