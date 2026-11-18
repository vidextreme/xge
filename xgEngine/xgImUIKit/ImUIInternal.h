// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "xgWindow.h"
#include "xgVec3.h"

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>

namespace xg::imui::detail
{
    inline constexpr ImU32 Accent         = IM_COL32( 71, 114, 179, 255);
    inline constexpr ImU32 AccentHover    = IM_COL32( 84, 129, 196, 255);
    inline constexpr ImU32 Selection      = IM_COL32( 51,  77, 128, 255);
    inline constexpr ImU32 ActiveObject   = IM_COL32(255, 160,  40, 255);
    inline constexpr ImU32 GapBg          = IM_COL32( 13,  13,  13, 255);
    inline constexpr ImU32 TopBarBg       = IM_COL32( 21,  21,  21, 255);
    inline constexpr ImU32 ToolbarBg      = IM_COL32( 29,  29,  29, 255);
    inline constexpr ImU32 HeaderBg       = IM_COL32( 37,  37,  37, 255);
    inline constexpr ImU32 AreaBg         = IM_COL32( 29,  29,  29, 255);
    inline constexpr ImU32 RowAltBg       = IM_COL32( 33,  33,  33, 255);
    inline constexpr ImU32 PanelBg        = IM_COL32( 31,  31,  31, 255);
    inline constexpr ImU32 PanelHeaderBg  = IM_COL32( 46,  46,  46, 255);
    inline constexpr ImU32 PanelHeaderHov = IM_COL32( 54,  54,  54, 255);
    inline constexpr ImU32 SegmentBg      = IM_COL32( 27,  27,  27, 255);
    inline constexpr ImU32 ButtonBg       = IM_COL32( 74,  74,  74, 255);
    inline constexpr ImU32 ButtonHover    = IM_COL32( 88,  88,  88, 255);
    inline constexpr ImU32 DropdownBg     = IM_COL32( 27,  27,  27, 255);
    inline constexpr ImU32 DropdownHover  = IM_COL32( 38,  38,  38, 255);
    inline constexpr ImU32 DropdownBorder = IM_COL32( 50,  50,  50, 255);
    inline constexpr ImU32 OverlayBg      = IM_COL32( 29,  29,  29, 235);
    inline constexpr ImU32 ViewportBg     = IM_COL32( 48,  48,  48, 255);
    inline constexpr ImU32 Outline        = IM_COL32( 14,  14,  14, 255);
    inline constexpr ImU32 TextColor      = IM_COL32(196, 196, 196, 255);
    inline constexpr ImU32 TextDim        = IM_COL32(140, 140, 140, 255);
    inline constexpr ImU32 TextSelected   = IM_COL32(220, 227, 238, 255);
    inline constexpr ImU32 TextOnAccent   = IM_COL32(232, 238, 250, 255);
    inline constexpr ImU32 IconColor      = IM_COL32(168, 168, 168, 255);
    inline constexpr ImU32 Grid           = IM_COL32(110, 110, 110, 255);
    inline constexpr ImU32 AxisLineX      = IM_COL32(164,  59,  59, 255);
    inline constexpr ImU32 AxisLineY      = IM_COL32( 90, 140,  42, 255);
    inline constexpr ImU32 AxisX          = IM_COL32(208,  72,  72, 255);
    inline constexpr ImU32 AxisY          = IM_COL32(111, 160,  74, 255);
    inline constexpr ImU32 AxisZ          = IM_COL32( 79, 135, 214, 255);
    inline constexpr ImU32 IconObject     = IM_COL32(217, 138,  58, 255);
    inline constexpr ImU32 IconData       = IM_COL32(106, 154,  72, 255);
    inline constexpr ImU32 IconComponent  = IM_COL32( 85, 133, 208, 255);
    inline constexpr ImU32 IconMaterial   = IM_COL32(196,  74,  74, 255);
    inline constexpr ImU32 IconWarning    = IM_COL32(214, 170,  60, 255);
    inline constexpr ImU32 Clear          = IM_COL32(  0,   0,   0,   0);
    inline constexpr ImU32 CloseHover     = IM_COL32(196,  49,  75, 255);

    inline constexpr float TopBarHeight       = 34.0f;
    inline constexpr float ToolbarHeight      = 30.0f;
            inline constexpr float HeaderHeight       = 26.0f;
    inline constexpr float AreaRounding       = 4.0f;
    inline constexpr float VTabWidth          = 28.0f;
    inline constexpr float SegButtonSize      = 20.0f;
    inline constexpr float SegmentGap         = 6.0f;
    inline constexpr float PanelHeaderHeight  = 22.0f;
    inline constexpr float PropertyLabelWidth = 92.0f;
    inline constexpr float WindowButtonWidth  = 46.0f;
    inline constexpr int   AppIconSize        = 18;

    inline bool _resetLayout = true;
    // True for the frame the dock layout was rebuilt in; panel focus requests wait until it has settled.
    inline bool _layoutJustReset = false;
    inline ImTextureData* _appIcon = nullptr;

    inline ImU32 WithAlpha(ImU32 c, float a)
    {
        return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a * 255.0f) << IM_COL32_A_SHIFT);
    }

    inline ImVec2 Offset(ImVec2 p, float x, float y) { return ImVec2(p.x + x, p.y + y); }
    inline ImVec4 ToVec4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

    enum class Icon
    {
        None,
        Hierarchy, SceneGrid, Game, Inspector, Project, Console, Lighting,
        Plus, Search, Star, Folder, Kebab, Account, Cloud, Branch, Play, Pause, Step,
        Hand, Move, Rotate, Scale, Rect, Transform,
        PivotCenter, PivotLocal, Globe, Magnet, Sphere, Wire, TwoD, Audio, Effects, EyeOff, Gizmo, Stats,
        Scene, Camera, Light, Mesh, Object, Component, Physics, Data, Material, Script,
        Info, Warning, Error,
        WinMinimize, WinMaximize, WinRestore, WinClose,
        Image, Trash,
        Cards,
        Tag, File, Filter, Sort, View,
        Lock, Unlock,
    };

    ImTextureData* CreateIconTexture(const uint8_t* rgba, int width, int height, int size);

    void ApplyTheme();
    void Arrow(ImDrawList* dl, ImVec2 from, ImVec2 to, ImU32 col, float head, float thickness = 1.5f);
    void BadgeText(ImDrawList* dl, ImVec2 c, const char* text);
    void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float s, ImU32 col);
    bool IconButton(const char* id, Icon icon, bool active = false, float size = 20.0f, ImU32 iconCol = IconColor);
    bool SearchField(const char* id, char* buffer, size_t size, float width);
    bool FlatButton(const char* label, bool active = false);
    void AlignRight(float width);

    // ImGui window name of a panel: the visible label (the title up to an optional "###"), led by spaces that leave room
    // for a tab icon when withIcon, and a stable id ("###" + the title), so the label can change without moving the
    // panel's docking state.
    std::string PanelWindowName(const char* title, bool withIcon);
    // Draws the icon on the current window's dock tab (dimmed while the tab isn't selected), or in its title bar when it
    // floats. Call right after Begin.
    void DrawPanelTabIcon(Icon icon);
    void BeginHeader();
    void EndHeader();
    void BeginBody(const char* id, ImVec2 padding = ImVec2(8.0f, 6.0f), ImGuiWindowFlags flags = 0);
    void DrawTitleBarControls(Window* window, const ImRect& bar, float dragStartX);
    void DrawMenus(Window* mainWindow);
    void DrawMainDockspace(Window* mainWindow);
    void DrawWindowFrame(Window* mainWindow);
}
