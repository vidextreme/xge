// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#include "xgHandles.h"
#include "xgWindow.h"
#include "xgRenderer.h"
#include "xgScriptEngine.h"
#include "xgScriptHost.h"
#include "xgEventQueue.h"
#include "xgEventDispatcher.h"
#include "xgLog.h"
#include "xgEngineDef.h"
namespace xg
{
    class ScriptModule;
    class ScriptHost;
    class ScriptTree;
	class MessageCodec;
    class Messenger;
	class CodecRegistry;
    class ScriptNode;
    struct System;
    class LoggerImpl;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251 4275)
#endif
    class XG_API Engine : public ScriptEngine
    {
    public:
        Engine();
        ~Engine();

        bool Initialize(const EngineConfig& config);
        bool SetRendererModule(const char* path);

        void Run();
        void Shutdown();

        ScriptModule* AddScriptModule(const char* id,
            const char* path,
            ScriptModule* parent = nullptr, 
            const char* group = nullptr) override;

        ScriptModule* GetScriptModule(const char* id) override;
        void RemoveScriptModule(const char* id) override;
        ScriptModule* GetScriptModule(uint32_t moduleID) override;

        RendererUnique Renderer;
        WindowUnique MainWindow;

		TypeRegistry* GetTypeRegistry() override { return _typeRegistry.get(); }

        void AddLogCallback(LogCallback cb) override;
        void RemoveLogCallback(LogCallback cb) override;
        Logger* GetLogger() override;

		CodecRegistry* GetCodecRegistry(ScriptBackendType backendType) const override;
		PayloadMode GetPayloadMode() const override { return PayloadMode::JSON; }
        DynamicObjectUnique CreateDynamic(const TypeSchema* schema) override;

    private:
        const char* GetDefaultGroupFor(const char* path);

        ScriptHost* FindHostInGroupForBackend(const char* group,
            ScriptBackendType backend);

        ScriptHost* CreateHostFor(ScriptBackendType backend,
            const char* path);

        void RegisterHostInGroup(const char* group,
            ScriptBackendType backend,
            ScriptHost* host);

	protected:
		System* GetSystemImpl(TypeID id, ScriptModule* module) override;
		System* GetSystemImpl(TypeID id, const char* moduleID) override;
		void RegisterSystemImpl(ScriptModule* module, TypeID id, System* system) override;
    private:
        std::unordered_map<TypeID, System*> _systems;

        LoggerImpl* _logger = nullptr;

        uint32_t _nextModuleId = 0;
        void* _rendererLib = nullptr;

        void* _moduleStorage = nullptr;
        void* _hostStorage = nullptr;

        ScriptTree* _scriptTree = nullptr;
		ScriptModule* _rootModule = nullptr;
		ScriptNode* _rootNode = nullptr;
		xgUnique<TypeRegistry> _typeRegistry;

		CodecRegistry* _nativeCodecRegistry = nullptr;
        CodecRegistry* _coreclrCodecRegistry = nullptr;
    };
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}
