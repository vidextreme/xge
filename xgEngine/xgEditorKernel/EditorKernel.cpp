#include "pch.h"
#include "EditorKernel.h"
#include "xgEngine.h"
#include "xgScriptHost.h"
#include "xgScriptModule.h"

// ImGui core
#include "imgui.h"
#include "imgui_internal.h"

// ImGui backends (SDL3 + SDL_Renderer3)
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

// SDL3 only in this CPP
#include <SDL3/SDL.h>

namespace xg
{
    EditorKernelModule::EditorKernelModule(const char* id)
        : ScriptModule(id)
    {
    }

    EditorKernelModule::~EditorKernelModule()
    {
        Shutdown();
    }

    bool EditorKernelModule::Init(ScriptEngine* engine)
    {
        if (!engine)
            return false;

        _engine = engine;

        InitImGui();

        _editorHost =
            _engine->AddScriptModule("editor", "Editor.CoreCLR.dll");

        _editorModule = _engine->GetScriptModule("editor");

        return true;
    }

    void EditorKernelModule::Update(float dt)
    {
        BeginImGuiFrame();

        ImGui::Begin("Editor Kernel");
        ImGui::Text("Native editor kernel running.");
        ImGui::Text("dt: %.3f ms", dt * 1000.0f);
        ImGui::End();

        // Managed editor UI runs in Editor.CoreCLR::Update

        EndImGuiFrame();
    }

    void EditorKernelModule::Shutdown()
    {
        ShutdownImGui();

        if (_engine && _editorModule)
        {
            _engine->RemoveScriptModule("editor");
            _editorModule = nullptr;
        }

        _editorHost = nullptr;
        _engine = nullptr;
        _isValid = false;
    }

    // ------------------------------------------------------------
    // ImGui init / shutdown (SDL3 + SDL_Renderer3, CPU path)
    // ------------------------------------------------------------

    void EditorKernelModule::InitImGui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        auto* engine = static_cast<Engine*>(_engine);

        // Platform: SDL window via backend-neutral handle
        SDL_Window* window = static_cast<SDL_Window*>(
            engine->MainWindow->GetPlatformWindow()
            );

        // Editor-only SDL renderer (no DX12, no engine renderer coupling)
        SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, nullptr);
        SDL_SetRenderVSync(sdlRenderer, 1);

        _sdlRenderer = sdlRenderer; // stored as void* in header

        ImGui_ImplSDL3_InitForSDLRenderer(window, sdlRenderer);
        ImGui_ImplSDLRenderer3_Init(sdlRenderer);
    }

    void EditorKernelModule::ShutdownImGui()
    {
        if (_sdlRenderer)
        {
            ImGui_ImplSDLRenderer3_Shutdown();
            ImGui_ImplSDL3_Shutdown();

            SDL_DestroyRenderer(static_cast<SDL_Renderer*>(_sdlRenderer));
            _sdlRenderer = nullptr;
        }

        ImGui::DestroyContext();
    }

    // ------------------------------------------------------------
    // Frame lifecycle
    // ------------------------------------------------------------

    void EditorKernelModule::BeginImGuiFrame()
    {
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void EditorKernelModule::EndImGuiFrame()
    {
        ImGui::Render();

        SDL_Renderer* sdlRenderer = static_cast<SDL_Renderer*>(_sdlRenderer);
        if (!sdlRenderer)
            return;

        // Simple editor-only clear/present; adjust if you integrate with your main render loop
        SDL_SetRenderDrawColor(sdlRenderer, 30, 30, 30, 255);
        SDL_RenderClear(sdlRenderer);

        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdlRenderer);

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        SDL_RenderPresent(sdlRenderer);
    }
}

// Factory for ScriptHostNative (CreateScriptModule path)
extern "C"
XG_MODULE_EXPORT xg::ScriptModule* CreateScriptModule(const char* id)
{
    return new xg::EditorKernelModule(id);
}
