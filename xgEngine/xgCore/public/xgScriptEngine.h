// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#include "xgLog.h"
#include "xgScriptHost.h"
#include "xgScriptMessage.h"
#include "xgDynamicObject.h"
#include "xgBase.h"

namespace xg
{
    class EventDispatcher;
	class EventQueue;
	class Messenger;
	class ScriptModule;
	class TypeRegistry;
    class CodecRegistry;
	class DynamicObject;
	class TypeSchema;
    class System;
	class SceneGraph;
    XG_ENUM(inherit = byte)
        enum class ScriptBackendType : uint8_t
    {
        Null,
        CoreCLR,
        Native,
        //Squirrel, //very very soon!
        //Lua,
        //Python,
        //JavaScript,
    };
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

        virtual TypeRegistry* GetTypeRegistry() = 0;

        virtual void AddLogCallback(LogCallback cb) = 0;
        virtual void RemoveLogCallback(LogCallback cb) = 0;

        virtual CodecRegistry* GetCodecRegistry(ScriptBackendType backendType) const = 0;
        virtual PayloadMode GetPayloadMode() const = 0;
        virtual DynamicObjectUnique CreateDynamic(const TypeSchema* schema) = 0;


        template<typename T>
        T* GetSystem(ScriptModule* module = nullptr) {
            return static_cast<T*>(GetSystemImpl(T::SuperTypeID, module));
        }

        template<typename T>
        T* GetSystem(const char* moduleID) {
            return static_cast<T*>(GetSystemImpl(T::SuperTypeID, moduleID));
        }

        template<typename T>
        void RegisterSystem(ScriptModule* module, T* system)
        {
			return RegisterSystemImpl(module, T::SuperTypeID, system);
        }
    protected:
        virtual System* GetSystemImpl(TypeID id, ScriptModule* module) = 0;
        virtual System* GetSystemImpl(TypeID id, const char* moduleID) = 0;
        virtual void RegisterSystemImpl(ScriptModule* module, TypeID id, System* system) = 0;
    };
}
