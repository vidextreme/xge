#pragma once

#include "xgBase.h"
#include "xgWindow.h"
#include "xgRenderer.h"
#include "xgScriptEngine.h"
#include "xgEngine.generated.h"
#include "xgEventQueue.h"
#include "xgEventDispatcher.h"
#include "xgLog.h"

namespace xg
{
    class ScriptModule;
    class ScriptHost;
    class ScriptTree;

    //
    // EngineConfig (reflection-driven)
    //
    XG_SERIALIZABLE()
        struct EngineConfig
    {
        XG_FIELD()
            const char* RendererModule = nullptr;
    };


    //
    // Engine
    //
    // Owns:
    //  - Window
    //  - Renderer
    //  - ScriptHosts (grouped)
    //  - ScriptModules
    //  - ScriptTree
    //  - EventQueue / EventDispatcher
    //
    class Engine : public ScriptEngine
    {
    public:
        Engine();
        ~Engine();

        bool Initialize(const EngineConfig& config);
        bool SetRendererModule(const char* path);

        void Run();
        void Shutdown();

        //
        // Script module management
        //
        // Automatically selects or creates a ScriptHost based on:
        //   - file extension (backend)
        //   - optional group name (for isolation)
        //
        // If group == nullptr:
        //      modules are grouped by backend (e.g., "coreclr", "native")
        //
        // If group != nullptr:
        //      modules are isolated into that group (e.g., "sandbox1")
        //
        ScriptModule* AddScriptModule(const char* id,
            const char* path,
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
        //
        // Host selection helpers
        //
        const char* GetDefaultGroupFor(const char* path);
        ScriptHost* FindHostInGroup(const char* group);
        ScriptHost* CreateHostFor(const char* path);
        void        RegisterHostInGroup(const char* group, ScriptHost* host);

    private:
        void* _rendererLib = nullptr;

        // Opaque internal storage for modules
        void* _moduleStorage = nullptr;

        // Stores ScriptHost* entries (vector<HostEntry>)
        void* _hostStorage = nullptr;

        ScriptTree* _scriptTree = nullptr;

        EventQueue      _queue;       // value type
        EventDispatcher _dispatcher;  // value type
    };
}
