#pragma once

#include "xgBase.h"
#include "xgWindow.h"
#include "xgRenderer.h"
#include "xgScriptEngine.h"
#include "xgScriptHost.h"
#include "xgEngine.generated.h"
#include "xgEventQueue.h"
#include "xgEventDispatcher.h"
#include "xgLog.h"

namespace xg
{
    class ScriptModule;
    class ScriptHost;
    class ScriptTree;

    XG_SERIALIZABLE()
        struct EngineConfig
    {
        XG_FIELD()
            const char* RendererModule = nullptr;
    };

    class Engine : public ScriptEngine
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

        Renderer* Renderer = nullptr;
        std::unique_ptr<Window> MainWindow;

        EventDispatcher* GetDispatcher() override { return &_dispatcher; }
        EventQueue* GetQueue() override { return &_queue; }

        void AddLogCallback(LogCallback cb) override;
        void RemoveLogCallback(LogCallback cb) override;

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
        void* _rendererLib = nullptr;

        void* _moduleStorage = nullptr;
        void* _hostStorage = nullptr;

        ScriptTree* _scriptTree = nullptr;

        EventQueue      _queue;
        EventDispatcher _dispatcher;
    };
}
