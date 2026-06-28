#pragma once

#include "xgModules.h"
#include "xgEngineDef.h"
#include "xgMessage.h"
#include "xgScriptEngine.h"

#include "xgReflection.h"
namespace xg
{
    class ScriptEngine;   // forward declare
    class ScriptHost;

    class ScriptModule
    {
    public:
        explicit ScriptModule(const char* id, ScriptHost* host, const char* group)
            : _id(id)
            , _host(host)
            , _group(group)
        {}

        virtual ~ScriptModule() = default;

        const char* GetId() const { return _id; }
        ScriptHost* GetHost() const { return _host; }
        const char* GetGroup() const { return _group; }

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

        template<typename T>
        void Send(const char* targetId, uint32_t typeId, const T& payload)
        {
            if (!_engine)
                return;

            _engine->GetMessenger()->Send(
                targetId,
                typeId,
                &payload,
                GetTypeInfo<T>()
            );
        }

        template<typename T>
        void Broadcast(uint32_t typeId, const T& payload)
        {
            if (!_engine)
                return;

            _engine->GetMessenger()->Broadcast(
                typeId,
                &payload,
                GetTypeInfo<T>()
            );
        }

        template<typename T>
        T Decode(const ScriptMessage& msg)
        {
            return msg.Decode<T>(_engine->GetMessenger()->GetCodec());
        }

        template<typename T>
        bool DecodeInto(const ScriptMessage& msg, T& outObj)
        {
            return msg.DecodeInto<T>(outObj, _engine->GetMessenger()->GetCodec());
        }


    protected:
        ScriptHost* _host;
        ScriptEngine* _engine = nullptr;

    private:
        const char* _id;
        const char* _group;
    };
}
