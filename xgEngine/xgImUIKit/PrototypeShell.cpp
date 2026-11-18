// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
// Retired prototype UI, kept for reference only. It is never compiled.
#if 0
#include "ImUIInternal.h"
#include "ImUIHost.h"
#include "PrototypeInternal.h"

namespace xg::imui::detail
{
    void DrawMainToolbar()
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ToolbarBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 5.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
        ImGui::BeginChild("##maintoolbar", ImVec2(0.0f, ToolbarHeight), ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        if (BeginDropdown("##account", "XG", Icon::Account))
        {
            ImGui::MenuItem("Sign In...");
            ImGui::MenuItem("Account Settings");
            ImGui::EndPopup();
        }
        ImGui::SameLine(0.0f, 6.0f);
        IconButton("##cloud", Icon::Cloud, false, SegButtonSize);
        ImGui::SetItemTooltip("Cloud Services");
        ImGui::SameLine(0.0f, 2.0f);
        IconButton("##vcs", Icon::Branch, false, SegButtonSize);
        ImGui::SetItemTooltip("Version Control");
        ImGui::SameLine();

        const float playW = SegmentWidth(3);
        ImGui::SetCursorPosX(ImMax(ImGui::GetCursorPosX(), ImFloor((ImGui::GetWindowWidth() - playW) * 0.5f)));
        BeginSegment(3, TopBarBg);
        if (SegButton("##play", Icon::Play, _playing, _playing ? "Stop" : "Play"))
        {
            _playing = !_playing;
            if (!_playing)
                _paused = false;
        }
        if (SegButton("##pause", Icon::Pause, _paused, "Pause"))
            _paused = !_paused;
        SegButton("##step", Icon::Step, false, "Step");
        EndSegment();

        const float layersW = DropdownWidth("Layers"), layoutW = DropdownWidth("Layout");
        AlignRight(SegButtonSize + 4.0f + layersW + 4.0f + layoutW);
        IconButton("##search", Icon::Search, false, SegButtonSize);
        ImGui::SetItemTooltip("Search");
        ImGui::SameLine();
        if (BeginDropdown("##layers", "Layers"))
        {
            ImGui::MenuItem("Everything");
            ImGui::MenuItem("Default", nullptr, true);
            ImGui::MenuItem("UI", nullptr, true);
            ImGui::MenuItem("Water", nullptr, true);
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (BeginDropdown("##layout", "Layout"))
        {
            if (ImGui::MenuItem("Default"))
                _resetLayout = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Reset All Layouts"))
                _resetLayout = true;
            ImGui::EndPopup();
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    static const char* const BuiltInMenus[] = { "File", "Edit", "Assets", "Actor", "Component", "Services", "Window", "Help" };

    void DrawMenus(Window* mainWindow)
    {
        if (ImGui::BeginMenu("File"))
        {
            ImGui::MenuItem("New Scene", "Ctrl N");
            ImGui::MenuItem("Open Scene", "Ctrl O");
            ImGui::MenuItem("Save", "Ctrl S");
            ImGui::MenuItem("Save As...", "Shift Ctrl S");
            ImGui::Separator();
            ImGui::MenuItem("Build Settings...", "Shift Ctrl B");
            ImGui::Separator();
            if (ImGui::MenuItem("Exit") && mainWindow)
                mainWindow->RequestClose();
            DrawRegisteredMenuItems("File");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::MenuItem("Undo", "Ctrl Z");
            ImGui::MenuItem("Redo", "Ctrl Y");
            ImGui::Separator();
            ImGui::MenuItem("Select All", "Ctrl A");
            ImGui::MenuItem("Deselect All", "Shift D");
            ImGui::Separator();
            ImGui::MenuItem("Project Settings...");
            ImGui::MenuItem("Preferences...");
            DrawRegisteredMenuItems("Edit");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Assets"))
        {
            if (ImGui::BeginMenu("Create"))
            {
                ImGui::MenuItem("Folder");
                ImGui::MenuItem("Material");
                ImGui::MenuItem("Script");
                ImGui::EndMenu();
            }
            ImGui::MenuItem("Import New Asset...");
            ImGui::MenuItem("Refresh", "Ctrl R");
            DrawRegisteredMenuItems("Assets");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Actor"))
        {
            ImGui::MenuItem("Create Empty", "Shift Ctrl N");
            if (ImGui::BeginMenu("3D Object"))
            {
                ImGui::MenuItem("Cube");
                ImGui::MenuItem("Plane");
                ImGui::MenuItem("Sphere");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Light"))
            {
                ImGui::MenuItem("Directional Light");
                ImGui::MenuItem("Point Light");
                ImGui::MenuItem("Spot Light");
                ImGui::EndMenu();
            }
            ImGui::MenuItem("Camera");
            DrawRegisteredMenuItems("Actor");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Component"))
        {
            ImGui::MenuItem("Add...", "Shift Ctrl A");
            if (ImGui::BeginMenu("Physics"))
            {
                ImGui::MenuItem("Rigidbody");
                ImGui::MenuItem("Box Collider");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Rendering"))
            {
                ImGui::MenuItem("Camera");
                ImGui::MenuItem("Light");
                ImGui::EndMenu();
            }
            DrawRegisteredMenuItems("Component");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Services"))
        {
            ImGui::MenuItem("Cloud Build");
            ImGui::MenuItem("Version Control");
            DrawRegisteredMenuItems("Services");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window"))
        {
            if (ImGui::BeginMenu("Layouts"))
            {
                if (ImGui::MenuItem("Reset Layout"))
                    _resetLayout = true;
                ImGui::EndMenu();
            }
            ImGui::Separator();
            for (const char* panel : { "Scene", "Game", "Lighting" })
                if (ImGui::MenuItem(panel))
                    ImGui::SetWindowFocus(detail::PanelWindowName(panel, true).c_str());
            DrawRegisteredPanelsMenu();
            DrawRegisteredMenuItems("Window");
            ImGui::EndMenu();
        }
        DrawExtraTopLevelMenus(BuiltInMenus, static_cast<int>(sizeof(BuiltInMenus) / sizeof(BuiltInMenus[0])));
        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("About XGE");
            ImGui::MenuItem("Documentation");
            DrawRegisteredMenuItems("Help");
            ImGui::EndMenu();
        }
    }

    void DrawStatusBar()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::PushStyleColor(ImGuiCol_WindowBg, TopBarBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
        if (ImGui::BeginViewportSideBar("##StatusBar", viewport, ImGuiDir_Down, ImGui::GetFrameHeight(), flags))
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            const float midY = p.y + ImGui::GetTextLineHeight() * 0.5f;
            DrawIcon(ImGui::GetWindowDrawList(), Icon::Info, ImVec2(p.x + 6.0f, midY), 12.0f, TextDim);
            ImGui::SetCursorScreenPos(Offset(p, 18.0f, 0.0f));
            ImGui::TextColored(ToVec4(TextDim), "%s", _playing ? (_paused ? "Paused" : "Playing") : "Ready");

            const char* version = "XGE 0.1";
            ImGui::SameLine();
            AlignRight(ImGui::CalcTextSize(version).x);
            ImGui::TextColored(ToVec4(TextDim), "%s", version);
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    void DockPrototypePanels(ImGuiID dockMain, ImGuiID dockInspector)
    {
        ImGui::DockBuilderDockWindow(detail::PanelWindowName("Scene", true).c_str(), dockMain);
        ImGui::DockBuilderDockWindow(detail::PanelWindowName("Game", true).c_str(), dockMain);
        ImGui::DockBuilderDockWindow(detail::PanelWindowName("Lighting", true).c_str(), dockInspector);
        detail::_selectDefaultTabs = 2;
    }

    void DrawPrototypePanels()
    {
        detail::DrawSceneView();
        detail::DrawGameView();
        detail::DrawLighting();
        if (detail::_selectDefaultTabs > 0 && --detail::_selectDefaultTabs == 0)
            ImGui::SetWindowFocus(detail::PanelWindowName("Scene", true).c_str());
    }
}
#endif
