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

        RendererUnique Renderer;
        WindowUnique MainWindow;

        EventDispatcher* GetDispatcher() override { return _dispatcher; }
        EventQueue* GetQueue() override { return _queue; }
        Messenger* GetMessenger() override { return _messenger; }
		TypeRegistry* GetTypeRegistry() override { return _typeRegistry.get(); }


        void AddLogCallback(LogCallback cb) override;
        void RemoveLogCallback(LogCallback cb) override;

		CodecRegistry* GetCodecRegistry(ScriptBackendType backendType) const override;
		PayloadMode GetPayloadMode() const override { return PayloadMode::JSON; }
    private:
        const char* GetDefaultGroupFor(const char* path);

        ScriptHost* FindHostInGroupForBackend(const char* group,
            ScriptBackendType backend);

        ScriptHost* CreateHostFor(ScriptBackendType backend,
            const char* path);

        void RegisterHostInGroup(const char* group,
            ScriptBackendType backend,
            ScriptHost* host);

    private:
        uint32_t _nextModuleId = 0;
        void* _rendererLib = nullptr;

        void* _moduleStorage = nullptr;
        void* _hostStorage = nullptr;

        ScriptTree* _scriptTree = nullptr;

        EventQueue*      _queue;
        EventDispatcher* _dispatcher;
		Messenger* 	_messenger;
		xgUnique<TypeRegistry> _typeRegistry;

		CodecRegistry* _nativeCodecRegistry = nullptr;
        CodecRegistry* _coreclrCodecRegistry = nullptr;
    };
}
