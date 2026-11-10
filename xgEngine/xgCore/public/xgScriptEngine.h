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
	struct TypeSchema;
    struct Subsystem;
    struct EngineConfig;
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

        virtual ScriptModule* GetScriptModule(uint32_t moduleID) = 0;

        virtual TypeRegistry* GetTypeRegistry() = 0;

        virtual void AddLogCallback(LogCallback cb) = 0;
        virtual void RemoveLogCallback(LogCallback cb) = 0;

        virtual CodecRegistry* GetCodecRegistry(ScriptBackendType backendType) const = 0;
        virtual PayloadMode GetPayloadMode() const = 0;
        virtual DynamicObjectUnique CreateDynamic(const TypeSchema* schema) = 0;

        template<typename T>
        T* GetSubsystem(ScriptModule* module = nullptr) {
            return static_cast<T*>(GetSubsystemImpl(T::SuperTypeID, module));
        }

        template<typename T>
        T* GetSubsystemWithAlias(ScriptModule* module, const char* subsystemAlias) {
            return static_cast<T*>(GetSubsystemImpl(fnv1a(subsystemAlias), module));
        }

        template<typename T>
        T* GetSubsystem(const char* moduleID) {
            return static_cast<T*>(GetSubsystemImpl(T::SuperTypeID, moduleID));
        }

        template<typename T>
        void RegisterSubsystem(ScriptModule* module, T* system)
        {
			return RegisterSubsystemImpl(module, T::SuperTypeID, system);
        }

        template<typename T>
        void RegisterSubsystemWithAlias(ScriptModule* module, T* system, const char* subsystemAlias)
        {
            return RegisterSubsystemImpl(module, fnv1a(subsystemAlias), system);
        }

        // Shuts down and destroys the subsystem registered on the module's own node (root when module is null).
        template<typename T>
        bool UnregisterSubsystem(ScriptModule* module)
        {
            return UnregisterSubsystemImpl(module, T::SuperTypeID);
        }

        bool UnregisterSubsystemWithAlias(ScriptModule* module, const char* subsystemAlias)
        {
            return UnregisterSubsystemImpl(module, fnv1a(subsystemAlias));
        }

    protected:
        virtual Subsystem* GetSubsystemImpl(TypeID id, ScriptModule* module) = 0;
        virtual Subsystem* GetSubsystemImpl(TypeID id, const char* moduleID) = 0;
        virtual void RegisterSubsystemImpl(ScriptModule* module, TypeID id, Subsystem* system) = 0;

    public:
        virtual Logger* GetLogger() = 0;
        virtual const EngineConfig* GetEngineConfig() const = 0;

    protected:
        virtual bool UnregisterSubsystemImpl(ScriptModule* module, TypeID id) = 0;
    };
}
