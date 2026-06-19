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

        //
        // Script module management
        //
        // Creates or reuses a ScriptHost based on the module path.
        // Returns the ScriptHost used (so caller can reuse it).
        //
        ScriptHost* AddScriptModule(const char* id,
            const char* path,
            ScriptHost* hostOverride = nullptr) override;

        ScriptModule* GetScriptModule(const char* id) override;
        void RemoveScriptModule(const char* id) override;

        Renderer* Renderer = nullptr;
        std::unique_ptr<Window> MainWindow;

        EventDispatcher* GetDispatcher() override;
        EventQueue* GetQueue() override;

        void AddLogCallback(LogCallback cb) override;
        void RemoveLogCallback(LogCallback cb) override;
    private:
        ScriptHost* GetOrCreateHostFor(const std::string& path);

    private:
        void* _rendererLib = nullptr;

        // Opaque internal storage
        void* _moduleStorage = nullptr;
        void* _hostStorage = nullptr; // NEW: stores ScriptHost* for destruction

        EventQueue _queue;          // value type
        EventDispatcher _dispatcher; // value type
    };
}
