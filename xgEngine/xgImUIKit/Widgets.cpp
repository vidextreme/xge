// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "ImUIInternal.h"

#include <cstring>

namespace xg::imui::detail
{
    bool IconButton(const char* id, Icon icon, bool active, float size, ImU32 iconCol)
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 bg = active ? Accent : (ImGui::IsItemHovered() ? WithAlpha(IM_COL32_WHITE, 0.07f) : Clear);
        if (bg != Clear)
            dl->AddRectFilled(p, Offset(p, size, size), bg, 4.0f);
        DrawIcon(dl, icon, Offset(p, size * 0.5f, size * 0.5f), size * 0.62f, active ? TextOnAccent : iconCol);
        return pressed;
    }

    bool SearchField(const char* id, char* buffer, size_t size, float width)
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, DropdownBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, DropdownHover);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, DropdownBg);
        ImGui::PushStyleColor(ImGuiCol_Border, DropdownBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(22.0f, 3.0f));
        ImGui::SetNextItemWidth(width);
        const bool changed = ImGui::InputTextWithHint(id, "Search", buffer, size);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        DrawIcon(ImGui::GetWindowDrawList(), Icon::Search, ImVec2(p.x + 12.0f, p.y + ImGui::GetFrameHeight() * 0.5f), 11.0f, TextDim);
        return changed;
    }

    bool FlatButton(const char* label, bool active)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, active ? Accent : Clear);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? AccentHover : WithAlpha(IM_COL32_WHITE, 0.07f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Accent);
        bool pressed = ImGui::Button(label);
        ImGui::PopStyleColor(3);
        return pressed;
    }

    void AlignRight(float width)
    {
        float slack = ImGui::GetContentRegionAvail().x - width;
        if (slack > 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + slack);
    }

    std::string PanelWindowName(const char* title, bool withIcon)
    {
        const char* hashes = std::strstr(title, "###");
        const std::string label = hashes ? std::string(title, hashes) : std::string(title);
        const std::string id = hashes ? std::string(hashes + 3) : std::string(title);
        return (withIcon ? std::string("      ") : std::string()) + label + "###" + id;
    }

    void DrawPanelTabIcon(Icon icon)
    {
        if (icon == Icon::None)
            return;
        constexpr float IconSize = 13.0f;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const float padding = ImGui::GetStyle().FramePadding.x;
        if (window->DockIsActive)
        {
            ImGuiDockNode* node = window->DockNode;
            if (!node || !node->HostWindow || !node->TabBar || node->IsHiddenTabBar() || node->IsNoTabBar())
                return;
            const ImRect tab = window->DC.DockTabItemRect;
            if (tab.GetWidth() <= 0.0f)
                return;
            const bool selected = node->TabBar && node->TabBar->VisibleTabId == window->TabId;
            ImDrawList* dl = node->HostWindow->DrawList;
            dl->PushClipRect(tab.Min, tab.Max, false);
            DrawIcon(dl, icon, ImVec2(ImFloor(tab.Min.x + padding + IconSize * 0.5f + 1.0f), ImFloor(tab.GetCenter().y)), IconSize,
                selected ? IconColor : WithAlpha(IconColor, 0.55f));
            dl->PopClipRect();
            return;
        }
        if ((window->Flags & ImGuiWindowFlags_NoTitleBar) != 0 || window->SkipItems)
            return;
        const ImRect bar = window->TitleBarRect();
        window->DrawList->PushClipRect(bar.Min, bar.Max, false);
        DrawIcon(window->DrawList, icon, ImVec2(ImFloor(bar.Min.x + padding + IconSize * 0.5f + 1.0f), ImFloor(bar.GetCenter().y)), IconSize, IconColor);
        window->DrawList->PopClipRect();
    }

    void BeginHeader()
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HeaderBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 3.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
        ImGui::BeginChild("##header", ImVec2(0.0f, HeaderHeight), ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    void EndHeader()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    void BeginBody(const char* id, ImVec2 padding, ImGuiWindowFlags flags)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
        ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding, flags);
        ImGui::PopStyleVar();
    }

}
