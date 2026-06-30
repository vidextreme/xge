#pragma once
#include "xgScriptModule.h"
#include "xgEvent.h"

#include <memory>   // allowed (non‑public header)

struct SDL_Renderer;   // forward declare, keep SDL out of the header
struct SDL_Window;

namespace xg
{
    class ScriptEngine;
    class EventListener;

    XG_SERIALIZABLE()
        struct PlayerState
    {
        XG_FIELD()
            int health;
        XG_FIELD()
            float x;
        XG_FIELD()
            float y;
    };

    class EditorKernelModule : public ScriptModule
    {
    public:
        EditorKernelModule(const char* id, ScriptHost* host, const char* group);
        ~EditorKernelModule() override;

        bool Init(ScriptEngine* engine) override;
        void Update(float dt) override;
        void Shutdown() override;

        bool IsValid() const override { return _isValid; }

    private:
        // ImGui lifecycle
        void InitImGui();
        void ShutdownImGui();
        void BeginImGuiFrame();
        void EndImGuiFrame();

        // Platform event handler (called by EventDispatcher)
        void OnEvent(const xgEvent& e);
        void OnMessage(const ScriptMessage& msg) override;
    private:
        ScriptEngine* _engine = nullptr;
        ScriptModule* _editorModule = nullptr;

        // Stored as void* to avoid SDL includes in the header
        void* _sdlRenderer = nullptr;

        bool _isValid = true;

        // Modernized: EditorKernel owns its listener
        std::unique_ptr<EventListener> _eventListener;
    };
}
