// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "ImUIInternal.h"
#include "ImUIHost.h"

namespace xg::imui::detail
{
    void DrawTitleBarControls(Window* window, const ImRect& bar, float dragStartX)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float barH = bar.GetHeight();
        const float controlsX = bar.Max.x - 3.0f * WindowButtonWidth;

        const bool maximized = window->IsMaximized();
        const Icon icons[3] = { Icon::WinMinimize, maximized ? Icon::WinRestore : Icon::WinMaximize, Icon::WinClose };
        const char* tips[3] = { "Minimize", maximized ? "Restore" : "Maximize", "Close" };
        for (int i = 0; i < 3; ++i)
        {
            ImVec2 b0(controlsX + i * WindowButtonWidth, bar.Min.y);
            ImVec2 b1(b0.x + WindowButtonWidth, bar.Max.y);
            ImGui::SetCursorScreenPos(b0);
            ImGui::PushID(i);
            bool pressed = ImGui::InvisibleButton("##wincontrol", ImVec2(WindowButtonWidth, barH));
            bool hovered = ImGui::IsItemHovered();
            bool held = ImGui::IsItemActive();
            ImGui::SetItemTooltip("%s", tips[i]);
            ImGui::PopID();

            const bool isClose = (i == 2);
            if (hovered || held)
            {
                ImU32 bg = isClose ? (held ? WithAlpha(CloseHover, 0.8f) : CloseHover)
                                   : WithAlpha(IM_COL32_WHITE, held ? 0.14f : 0.08f);
                dl->AddRectFilled(b0, b1, bg);
            }
            DrawIcon(dl, icons[i], ImVec2(ImFloor((b0.x + b1.x) * 0.5f), ImFloor((b0.y + b1.y) * 0.5f)), 10.0f,
                (isClose && hovered) ? IM_COL32_WHITE : IconColor);

            if (pressed)
            {
                if (i == 0) window->Minimize();
                else if (i == 1) { if (maximized) window->Restore(); else window->Maximize(); }
                else window->RequestClose();
            }
        }

        ImRect drag(ImVec2(dragStartX, bar.Min.y), ImVec2(controlsX, bar.Max.y));
        if (drag.GetWidth() > 0.0f)
        {
            const char* title = window->GetTitle();
            if (title && title[0])
            {
                ImVec2 ts = ImGui::CalcTextSize(title);
                float x = ImClamp(ImFloor((bar.Min.x + bar.Max.x - ts.x) * 0.5f), drag.Min.x + 8.0f, ImMax(drag.Min.x + 8.0f, drag.Max.x - 8.0f - ts.x));
                float y = ImFloor(bar.Min.y + (barH - ts.y) * 0.5f);
                float maxX = drag.Max.x - 8.0f;
                ImGui::PushStyleColor(ImGuiCol_Text, TextDim);
                ImGui::RenderTextEllipsis(dl, ImVec2(x, y), ImVec2(maxX, y + ts.y), maxX, title, nullptr, &ts);
                ImGui::PopStyleColor();
            }

            if (ImGui::IsMouseHoveringRect(drag.Min, drag.Max) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (maximized) window->Restore(); else window->Maximize();
            }
        }

        const ImVec2 origin = ImGui::GetMainViewport()->Pos;
        WindowRect rect = {
            static_cast<int>(drag.Min.x - origin.x), static_cast<int>(drag.Min.y - origin.y),
            static_cast<int>(ImMax(0.0f, drag.GetWidth())), static_cast<int>(barH) };
        window->SetDragRegions(&rect, rect.Width > 0 ? 1 : 0);
    }

    static const char* const BuiltInMenus[] = { "File", "Edit", "Assets", "Actor", "Component", "Window", "Help" };

    static void DrawRegisteredMenu(const char* name)
    {
        if (HasRegisteredMenuItems(name) && ImGui::BeginMenu(name))
        {
            DrawRegisteredMenuItems(name, false);
            ImGui::EndMenu();
        }
    }

    void DrawMenus(Window* mainWindow)
    {
        if (ImGui::BeginMenu("File"))
        {
            if (HasRegisteredMenuItems("File"))
            {
                DrawRegisteredMenuItems("File", false);
                ImGui::Separator();
            }
            if (ImGui::MenuItem("Exit") && mainWindow)
                mainWindow->RequestClose();
            ImGui::EndMenu();
        }
        for (const char* name : { "Edit", "Assets", "Actor", "Component" })
            DrawRegisteredMenu(name);
        if (ImGui::BeginMenu("Window"))
        {
            if (ImGui::BeginMenu("Layouts"))
            {
                if (ImGui::MenuItem("Reset Layout"))
                    _resetLayout = true;
                ImGui::EndMenu();
            }
            DrawRegisteredPanelsMenu();
            DrawRegisteredMenuItems("Window", true);
            ImGui::EndMenu();
        }
        DrawExtraTopLevelMenus(BuiltInMenus, static_cast<int>(sizeof(BuiltInMenus) / sizeof(BuiltInMenus[0])));
        DrawRegisteredMenu("Help");
    }

    static void DrawToolbar()
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ToolbarBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 5.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
        ImGui::BeginChild("##maintoolbar", ImVec2(0.0f, ToolbarHeight), ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawShellToolbar();
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    static void DrawStatusBar()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::PushStyleColor(ImGuiCol_WindowBg, TopBarBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
        if (ImGui::BeginViewportSideBar("##StatusBar", viewport, ImGuiDir_Down, ImGui::GetFrameHeight(), flags))
            DrawShellStatusBar();
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    static void DrawAppIcon(ImDrawList* dl, float iconX, float barCenterY)
    {
        const float iconSize = static_cast<float>(detail::AppIconSize);
        if (detail::_appIcon)
        {
            const ImVec2 i0(iconX, ImFloor(barCenterY - iconSize * 0.5f));
            dl->AddImage(detail::_appIcon->GetTexRef(),
                i0, detail::Offset(i0, iconSize, iconSize), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), detail::TextColor);
        }
        else
        {
            dl->AddCircleFilled(ImVec2(iconX + iconSize * 0.5f, barCenterY), 8.0f, detail::Accent, 20);
            dl->AddText(ImVec2(iconX + iconSize * 0.5f - 3.5f, barCenterY - ImGui::GetFontSize() * 0.5f), IM_COL32_WHITE, "X");
        }
    }

    static void DrawWindowOutline(Window* mainWindow)
    {
        if (!mainWindow || !mainWindow->HasCustomFrame() || mainWindow->IsMaximized())
            return;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::GetForegroundDrawList(viewport)->AddRect(viewport->Pos,
            ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), IM_COL32(58, 58, 58, 255));
    }

    void DrawWindowFrame(Window* mainWindow)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::PushStyleColor(ImGuiCol_WindowBg, TopBarBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
        if (ImGui::BeginViewportSideBar("##WindowFrame", viewport, ImGuiDir_Up, detail::TopBarHeight, flags))
        {
            const ImGuiWindow* window = ImGui::GetCurrentWindow();
            const ImRect bar(window->Pos, detail::Offset(window->Pos, window->Size.x, window->Size.y));
            const float iconX = bar.Min.x + 8.0f;
            DrawAppIcon(ImGui::GetWindowDrawList(), iconX, ImFloor((bar.Min.y + bar.Max.y) * 0.5f));
            if (mainWindow && mainWindow->HasCustomFrame())
                detail::DrawTitleBarControls(mainWindow, bar, iconX + static_cast<float>(detail::AppIconSize) + 4.0f);
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        DrawWindowOutline(mainWindow);
    }

    void DrawMainDockspace(Window* mainWindow)
    {
        detail::_layoutJustReset = false;
        detail::DrawStatusBar();

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

        ImGui::PushStyleColor(ImGuiCol_WindowBg, detail::GapBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, detail::TopBarHeight * 0.5f - 7.0f));

        ImGui::Begin("MainDockspace", nullptr, windowFlags);
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor();

        if (ImGui::BeginMenuBar())
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImRect bar = ImGui::GetCurrentWindow()->MenuBarRect();
            const float barH = bar.GetHeight();
            const float barCenterY = ImFloor((bar.Min.y + bar.Max.y) * 0.5f);
            const float iconX = bar.Min.x + 8.0f;
            const float iconSize = static_cast<float>(detail::AppIconSize);
            DrawAppIcon(dl, iconX, barCenterY);

            const float textTop = ImFloor(barCenterY - ImGui::GetFontSize() * 0.5f);
            ImGui::SetCursorScreenPos(ImVec2(iconX + iconSize + 4.0f, textTop - ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, ImFloor((barH - ImGui::GetFontSize()) * 0.5f)));
            detail::DrawMenus(mainWindow);
            ImGui::PopStyleVar();
            const float menusEndX = ImGui::GetCursorScreenPos().x;

            if (mainWindow && mainWindow->HasCustomFrame())
                detail::DrawTitleBarControls(mainWindow, bar, menusEndX + 12.0f);

            ImGui::EndMenuBar();
        }

        float dockTop = ImGui::GetCursorScreenPos().y;
        if (detail::HasShellToolbar())
        {
            detail::DrawToolbar();
            dockTop = ImGui::GetItemRectMax().y;
        }

        ImGuiID dockspaceID = ImGui::GetID("MainDockspaceID");
        const ImGuiWindow* host = ImGui::GetCurrentWindow();
        const ImVec2 dockPos(host->Pos.x + 2.0f, dockTop + 2.0f);
        const ImVec2 dockSize(ImMax(host->Size.x - 4.0f, 1.0f), ImMax(host->Pos.y + host->Size.y - dockPos.y - 2.0f, 1.0f));
        ImGui::SetCursorScreenPos(dockPos);
        ImGui::DockSpace(dockspaceID, dockSize, ImGuiDockNodeFlags_None);

        if (detail::_resetLayout)
        {
            detail::_resetLayout = false;
            ImGui::DockBuilderRemoveNode(dockspaceID);
            ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceID, dockSize);

            ImGuiID dockMain = dockspaceID;
            ImGuiID dockInspector, dockProject, dockHierarchy;
            ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.22f, &dockInspector, &dockMain);
            ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, &dockProject, &dockMain);
            ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.20f, &dockHierarchy, &dockMain);

            detail::_layoutJustReset = true;
            DockRegisteredPanels(dockMain, dockHierarchy, dockInspector, dockProject);

            ImGui::DockBuilderFinish(dockspaceID);
        }

        DrawWindowOutline(mainWindow);

        ImGui::End();
    }
}
