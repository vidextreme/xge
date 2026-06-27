#include "pch.h"
#include "EditorKernel.h"
#include "xgEngine.h"
#include "xgScriptHost.h"
#include "xgScriptModule.h"
#include "xgMemberCallback.h"
#include "xgEventDispatcher.h"
#include "platform/xgEventToSDL.h"
#include "xgListenerBinding.h"

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

    void SetImGuiTheme_Blender2026()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // --- Core background ---
        colors[ImGuiCol_WindowBg] = ImVec4(0.145f, 0.145f, 0.145f, 1.00f); // Main panels
        colors[ImGuiCol_ChildBg] = ImVec4(0.145f, 0.145f, 0.145f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);

        // --- Text ---
        colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

        // --- Headers / Collapsing ---
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

        // --- Buttons ---
        colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);

        // --- Frames (input boxes, sliders) ---
        colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);

        // --- Tabs ---
        colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

        // --- Scrollbars ---
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);

        // --- Checkboxes / Radio / Slider grabs ---
        colors[ImGuiCol_CheckMark] = ImVec4(0.35f, 0.55f, 0.95f, 1.00f); // Blender blue
        colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.55f, 0.95f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.25f, 0.45f, 0.85f, 1.00f);

        // --- Resize grips ---
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);

        // --- Separators ---
        colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

        // --- Title bars ---
        colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

        // --- Style metrics (Blender is flat + minimal rounding) ---
        style.WindowRounding = 2.0f;
        style.FrameRounding = 2.0f;
        style.GrabRounding = 2.0f;
        style.ScrollbarRounding = 2.0f;

        style.WindowBorderSize = 0.5f;
        style.FrameBorderSize = 0.3f;

        colors[ImGuiCol_Border] = ImVec4(0.2f, 0.2f, 0.2f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
    }


    void DrawMainDockspace_BlenderLike()
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_MenuBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin("MainDockspace", nullptr, windowFlags);
        ImGui::PopStyleVar(2);

        ImGuiID dockspaceID = ImGui::GetID("MainDockspaceID");
        ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        static bool firstLayout = true;
        if (firstLayout)
        {
            firstLayout = false;
            ImGui::DockBuilderRemoveNode(dockspaceID);
            ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

            ImGuiID dock_main = dockspaceID;
            ImGuiID dock_left;
            ImGuiID dock_right;
            ImGuiID dock_bottom;
            ImGuiID dock_timeline;
            ImGuiID dock_assets;
            ImGuiID dock_topRight;

            // Bottom area (Timeline + Assets)
            ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.30f, &dock_bottom, &dock_main);

            // Split bottom horizontally: Timeline (left) and Assets (right)
            ImGui::DockBuilderSplitNode(dock_bottom, ImGuiDir_Right, 0.60f, &dock_assets, &dock_timeline);

            // Right side (Properties + Outliner)
            ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.30f, &dock_right, &dock_main);
            ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.40f, &dock_topRight, &dock_right);

            // Left Toolbar
            ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.12f, &dock_left, &dock_main);

            // Assign windows
            ImGui::DockBuilderDockWindow("Viewport", dock_main);
            ImGui::DockBuilderDockWindow("Outliner", dock_topRight);
            ImGui::DockBuilderDockWindow("Properties", dock_right);
            ImGui::DockBuilderDockWindow("Timeline", dock_timeline);
            ImGui::DockBuilderDockWindow("Assets", dock_assets);
            ImGui::DockBuilderDockWindow("Toolbar", dock_left);

            ImGui::DockBuilderFinish(dockspaceID);
        }


        // Optional menu bar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                ImGui::MenuItem("New");
                ImGui::MenuItem("Open...");
                ImGui::MenuItem("Save");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                ImGui::MenuItem("Preferences");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Layout"))
            {
                ImGui::MenuItem("Reset Blender Layout");
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::End();
    }


    void DrawBlenderPanels()
    {
        // -------------------------
        // Viewport (deep sky-blue)
        // -------------------------
        if (ImGui::Begin("Viewport"))
        {
            ImVec2 size = ImGui::GetContentRegionAvail();
            ImDrawList* draw = ImGui::GetWindowDrawList();

            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);

            // Deep sky-blue background
            ImU32 deepSkyBlue = IM_COL32(70, 130, 180, 255);
            draw->AddRectFilled(p0, p1, deepSkyBlue);

            ImGui::SetCursorScreenPos(ImVec2(p0.x + 10, p0.y + 10));
            ImGui::Text("3D Viewport");
        }
        ImGui::End();

        // -------------------------
        // Toolbar (full-width buttons)
        // -------------------------
        if (ImGui::Begin("Toolbar"))
        {
            ImGui::Text("Tools");

            float fullWidth = ImGui::GetContentRegionAvail().x;

            ImGui::Button("Select", ImVec2(fullWidth, 0));
            ImGui::Button("Move", ImVec2(fullWidth, 0));
            ImGui::Button("Rotate", ImVec2(fullWidth, 0));
            ImGui::Button("Scale", ImVec2(fullWidth, 0));
        }
        ImGui::End();

        // -------------------------
        // Outliner
        // -------------------------
        if (ImGui::Begin("Outliner"))
        {
            ImGui::Text("Scene Collection");
            ImGui::BulletText("Camera");
            ImGui::BulletText("Light");
            ImGui::BulletText("Cube");
        }
        ImGui::End();

        // -------------------------
        // Properties (safe sliders)
        // -------------------------
        if (ImGui::Begin("Properties"))
        {
            ImGui::Text("Properties");
            ImGui::Separator();
            ImGui::Text("Object");

            static float locX = 0.0f;
            static float locY = 0.0f;
            static float locZ = 0.0f;

            ImGui::SliderFloat("Location X", &locX, -10.0f, 10.0f);
            ImGui::SliderFloat("Location Y", &locY, -10.0f, 10.0f);
            ImGui::SliderFloat("Location Z", &locZ, -10.0f, 10.0f);
        }
        ImGui::End();

        // -------------------------
        // Timeline (safe)
        // -------------------------
        if (ImGui::Begin("Timeline"))
        {
            ImGui::Text("Timeline");

            static int frame = 1;
            ImGui::SliderInt("Frame", &frame, 1, 250);
        }
        ImGui::End();

        // -------------------------
        // Assets Panel (Unity-style)
        // -------------------------
        if (ImGui::Begin("Assets"))
        {
            ImGui::Text("Assets");
            ImGui::Separator();

            float cellSize = 80.0f;
            float padding = 10.0f;
            float panelWidth = ImGui::GetContentRegionAvail().x;

            int columns = (int)(panelWidth / (cellSize + padding));
            if (columns < 1) columns = 1;

            ImGui::Columns(columns, nullptr, false);

            for (int i = 0; i < 20; i++)
            {
                ImGui::Button(("Asset " + std::to_string(i)).c_str(), ImVec2(cellSize, cellSize));
                ImGui::NextColumn();
            }

            ImGui::Columns(1);
        }
        ImGui::End();
    }


    //
    // ------------------------------------------------------------
    // EditorKernelModule
    // ------------------------------------------------------------
    //

    EditorKernelModule::EditorKernelModule(const char* id, ScriptHost* host, const char* group)
        : ScriptModule(id, host, group)
    {}

    EditorKernelModule::~EditorKernelModule()
    {
        Shutdown();
    }

    bool EditorKernelModule::Init(ScriptEngine* engine)
    {
        if (!engine)
            return false;

        _engine = engine;

        //
        // Create event listener (modernized)
        //
        _eventListener = std::make_unique<ListenerBinding<EditorKernelModule>>(
            this,
            &EditorKernelModule::OnEvent
        );

        engine->GetDispatcher()->AddListener(_eventListener.get());

        //
        // ImGui setup
        //
        InitImGui();

        //
        // Load managed editor module
        //
        const char* group = GetGroup();
        _editorModule = _engine->AddScriptModule("editor", "Editor.CoreCLR.dll", this, group);

        return true;
    }

    void EditorKernelModule::Shutdown()
    {
        if (!_isValid)
            return;

        //
        // Unregister event listener
        //
        if (_engine)
        {
            if (auto* dispatcher = _engine->GetDispatcher())
                dispatcher->RemoveListener(_eventListener.get());
        }

        _eventListener.reset();

        //
        // Shutdown ImGui
        //
        ShutdownImGui();

        //
        // Remove managed editor module
        //
        if (_engine && _editorModule)
        {
            _engine->RemoveScriptModule("editor");
            _editorModule = nullptr;
        }

        _engine = nullptr;
        _isValid = false;
    }

    //
    // ------------------------------------------------------------
    // Update
    // ------------------------------------------------------------
    //
    void EditorKernelModule::Update(float dt)
    {
        BeginImGuiFrame();

        DrawMainDockspace_BlenderLike();
        DrawBlenderPanels();

        EndImGuiFrame();
    }

    //
    // ------------------------------------------------------------
    // ImGui Init / Shutdown
    // ------------------------------------------------------------
    //
    void EditorKernelModule::InitImGui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        // Fonts
        io.Fonts->AddFontFromFileTTF("../editor/fonts/DejaVuSans.ttf", 14.0f);
        io.Fonts->AddFontFromFileTTF("../editor/fonts/DejaVuSans.ttf", 13.0f);
        io.Fonts->AddFontFromFileTTF("../editor/fonts/DejaVuSans-Bold.ttf", 13.0f);

        SetImGuiTheme_Blender2026();

        auto* engine = static_cast<Engine*>(_engine);
        SDL_Window* window = static_cast<SDL_Window*>(engine->MainWindow->GetPlatformWindow());

        SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, nullptr);
        SDL_SetRenderVSync(sdlRenderer, 1);

        _sdlRenderer = sdlRenderer;

        ImGui_ImplSDL3_InitForSDLRenderer(window, sdlRenderer);
        ImGui_ImplSDLRenderer3_Init(sdlRenderer);

        SDL_Surface* icon = SDL_LoadBMP("../editor/xge-icon.bmp");
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    }

    void EditorKernelModule::ShutdownImGui()
    {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();

        if (_sdlRenderer)
        {
            SDL_DestroyRenderer(static_cast<SDL_Renderer*>(_sdlRenderer));
            _sdlRenderer = nullptr;
        }

        ImGui::DestroyContext();
    }

    //
    // ------------------------------------------------------------
    // Frame lifecycle
    // ------------------------------------------------------------
    //
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

    //
    // ------------------------------------------------------------
    // Event Handling
    // ------------------------------------------------------------
    //
    void EditorKernelModule::OnEvent(const xgEvent& e)
    {
        SDL_Window* window = static_cast<SDL_Window*>(
            static_cast<Engine*>(_engine)->MainWindow->GetPlatformWindow()
            );

        SDL_Event sdl = xg::ToSDL(e, window);
        ImGui_ImplSDL3_ProcessEvent(&sdl);
    }
    void EditorKernelModule::OnMessage(const ScriptMessage& msg)
    {}
}

//
// ------------------------------------------------------------
// ScriptHostNative factory
// ------------------------------------------------------------
//
extern "C"
XG_MODULE_EXPORT xg::ScriptModule* CreateScriptModule(const char* id, xg::ScriptHost* host, const char* group)
{
    return new xg::EditorKernelModule(id, host, group);
}
