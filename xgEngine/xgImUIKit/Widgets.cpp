// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "ImUIInternal.h"

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

    bool StateButton(const char* id, Icon icon, bool on, const char* tip, float size, bool hasMenu)
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
        bool hovered = ImGui::IsItemHovered();
        bool held = ImGui::IsItemActive();
        if (tip)
            ImGui::SetItemTooltip("%s", tip);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 q = Offset(p, size, size);
        if (on)
            dl->AddRectFilled(p, q, (hovered || held) ? AccentHover : Accent, 3.0f);
        else if (hovered || held)
            dl->AddRectFilled(p, q, WithAlpha(IM_COL32_WHITE, held ? 0.10f : 0.06f), 3.0f);
        DrawIcon(dl, icon, ImVec2(ImFloor(p.x + size * 0.5f), ImFloor(p.y + size * 0.5f)), size * 0.62f, on ? TextOnAccent : IconColor);
        if (hasMenu)
            dl->AddTriangleFilled(ImVec2(q.x - 6.0f, q.y - 2.0f), ImVec2(q.x - 2.0f, q.y - 2.0f), ImVec2(q.x - 2.0f, q.y - 6.0f),
                on ? WithAlpha(TextOnAccent, 0.8f) : TextDim);
        return pressed;
    }

    bool PressedMenuCorner()
    {
        return ImGui::GetIO().MouseClickedPos[0].x >= ImGui::GetItemRectMax().x - 7.0f;
    }



    void BeginSegment(int count, ImU32 bg)
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, p.y - 1.0f), ImVec2(p.x + SegmentWidth(count), p.y + SegButtonSize + 1.0f), bg, 4.0f);
        ImGui::SetCursorScreenPos(ImVec2(p.x + 1.0f, p.y));
        _segmentStart = p;
        _segmentIndex = 0;
    }

    bool SegButton(const char* id, Icon icon, bool on, const char* tip, bool hasMenu)
    {
        if (_segmentIndex++ > 0)
            ImGui::SameLine(0.0f, 1.0f);
        return StateButton(id, icon, on, tip, SegButtonSize, hasMenu);
    }

    void EndSegment()
    {
        ImGui::SameLine(0.0f, 1.0f + SegmentGap);
    }

    bool BeginAnchoredPopup(const char* id, bool openNow, ImVec2 pos)
    {
        ImGuiID popupId = ImGui::GetID(id);
        if (openNow)
            ImGui::OpenPopupEx(popupId);
        if (!ImGui::IsPopupOpen(popupId, ImGuiPopupFlags_None))
            return false;
        ImGui::SetNextWindowPos(pos);
        return ImGui::BeginPopupEx(popupId, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
    }


    float DropdownWidth(const char* label, Icon icon)
    {
        return ImGui::CalcTextSize(label, nullptr, true).x + (icon != Icon::None ? 18.0f : 0.0f) + 26.0f;
    }

    bool BeginDropdown(const char* id, const char* label, Icon icon, float width, ImU32 iconCol)
    {
        if (width <= 0.0f)
            width = DropdownWidth(label, icon);
        ImVec2 p = ImGui::GetCursorScreenPos();
        const float h = ImGui::GetFrameHeight();
        ImGui::PushID(id);
        bool pressed = ImGui::InvisibleButton("##dropdown", ImVec2(width, h));
        bool hovered = ImGui::IsItemHovered();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 q = Offset(p, width, h);
        dl->AddRectFilled(p, q, hovered ? DropdownHover : DropdownBg, 4.0f);
        dl->AddRect(p, q, DropdownBorder, 4.0f);
        const float midY = ImFloor(p.y + h * 0.5f);
        float x = p.x + 7.0f;
        if (icon != Icon::None)
        {
            DrawIcon(dl, icon, ImVec2(x + 6.0f, midY), 12.0f, iconCol);
            x += 18.0f;
        }
        dl->PushClipRect(p, ImVec2(q.x - 14.0f, q.y), true);
        dl->AddText(ImVec2(x, midY - ImGui::GetFontSize() * 0.5f), TextColor, label, ImGui::FindRenderedTextEnd(label));
        dl->PopClipRect();
        dl->AddTriangleFilled(ImVec2(q.x - 12.0f, midY - 2.0f), ImVec2(q.x - 6.0f, midY - 2.0f), ImVec2(q.x - 9.0f, midY + 2.0f), TextDim);
        bool open = BeginAnchoredPopup("##dropdownpopup", pressed, ImVec2(p.x, q.y + 2.0f));
        ImGui::PopID();
        return open;
    }

    void SearchField(const char* id, char* buffer, size_t size, float width)
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, DropdownBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, DropdownHover);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, DropdownBg);
        ImGui::PushStyleColor(ImGuiCol_Border, DropdownBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(22.0f, 3.0f));
        ImGui::SetNextItemWidth(width);
        ImGui::InputTextWithHint(id, "Search", buffer, size);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        DrawIcon(ImGui::GetWindowDrawList(), Icon::Search, ImVec2(p.x + 12.0f, p.y + ImGui::GetFrameHeight() * 0.5f), 11.0f, TextDim);
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


    bool BeginStyledPanel(const char* name, ImU32 bodyCol, bool defaultTab)
    {
        ImGuiWindowClass wc;
        wc.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
        wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
        ImGui::SetNextWindowClass(&wc);
        ImGui::SetNextWindowSize(ImVec2(420.0f, 320.0f), ImGuiCond_FirstUseEver);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, GapBg);
        bool open = ImGui::Begin(name, nullptr,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (defaultTab && _selectDefaultTabs > 0 && window->DockNode && window->DockNode->TabBar)
            window->DockNode->TabBar->NextSelectedTabId = window->TabId;
        if (open)
        {
            const ImRect r = ImGui::GetCurrentWindow()->InnerRect;
            ImGui::GetWindowDrawList()->AddRectFilled(r.Min, r.Max, bodyCol, AreaRounding, ImDrawFlags_RoundCornersBottom);
        }
        return open;
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

    bool VerticalIconTabs(const Icon* icons, const ImU32* colors, const char* const* tips, int count, int& active)
    {
        bool changed = false;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 2.0f));
        ImGui::BeginChild("##vtabs", ImVec2(VTabWidth, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImDrawList* dl = window->DrawList;
        dl->AddRectFilled(window->Pos, Offset(window->Pos, window->Size.x, window->Size.y), TopBarBg, AreaRounding, ImDrawFlags_RoundCornersBottomLeft);
        const float cell = VTabWidth - 4.0f;
        for (int i = 0; i < count; ++i)
        {
            ImVec2 p(window->Pos.x + 4.0f, ImGui::GetCursorScreenPos().y);
            ImGui::SetCursorScreenPos(p);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##vtab", ImVec2(cell, cell)))
            {
                active = i;
                changed = true;
            }
            const bool hovered = ImGui::IsItemHovered();
            ImGui::SetItemTooltip("%s", tips[i]);
            ImGui::PopID();

            const bool on = (active == i);
            if (on)
                dl->AddRectFilled(p, ImVec2(window->Pos.x + VTabWidth, p.y + cell), HeaderBg, AreaRounding, ImDrawFlags_RoundCornersLeft);
            else if (hovered)
                dl->AddRectFilled(p, Offset(p, cell, cell), WithAlpha(IM_COL32_WHITE, 0.05f), AreaRounding);
            DrawIcon(dl, icons[i], Offset(p, cell * 0.5f, cell * 0.5f), 13.0f, on ? colors[i] : WithAlpha(colors[i], 0.7f));
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::SameLine(0.0f, 0.0f);
        return changed;
    }

    bool BeginPanel(const char* title, Icon icon, ImU32 iconCol, bool defaultOpen)
    {
        ImGui::PushID(title);
        ImGuiStorage* storage = ImGui::GetStateStorage();
        const ImGuiID openId = ImGui::GetID("##open");
        bool open = storage->GetBool(openId, defaultOpen);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float w = ImGui::GetContentRegionAvail().x;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImVec2 q = Offset(p, w, PanelHeaderHeight);
        if (ImGui::InvisibleButton("##panelheader", ImVec2(w, PanelHeaderHeight)))
        {
            open = !open;
            storage->SetBool(openId, open);
        }
        const bool hovered = ImGui::IsItemHovered();
        dl->AddRectFilled(p, q, hovered ? PanelHeaderHov : PanelHeaderBg, AreaRounding,
            open ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll);

        const float midY = ImFloor(p.y + PanelHeaderHeight * 0.5f);
        if (open)
            dl->AddTriangleFilled(ImVec2(p.x + 7.0f, midY - 2.0f), ImVec2(p.x + 15.0f, midY - 2.0f), ImVec2(p.x + 11.0f, midY + 3.0f), TextDim);
        else
            dl->AddTriangleFilled(ImVec2(p.x + 9.0f, midY - 4.0f), ImVec2(p.x + 14.0f, midY), ImVec2(p.x + 9.0f, midY + 4.0f), TextDim);
        DrawIcon(dl, icon, ImVec2(p.x + 28.0f, midY), 13.0f, iconCol);
        ImFont* bold = ImGui::GetIO().Fonts->Fonts.Size > 2 ? ImGui::GetIO().Fonts->Fonts[2] : nullptr;
        ImGui::PushFont(bold, 0.0f);
        dl->AddText(ImVec2(p.x + 40.0f, midY - ImGui::GetFontSize() * 0.5f), TextColor, title);
        ImGui::PopFont();
        DrawIcon(dl, Icon::Kebab, ImVec2(q.x - 10.0f, midY), 10.0f, TextDim);

        if (!open)
        {
            ImGui::PopID();
            ImGui::Dummy(ImVec2(0.0f, 1.0f));
            return false;
        }

        ImGui::SetCursorScreenPos(ImVec2(p.x, q.y));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, PanelBg);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, AreaRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
        ImGui::BeginChild("##panelbody", ImVec2(w, 0.0f), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        ImGuiWindow* body = ImGui::GetCurrentWindow();
        body->DrawList->AddRectFilled(body->Pos, Offset(body->Pos, body->Size.x, AreaRounding), PanelBg);
        return true;
    }

    void EndPanel()
    {
        ImGui::EndChild();
        ImGui::PopID();
        ImGui::Dummy(ImVec2(0.0f, 1.0f));
    }

    void PropertyLabel(const char* label, float labelWidth)
    {
        const float x0 = ImGui::GetCursorPosX();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ToVec4(TextDim), "%s", label);
        ImGui::SameLine(x0 + labelWidth);
        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    void PropertyVec3(const char* name, float* v, float speed, const char* format)
    {
        static const char* axes[3] = { "X", "Y", "Z" };
        PropertyLabel(name);
        const float axisW = ImGui::CalcTextSize("X").x + 3.0f;
        const float gap = 6.0f;
        const float fieldW = ImMax(24.0f, (ImGui::GetContentRegionAvail().x - 3.0f * axisW - 2.0f * gap) / 3.0f);
        ImGui::PushID(name);
        for (int i = 0; i < 3; ++i)
        {
            if (i > 0)
                ImGui::SameLine(0.0f, gap);
            ImGui::TextColored(ToVec4(TextDim), "%s", axes[i]);
            ImGui::SameLine(0.0f, 3.0f);
            ImGui::PushID(i);
            ImGui::SetNextItemWidth(fieldW);
            ImGui::DragFloat("##v", &v[i], speed, 0.0f, 0.0f, format);
            ImGui::PopID();
        }
        ImGui::PopID();
    }
}
