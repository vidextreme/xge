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
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(22.0f, 4.0f));
        ImGui::SetNextItemWidth(width);
        const bool changed = ImGui::InputTextWithHint(id, "Search", buffer, size);
        ImGui::PopStyleVar();
        DrawFieldFrame(ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), ImGui::IsItemHovered(), ImGui::IsItemActive());
        DrawIcon(ImGui::GetWindowDrawList(), Icon::Search, ImVec2(p.x + 12.0f, p.y + ImGui::GetFrameHeight() * 0.5f), 11.0f, TextDim);
        return changed;
    }

    void DrawFieldFrame(const ImRect& rect, bool hovered, bool active)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 a(ImFloor(rect.Min.x), ImFloor(rect.Min.y));
        const ImVec2 b(ImFloor(rect.Max.x), ImFloor(rect.Max.y));
        if (active)
        {
            _fieldActive = true;
            dl->AddRect(ImVec2(a.x + 0.5f, a.y + 0.5f), ImVec2(b.x - 0.5f, b.y - 0.5f), Accent, 2.0f, 0, 1.0f);
            return;
        }
        const ImU32 side = hovered ? FieldEdgeHover : FieldEdgeSide;
        dl->AddRect(ImVec2(a.x + 0.5f, a.y + 0.5f), ImVec2(b.x - 0.5f, b.y - 0.5f), side, 2.0f, 0, 1.0f);
        dl->AddLine(ImVec2(a.x + 2.0f, a.y + 0.5f), ImVec2(b.x - 2.0f, a.y + 0.5f), hovered ? FieldEdgeHover : FieldEdgeTop, 1.0f);
        dl->AddLine(ImVec2(a.x + 1.0f, a.y + 1.5f), ImVec2(b.x - 1.0f, a.y + 1.5f), FieldShadow, 1.0f);
        dl->AddLine(ImVec2(a.x + 2.0f, b.y - 0.5f), ImVec2(b.x - 2.0f, b.y - 0.5f), hovered ? FieldEdgeHoverBottom : FieldEdgeBottom, 1.0f);
    }

    void DrawChecker(ImDrawList* dl, const ImRect& rect, float cell)
    {
        const ImVec2 a(ImFloor(rect.Min.x), ImFloor(rect.Min.y));
        const ImVec2 b(ImFloor(rect.Max.x), ImFloor(rect.Max.y));
        dl->AddRectFilled(a, b, ColorCheckerDark);
        dl->PushClipRect(a, b, true);
        int row = 0;
        for (float y = a.y; y < b.y; y += cell, ++row)
        {
            int column = 0;
            for (float x = a.x; x < b.x; x += cell, ++column)
            {
                if (((row + column) & 1) == 0)
                    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cell, y + cell), ColorCheckerLight);
            }
        }
        dl->PopClipRect();
    }

    void DrawColorBox(ImDrawList* dl, const ImRect& rect, const ImVec4& color, float rounding)
    {
        const ImVec2 a(ImFloor(rect.Min.x), ImFloor(rect.Min.y));
        const ImVec2 b(ImFloor(rect.Max.x), ImFloor(rect.Max.y));
        const ImU32 solid = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 1.0f));
        if (color.w >= 1.0f)
        {
            dl->AddRectFilled(a, b, solid, rounding);
        }
        else
        {
            DrawChecker(dl, ImRect(a, b), b.y - a.y > 24.0f ? 8.0f : 4.0f);
            const ImU32 alpha = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, ImMax(color.w, 0.0f)));
            dl->AddTriangleFilled(ImVec2(b.x, a.y), b, ImVec2(a.x, b.y), alpha);
            dl->AddTriangleFilled(a, ImVec2(b.x, a.y), ImVec2(a.x, b.y), solid);
        }
        dl->AddRect(ImVec2(a.x + 0.5f, a.y + 0.5f), ImVec2(b.x - 0.5f, b.y - 0.5f), FieldEdgeSide, rounding, 0, 1.0f);
    }

    ImFont* FontSlot(int slot)
    {
        ImFontAtlas* atlas = ImGui::GetIO().Fonts;
        return slot >= 0 && slot < atlas->Fonts.Size ? atlas->Fonts[slot] : ImGui::GetFont();
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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(BarPaddingX, BarPaddingY));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(BarGap, BarGap));
        ImGui::BeginChild("##header", ImVec2(0.0f, BarHeight), ImGuiChildFlags_AlwaysUseWindowPadding,
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
