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

    public:
        // Parent of module in the script tree; null for a root module or one that is not in the tree.
        virtual ScriptModule* GetParentScriptModule(ScriptModule* module) const = 0;

        // Startup splash: step text (null keeps it), detail line (null keeps it, "" clears it) and progress in [0, 1]
        // (negative = indeterminate, NaN = keep the current value). Both calls do nothing once the splash has closed.
        virtual void ReportStartup(const char* step, const char* detail, float progress) = 0;
        // Keeps the splash responsive during long blocking work (call every few tens of milliseconds).
        virtual void PumpStartup() = 0;

        // Runs fn(user) on the main thread after the current frame's module and actor updates, outside every module
        // update, so it may add or remove script modules. Callbacks queued while they run wait for the next frame.
        virtual void RunBetweenFrames(void (*fn)(void* user), void* user) = 0;

        // A fresh module id for a module created outside a script host (see AddExternalScriptModule).
        virtual uint32_t AllocateModuleId() = 0;

        // Adds a module built by the caller (constructed with an id from AllocateModuleId) under parent (null = root)
        // and initializes it. The engine owns it from then on, also when this fails (it is deleted). Its host may be
        // null. Outside module updates only.
        virtual bool AddExternalScriptModule(ScriptModule* module, ScriptModule* parent) = 0;

        // Pauses or resumes a module and its subtree: their updates and their actors' component updates stop while the
        // rest of the engine keeps running (the editor's play mode). Returns false for an unknown id.
        virtual bool SetModulePaused(const char* id, bool paused) = 0;
        virtual bool IsModulePaused(const char* id) = 0;
        // Runs a paused module's subtree (and its actors) for one frame at a fixed delta time, on the next frame.
        virtual bool StepModule(const char* id) = 0;

        // Pauses every module's actor (component) updates, on top of the module pauses above; off by default. The editor
        // keeps it on outside play mode.
        virtual void SetActorUpdatesPaused(bool paused) = 0;
        virtual bool AreActorUpdatesPaused() const = 0;
        // Runs the actor updates for one frame at a fixed delta time while they are paused, on the next frame.
        virtual void StepActorUpdates() = 0;
    };
}
