#pragma once
#include "xgModules.h"
#include "xgEngineDef.h"
namespace xg
{
    class ScriptEngine;
	class ScriptHost;
    class ScriptModule
    {
    public:
        explicit ScriptModule(const char* id, ScriptHost* host, const char* group)
            : _id(id)
            , _host(host)
            , _group(group)
        {
        }

        virtual ~ScriptModule() = default;

        const char* GetId() const { return _id; }
        ScriptHost* GetHost() const { return _host; }
        const char* GetGroup() const { return _group; }

        virtual bool Init(ScriptEngine* engine) = 0;
        virtual void Update(float dt) = 0;
        virtual void Shutdown() = 0;
        virtual bool IsValid() const = 0;

    protected:
        ScriptHost* _host;

    private:
        const char* _id; // user-defined stable C-string (e.g., "editor", "game")
        const char* _group;
    };
}
