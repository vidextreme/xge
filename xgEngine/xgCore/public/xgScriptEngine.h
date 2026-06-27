#pragma once

#include "xgScriptModule.h"
#include "xgLog.h"

namespace xg
{
    class EventDispatcher;
	class EventQueue;
	class Messenger;
    //
    // ScriptEngine
    //
    // Backend-agnostic interface implemented by the Engine.
    // ScriptHosts call into this to load/unload ScriptModules.
    //
    class ScriptEngine
    {
    public:
        virtual ~ScriptEngine() = default;

        virtual ScriptModule* AddScriptModule(const char* id,
            const char* path,
            ScriptModule* parent = nullptr, 
            const char* group = nullptr) = 0;

        virtual ScriptModule* GetScriptModule(const char* id) = 0;
        virtual void RemoveScriptModule(const char* id) = 0;

        virtual EventDispatcher* GetDispatcher() = 0;
        virtual EventQueue* GetQueue() = 0;

        virtual Messenger* GetMessenger() = 0;

        virtual void AddLogCallback(LogCallback cb) = 0;
        virtual void RemoveLogCallback(LogCallback cb) = 0;
    };
}
