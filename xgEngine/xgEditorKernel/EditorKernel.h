#pragma once
#include "xgScriptModule.h"

struct SDL_Renderer;   // forward declare, keep SDL out of the header
struct SDL_Window;

namespace xg
{
    class ScriptEngine;

    class EditorKernelModule : public ScriptModule
    {
    public:
        EditorKernelModule(const char* id);
        virtual ~EditorKernelModule();

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

    private:
        ScriptEngine* _engine = nullptr;
        ScriptModule* _editorModule = nullptr;
        void* _editorHost = nullptr;

        // Stored as void* to avoid SDL includes in the header
        void* _sdlRenderer = nullptr;

        bool _isValid = true;
    };
}
