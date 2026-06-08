#pragma once
#include <memory>

#include "xgBase.h"
#include "xgWindow.h"
#include "xgRenderer.h"

namespace xg
{
    class ScriptModule;
    class ScriptHost;

    struct EngineConfig
    {
        const char* RendererModule = nullptr;
    };

    class Engine
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
            ScriptHost* hostOverride = nullptr);

        ScriptModule* GetScriptModule(const char* id);
        void RemoveScriptModule(const char* id);

        Renderer* Renderer = nullptr;
        std::unique_ptr<Window> MainWindow;

    private:
        ScriptHost* GetOrCreateHostFor(const std::string& path);

    private:
        void* _rendererLib = nullptr;

        // Opaque internal storage
        void* _moduleStorage = nullptr;
        void* _hostStorage = nullptr; // NEW: stores ScriptHost* for destruction
    };
}
