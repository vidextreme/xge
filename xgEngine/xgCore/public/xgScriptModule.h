#pragma once

#include "xgModules.h"
#include "xgEngineDef.h"
#include "xgScriptEngine.h"
#include "xgScriptMessage.h"

namespace xg
{
    class ScriptEngine;   // forward declare
    class ScriptHost;

	//standard function pointers for ScriptModule entry points
    using InitFunc = bool (*)(ScriptEngine*, uint32_t);
    using UpdateFunc = void (*)(float);
    using ShutdownFunc = void (*)();
    using OnMessageFunc = void (*)(const char* typeName, void* payload, int payloadSize);

    class ScriptModule
    {
    public:
        explicit ScriptModule(const char* id, uint32_t moduleID, ScriptHost* host, const char* group)
            : _id(id)
            , _host(host)
            , _group(group)
            , _moduleId(moduleID)
        {}

        virtual ~ScriptModule() = default;

        const char* GetId() const { return _id; }
        ScriptHost* GetHost() const { return _host; }
        const char* GetGroup() const { return _group; }
        uint32_t GetModuleId() const { return _moduleId; }
        bool Initialize(ScriptEngine* engine)
        {
            _engine = engine;
            return Init(engine);
        }

        virtual bool Init(ScriptEngine* engine) = 0;
        virtual void Update(float dt) = 0;
        virtual void Shutdown() = 0;
        virtual bool IsValid() const = 0;

        virtual void OnMessage(const ScriptMessage& msg) = 0;       


    protected:
        ScriptHost* _host;
        ScriptEngine* _engine = nullptr;
        uint32_t _moduleId = 0;
    private:
        const char* _id;
        const char* _group;
    };
}
