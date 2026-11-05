// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "EditorKernel.h"
#include "xgEngine.h"
#include "xgScriptHost.h"
#include "xgScriptModule.h"
#include "xgMemberCallback.h"
#include "xgEventDispatcher.h"
#include "platform/xgEventToSDL.h"
#include "xgListenerBinding.h"
#include "xgMessenger.h"
#include "xgRoute.h"
#include "xgCodecRegistry.h"
#include "xgSceneGraph.h"
#include "xgSceneGraphFactory.h"
#include "xgActorRegistry.h"

// ImGui core
#include "imgui.h"
#include "imgui_internal.h"
#include "xgEngineDef.h"
#include "xgEngineDef.generated.h"

#include "xgHierarchyNode.h"
#include "xgComponentSceneGraph.h"
#include "xgVec3.h"

#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlgpu3.h"

// SDL3 only in this CPP
#include <SDL3/SDL.h>

namespace xg
{
    //template<>
    //TypeInfo<PlayerState> TypeInfo<PlayerState>::Instance;

    namespace ui
    {
        constexpr ImU32 Accent         = IM_COL32( 71, 114, 179, 255);
        constexpr ImU32 AccentHover    = IM_COL32( 84, 129, 196, 255);
        constexpr ImU32 Selection      = IM_COL32( 51,  77, 128, 255);
        constexpr ImU32 ActiveObject   = IM_COL32(255, 160,  40, 255);
        constexpr ImU32 GapBg          = IM_COL32( 13,  13,  13, 255);
        constexpr ImU32 TopBarBg       = IM_COL32( 21,  21,  21, 255);
        constexpr ImU32 ToolbarBg      = IM_COL32( 29,  29,  29, 255);
        constexpr ImU32 HeaderBg       = IM_COL32( 37,  37,  37, 255);
        constexpr ImU32 AreaBg         = IM_COL32( 29,  29,  29, 255);
        constexpr ImU32 RowAltBg       = IM_COL32( 33,  33,  33, 255);
        constexpr ImU32 PanelBg        = IM_COL32( 31,  31,  31, 255);
        constexpr ImU32 PanelHeaderBg  = IM_COL32( 46,  46,  46, 255);
        constexpr ImU32 PanelHeaderHov = IM_COL32( 54,  54,  54, 255);
        constexpr ImU32 SegmentBg      = IM_COL32( 27,  27,  27, 255);
        constexpr ImU32 DropdownBg     = IM_COL32( 27,  27,  27, 255);
        constexpr ImU32 DropdownHover  = IM_COL32( 38,  38,  38, 255);
        constexpr ImU32 DropdownBorder = IM_COL32( 50,  50,  50, 255);
        constexpr ImU32 OverlayBg      = IM_COL32( 29,  29,  29, 235);
        constexpr ImU32 ViewportBg     = IM_COL32( 48,  48,  48, 255);
        constexpr ImU32 Outline        = IM_COL32( 14,  14,  14, 255);
        constexpr ImU32 TextColor      = IM_COL32(196, 196, 196, 255);
        constexpr ImU32 TextDim        = IM_COL32(140, 140, 140, 255);
        constexpr ImU32 TextSelected   = IM_COL32(220, 227, 238, 255);
        constexpr ImU32 TextOnAccent   = IM_COL32(232, 238, 250, 255);
        constexpr ImU32 IconColor      = IM_COL32(168, 168, 168, 255);
        constexpr ImU32 Grid           = IM_COL32(110, 110, 110, 255);
        constexpr ImU32 AxisLineX      = IM_COL32(164,  59,  59, 255);
        constexpr ImU32 AxisLineY      = IM_COL32( 90, 140,  42, 255);
        constexpr ImU32 AxisX          = IM_COL32(208,  72,  72, 255);
        constexpr ImU32 AxisY          = IM_COL32(111, 160,  74, 255);
        constexpr ImU32 AxisZ          = IM_COL32( 79, 135, 214, 255);
        constexpr ImU32 IconObject     = IM_COL32(217, 138,  58, 255);
        constexpr ImU32 IconData       = IM_COL32(106, 154,  72, 255);
        constexpr ImU32 IconComponent  = IM_COL32( 85, 133, 208, 255);
        constexpr ImU32 IconMaterial   = IM_COL32(196,  74,  74, 255);
        constexpr ImU32 IconWarning    = IM_COL32(214, 170,  60, 255);
        constexpr ImU32 Clear          = IM_COL32(  0,   0,   0,   0);
        constexpr ImU32 CloseHover     = IM_COL32(196,  49,  75, 255);

        constexpr float TopBarHeight       = 34.0f;
        constexpr float ToolbarHeight      = 30.0f;
                constexpr float HeaderHeight       = 26.0f;
        constexpr float AreaRounding       = 4.0f;
        constexpr float VTabWidth          = 28.0f;
        constexpr float SegButtonSize      = 20.0f;
        constexpr float SegmentGap         = 6.0f;
        constexpr float PanelHeaderHeight  = 22.0f;
        constexpr float PropertyLabelWidth = 92.0f;
        constexpr float WindowButtonWidth  = 46.0f;
        constexpr int   AppIconSize        = 18;

        static bool s_resetLayout = true;
        static bool s_playing = false;
        static bool s_paused = false;
        static ImTextureData* s_appIcon = nullptr;

        inline ImU32 WithAlpha(ImU32 c, float a)
        {
            return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a * 255.0f) << IM_COL32_A_SHIFT);
        }

        inline ImVec2 Offset(ImVec2 p, float x, float y) { return ImVec2(p.x + x, p.y + y); }
        inline ImVec4 ToVec4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

        static void ApplyTheme()
        {
            ImGuiStyle& s = ImGui::GetStyle();
            ImVec4* c = s.Colors;
            auto rgb = [](int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); };

            c[ImGuiCol_Text]                      = rgb(196, 196, 196);
            c[ImGuiCol_TextDisabled]              = rgb(120, 120, 120);
            c[ImGuiCol_WindowBg]                  = rgb( 29,  29,  29);
            c[ImGuiCol_ChildBg]                   = rgb(  0,   0,   0, 0.0f);
            c[ImGuiCol_PopupBg]                   = rgb( 21,  21,  21, 0.98f);
            c[ImGuiCol_Border]                    = rgb( 50,  50,  50);
            c[ImGuiCol_BorderShadow]              = rgb(  0,   0,   0, 0.0f);
            c[ImGuiCol_FrameBg]                   = rgb( 74,  74,  74);
            c[ImGuiCol_FrameBgHovered]            = rgb( 88,  88,  88);
            c[ImGuiCol_FrameBgActive]             = rgb( 29,  29,  29);
            c[ImGuiCol_TitleBg]                   = rgb( 21,  21,  21);
            c[ImGuiCol_TitleBgActive]             = rgb( 21,  21,  21);
            c[ImGuiCol_TitleBgCollapsed]          = rgb( 21,  21,  21);
            c[ImGuiCol_MenuBarBg]                 = rgb( 21,  21,  21);
            c[ImGuiCol_ScrollbarBg]               = rgb(  0,   0,   0, 0.0f);
            c[ImGuiCol_ScrollbarGrab]             = rgb( 74,  74,  74);
            c[ImGuiCol_ScrollbarGrabHovered]      = rgb( 88,  88,  88);
            c[ImGuiCol_ScrollbarGrabActive]       = rgb(101, 101, 101);
            c[ImGuiCol_CheckMark]                 = rgb(232, 238, 250);
            c[ImGuiCol_CheckboxSelectedBg]        = rgb( 71, 114, 179);
            c[ImGuiCol_SliderGrab]                = rgb( 71, 114, 179);
            c[ImGuiCol_SliderGrabActive]          = rgb( 84, 129, 196);
            c[ImGuiCol_Button]                    = rgb( 74,  74,  74);
            c[ImGuiCol_ButtonHovered]             = rgb( 88,  88,  88);
            c[ImGuiCol_ButtonActive]              = rgb( 71, 114, 179);
            c[ImGuiCol_Header]                    = rgb( 51,  77, 128);
            c[ImGuiCol_HeaderHovered]             = rgb(255, 255, 255, 0.05f);
            c[ImGuiCol_HeaderActive]              = rgb( 51,  77, 128);
            c[ImGuiCol_Separator]                 = rgb( 13,  13,  13);
            c[ImGuiCol_SeparatorHovered]          = rgb( 71, 114, 179, 0.70f);
            c[ImGuiCol_SeparatorActive]           = rgb( 84, 129, 196);
            c[ImGuiCol_ResizeGrip]                = rgb(  0,   0,   0, 0.0f);
            c[ImGuiCol_ResizeGripHovered]         = rgb( 71, 114, 179, 0.60f);
            c[ImGuiCol_ResizeGripActive]          = rgb( 84, 129, 196);
            c[ImGuiCol_InputTextCursor]           = rgb(220, 220, 220);
            c[ImGuiCol_Tab]                       = rgb( 21,  21,  21);
            c[ImGuiCol_TabHovered]                = rgb( 44,  44,  44);
            c[ImGuiCol_TabSelected]               = rgb( 37,  37,  37);
            c[ImGuiCol_TabSelectedOverline]       = rgb( 37,  37,  37);
            c[ImGuiCol_TabDimmed]                 = rgb( 21,  21,  21);
            c[ImGuiCol_TabDimmedSelected]         = rgb( 37,  37,  37);
            c[ImGuiCol_TabDimmedSelectedOverline] = rgb( 37,  37,  37);
            c[ImGuiCol_DockingPreview]            = rgb( 71, 114, 179, 0.50f);
            c[ImGuiCol_DockingEmptyBg]            = rgb( 13,  13,  13);
            c[ImGuiCol_TableHeaderBg]             = rgb( 37,  37,  37);
            c[ImGuiCol_TableBorderStrong]         = rgb( 13,  13,  13);
            c[ImGuiCol_TableBorderLight]          = rgb( 37,  37,  37);
            c[ImGuiCol_TableRowBg]                = rgb(  0,   0,   0, 0.0f);
            c[ImGuiCol_TableRowBgAlt]             = rgb(255, 255, 255, 0.012f);
            c[ImGuiCol_TextLink]                  = rgb( 96, 146, 220);
            c[ImGuiCol_TextSelectedBg]            = rgb( 71, 114, 179, 0.45f);
            c[ImGuiCol_NavCursor]                 = rgb( 84, 129, 196);

            s.WindowRounding = 0.0f;
            s.ChildRounding = 0.0f;
            s.FrameRounding = 4.0f;
            s.PopupRounding = 4.0f;
            s.GrabRounding = 4.0f;
            s.TabRounding = 4.0f;
            s.ScrollbarRounding = 8.0f;

            s.WindowBorderSize = 0.0f;
            s.ChildBorderSize = 0.0f;
            s.PopupBorderSize = 1.0f;
            s.FrameBorderSize = 0.0f;
            s.TabBorderSize = 0.0f;

            s.WindowPadding = ImVec2(8.0f, 6.0f);
            s.FramePadding = ImVec2(7.0f, 3.0f);
            s.ItemSpacing = ImVec2(6.0f, 4.0f);
            s.ItemInnerSpacing = ImVec2(5.0f, 4.0f);
            s.IndentSpacing = 14.0f;
            s.ScrollbarSize = 10.0f;
            s.GrabMinSize = 8.0f;
            s.WindowMenuButtonPosition = ImGuiDir_None;
            s.DockingSeparatorSize = 2.0f;
            s.TabBarBorderSize = 0.0f;
            s.TabBarOverlineSize = 0.0f;
        }

        static ImTextureData* CreateIconTexture(SDL_Surface* source, int size)
        {
            if (!source)
                return nullptr;
            SDL_Surface* src = SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32);
            if (!src)
                return nullptr;
            ImTextureData* texture = IM_NEW(ImTextureData)();
            texture->Create(ImTextureFormat_RGBA32, size, size);
            for (int y = 0; y < size; ++y)
            {
                const int sy0 = y * src->h / size, sy1 = ImMax(sy0 + 1, (y + 1) * src->h / size);
                unsigned char* out = static_cast<unsigned char*>(texture->GetPixelsAt(0, y));
                for (int x = 0; x < size; ++x)
                {
                    const int sx0 = x * src->w / size, sx1 = ImMax(sx0 + 1, (x + 1) * src->w / size);
                    unsigned sum = 0, count = 0;
                    for (int sy = sy0; sy < sy1; ++sy)
                    {
                        const Uint8* in = static_cast<const Uint8*>(src->pixels) + sy * src->pitch;
                        for (int sx = sx0; sx < sx1; ++sx, ++count)
                        {
                            const Uint8* px = in + sx * 4;
                            sum += (px[0] * 54u + px[1] * 183u + px[2] * 19u) >> 8;
                        }
                    }
                    out[x * 4 + 0] = out[x * 4 + 1] = out[x * 4 + 2] = 255;
                    out[x * 4 + 3] = static_cast<unsigned char>(sum / count);
                }
            }
            SDL_DestroySurface(src);
            ImGui::RegisterUserTexture(texture);
            return texture;
        }

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
        };

        static void Arrow(ImDrawList* dl, ImVec2 from, ImVec2 to, ImU32 col, float head, float thickness = 1.5f)
        {
            dl->AddLine(from, to, col, thickness);
            float dx = to.x - from.x, dy = to.y - from.y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 1e-3f)
                return;
            dx /= len; dy /= len;
            ImVec2 l(to.x - dx * head - dy * head * 0.6f, to.y - dy * head + dx * head * 0.6f);
            ImVec2 r(to.x - dx * head + dy * head * 0.6f, to.y - dy * head - dx * head * 0.6f);
            dl->AddTriangleFilled(to, l, r, col);
        }

        static void BadgeText(ImDrawList* dl, ImVec2 c, const char* text)
        {
            ImVec2 ts = ImGui::CalcTextSize(text);
            dl->AddText(ImVec2(ImFloor(c.x - ts.x * 0.5f), ImFloor(c.y - ts.y * 0.5f)), IM_COL32(24, 24, 24, 255), text);
        }

        static void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float s, ImU32 col)
        {
            const float h = s * 0.5f;
            switch (icon)
            {
            case Icon::None:
                break;
            case Icon::Hierarchy:
                for (int i = 0; i < 3; ++i)
                {
                    float y = c.y - h * 0.6f + i * h * 0.6f;
                    float x = c.x - h * 0.7f + (i == 0 ? 0.0f : h * 0.35f);
                    dl->AddRectFilled(ImVec2(x, y - 1.5f), ImVec2(x + 3.0f, y + 1.5f), col);
                    dl->AddLine(ImVec2(x + 5.0f, y), ImVec2(c.x + h * 0.8f, y), col, 1.5f);
                }
                break;
            case Icon::SceneGrid:
                for (int i = -1; i <= 1; i += 2)
                {
                    dl->AddLine(ImVec2(c.x + i * h * 0.3f, c.y - h * 0.8f), ImVec2(c.x + i * h * 0.3f, c.y + h * 0.8f), col, 1.3f);
                    dl->AddLine(ImVec2(c.x - h * 0.8f, c.y + i * h * 0.3f), ImVec2(c.x + h * 0.8f, c.y + i * h * 0.3f), col, 1.3f);
                }
                break;
            case Icon::Game:
                dl->AddRect(ImVec2(c.x - h * 0.9f, c.y - h * 0.5f), ImVec2(c.x + h * 0.9f, c.y + h * 0.55f), col, h * 0.4f, 0, 1.3f);
                dl->AddLine(ImVec2(c.x - h * 0.65f, c.y), ImVec2(c.x - h * 0.2f, c.y), col, 1.3f);
                dl->AddLine(ImVec2(c.x - h * 0.42f, c.y - h * 0.23f), ImVec2(c.x - h * 0.42f, c.y + h * 0.23f), col, 1.3f);
                dl->AddCircleFilled(ImVec2(c.x + h * 0.3f, c.y + h * 0.05f), 1.4f, col);
                dl->AddCircleFilled(ImVec2(c.x + h * 0.55f, c.y - h * 0.15f), 1.4f, col);
                break;
            case Icon::Inspector:
            case Icon::Stats:
                dl->AddCircle(c, h * 0.82f, col, 16, 1.3f);
                dl->AddCircleFilled(ImVec2(c.x, c.y - h * 0.38f), 1.2f, col);
                dl->AddLine(ImVec2(c.x, c.y - h * 0.1f), ImVec2(c.x, c.y + h * 0.45f), col, 1.5f);
                break;
            case Icon::Project:
            case Icon::Folder:
                dl->AddRectFilled(ImVec2(c.x - h * 0.85f, c.y - h * 0.65f), ImVec2(c.x - h * 0.1f, c.y - h * 0.3f), col, 1.0f);
                dl->AddRectFilled(ImVec2(c.x - h * 0.85f, c.y - h * 0.45f), ImVec2(c.x + h * 0.85f, c.y + h * 0.65f), col, 1.5f);
                break;
            case Icon::Console:
                dl->AddRect(ImVec2(c.x - h * 0.85f, c.y - h * 0.7f), ImVec2(c.x + h * 0.85f, c.y + h * 0.7f), col, 1.5f, 0, 1.2f);
                dl->AddLine(ImVec2(c.x - h * 0.5f, c.y - h * 0.3f), ImVec2(c.x - h * 0.15f, c.y), col, 1.3f);
                dl->AddLine(ImVec2(c.x - h * 0.15f, c.y), ImVec2(c.x - h * 0.5f, c.y + h * 0.3f), col, 1.3f);
                dl->AddLine(ImVec2(c.x + h * 0.05f, c.y + h * 0.35f), ImVec2(c.x + h * 0.5f, c.y + h * 0.35f), col, 1.3f);
                break;
            case Icon::Lighting:
            case Icon::Light:
                dl->AddCircleFilled(c, h * 0.35f, col);
                for (int i = 0; i < 8; ++i)
                {
                    float a = i * (IM_PI / 4.0f);
                    dl->AddLine(ImVec2(c.x + cosf(a) * h * 0.55f, c.y + sinf(a) * h * 0.55f),
                                ImVec2(c.x + cosf(a) * h * 0.9f, c.y + sinf(a) * h * 0.9f), col, 1.2f);
                }
                break;
            case Icon::Plus:
                dl->AddLine(ImVec2(c.x - h * 0.7f, c.y), ImVec2(c.x + h * 0.7f, c.y), col, 1.5f);
                dl->AddLine(ImVec2(c.x, c.y - h * 0.7f), ImVec2(c.x, c.y + h * 0.7f), col, 1.5f);
                break;
            case Icon::Search:
                dl->AddCircle(ImVec2(c.x - h * 0.15f, c.y - h * 0.15f), h * 0.5f, col, 12, 1.5f);
                dl->AddLine(ImVec2(c.x + h * 0.2f, c.y + h * 0.2f), ImVec2(c.x + h * 0.75f, c.y + h * 0.75f), col, 2.0f);
                break;
            case Icon::Star:
            {
                ImVec2 pts[10];
                for (int i = 0; i < 10; ++i)
                {
                    float a = -IM_PI * 0.5f + i * IM_PI / 5.0f;
                    float r = (i % 2) ? h * 0.4f : h * 0.9f;
                    pts[i] = ImVec2(c.x + cosf(a) * r, c.y + sinf(a) * r + h * 0.08f);
                }
                dl->AddPolyline(pts, 10, col, ImDrawFlags_Closed, 1.2f);
                break;
            }
            case Icon::Kebab:
                for (int i = -1; i <= 1; ++i)
                    dl->AddCircleFilled(ImVec2(c.x, c.y + i * h * 0.55f), 1.3f, col);
                break;
            case Icon::Account:
                dl->AddCircle(c, h * 0.85f, col, 20, 1.2f);
                dl->AddCircleFilled(ImVec2(c.x, c.y - h * 0.22f), h * 0.28f, col);
                dl->PathArcTo(ImVec2(c.x, c.y + h * 0.62f), h * 0.48f, IM_PI * 1.15f, IM_PI * 1.85f, 10);
                dl->PathStroke(col, 0, 1.3f);
                break;
            case Icon::Cloud:
                dl->AddCircleFilled(ImVec2(c.x - h * 0.4f, c.y + h * 0.12f), h * 0.38f, col);
                dl->AddCircleFilled(ImVec2(c.x + h * 0.05f, c.y - h * 0.12f), h * 0.5f, col);
                dl->AddCircleFilled(ImVec2(c.x + h * 0.5f, c.y + h * 0.15f), h * 0.35f, col);
                dl->AddRectFilled(ImVec2(c.x - h * 0.4f, c.y + h * 0.1f), ImVec2(c.x + h * 0.5f, c.y + h * 0.5f), col);
                break;
            case Icon::Branch:
                dl->AddLine(ImVec2(c.x - h * 0.4f, c.y - h * 0.45f), ImVec2(c.x - h * 0.4f, c.y + h * 0.45f), col, 1.3f);
                dl->AddBezierQuadratic(ImVec2(c.x + h * 0.45f, c.y - h * 0.15f), ImVec2(c.x + h * 0.45f, c.y + h * 0.3f),
                                       ImVec2(c.x - h * 0.4f, c.y + h * 0.3f), col, 1.3f);
                dl->AddCircle(ImVec2(c.x - h * 0.4f, c.y - h * 0.62f), h * 0.18f, col, 8, 1.2f);
                dl->AddCircle(ImVec2(c.x - h * 0.4f, c.y + h * 0.62f), h * 0.18f, col, 8, 1.2f);
                dl->AddCircle(ImVec2(c.x + h * 0.45f, c.y - h * 0.32f), h * 0.18f, col, 8, 1.2f);
                break;
            case Icon::Play:
                dl->AddTriangleFilled(ImVec2(c.x - h * 0.5f, c.y - h * 0.7f), ImVec2(c.x + h * 0.7f, c.y), ImVec2(c.x - h * 0.5f, c.y + h * 0.7f), col);
                break;
            case Icon::Pause:
                dl->AddRectFilled(ImVec2(c.x - h * 0.55f, c.y - h * 0.65f), ImVec2(c.x - h * 0.15f, c.y + h * 0.65f), col);
                dl->AddRectFilled(ImVec2(c.x + h * 0.15f, c.y - h * 0.65f), ImVec2(c.x + h * 0.55f, c.y + h * 0.65f), col);
                break;
            case Icon::Step:
                dl->AddRectFilled(ImVec2(c.x + h * 0.4f, c.y - h * 0.65f), ImVec2(c.x + h * 0.65f, c.y + h * 0.65f), col);
                dl->AddTriangleFilled(ImVec2(c.x - h * 0.65f, c.y - h * 0.65f), ImVec2(c.x + h * 0.3f, c.y), ImVec2(c.x - h * 0.65f, c.y + h * 0.65f), col);
                break;
            case Icon::Hand:
                dl->AddRectFilled(ImVec2(c.x - h * 0.5f, c.y - h * 0.1f), ImVec2(c.x + h * 0.5f, c.y + h * 0.8f), col, h * 0.3f);
                for (int i = 0; i < 4; ++i)
                {
                    float x = c.x - h * 0.375f + i * h * 0.25f;
                    float top = c.y - h * ((i == 1 || i == 2) ? 0.85f : 0.65f);
                    dl->AddLine(ImVec2(x, c.y), ImVec2(x, top), col, h * 0.2f);
                }
                dl->AddLine(ImVec2(c.x - h * 0.45f, c.y + h * 0.3f), ImVec2(c.x - h * 0.85f, c.y - h * 0.05f), col, h * 0.2f);
                break;
            case Icon::Move:
                Arrow(dl, c, ImVec2(c.x + h * 0.9f, c.y), col, h * 0.35f);
                Arrow(dl, c, ImVec2(c.x - h * 0.9f, c.y), col, h * 0.35f);
                Arrow(dl, c, ImVec2(c.x, c.y + h * 0.9f), col, h * 0.35f);
                Arrow(dl, c, ImVec2(c.x, c.y - h * 0.9f), col, h * 0.35f);
                break;
            case Icon::Rotate:
                dl->PathArcTo(c, h * 0.7f, IM_PI * 0.15f, IM_PI * 1.75f, 16);
                dl->PathStroke(col, 0, 1.6f);
                Arrow(dl, ImVec2(c.x + cosf(IM_PI * 1.6f) * h * 0.7f, c.y + sinf(IM_PI * 1.6f) * h * 0.7f),
                          ImVec2(c.x + cosf(IM_PI * 1.78f) * h * 0.72f, c.y + sinf(IM_PI * 1.78f) * h * 0.72f), col, h * 0.4f);
                break;
            case Icon::Scale:
                dl->AddRect(ImVec2(c.x - h * 0.85f, c.y - h * 0.2f), ImVec2(c.x + h * 0.2f, c.y + h * 0.85f), col, 1.0f, 0, 1.3f);
                Arrow(dl, ImVec2(c.x - h * 0.1f, c.y + h * 0.1f), ImVec2(c.x + h * 0.85f, c.y - h * 0.85f), col, h * 0.4f);
                break;
            case Icon::Rect:
            {
                ImVec2 a(c.x - h * 0.7f, c.y - h * 0.55f), b(c.x + h * 0.7f, c.y + h * 0.55f);
                dl->AddRect(a, b, col, 0.0f, 0, 1.2f);
                const ImVec2 corners[4] = { a, ImVec2(b.x, a.y), b, ImVec2(a.x, b.y) };
                for (const ImVec2& k : corners)
                    dl->AddRectFilled(ImVec2(k.x - 1.5f, k.y - 1.5f), ImVec2(k.x + 1.5f, k.y + 1.5f), col);
                break;
            }
            case Icon::Transform:
                DrawIcon(dl, Icon::Move, c, s * 0.8f, col);
                dl->AddCircle(c, h * 0.95f, col, 16, 1.2f);
                break;
            case Icon::PivotCenter:
                dl->AddCircle(c, h * 0.62f, col, 16, 1.3f);
                dl->AddCircleFilled(c, h * 0.2f, col);
                for (int i = 0; i < 4; ++i)
                {
                    float a = i * IM_PI * 0.5f;
                    dl->AddLine(ImVec2(c.x + cosf(a) * h * 0.62f, c.y + sinf(a) * h * 0.62f),
                                ImVec2(c.x + cosf(a) * h * 0.95f, c.y + sinf(a) * h * 0.95f), col, 1.3f);
                }
                break;
            case Icon::PivotLocal:
            {
                ImVec2 o(c.x - h * 0.45f, c.y + h * 0.45f);
                Arrow(dl, o, ImVec2(o.x + h * 1.25f, o.y), col, h * 0.3f, 1.3f);
                Arrow(dl, o, ImVec2(o.x, o.y - h * 1.25f), col, h * 0.3f, 1.3f);
                dl->AddCircleFilled(o, h * 0.22f, col);
                break;
            }
            case Icon::Globe:
                dl->AddCircle(c, h * 0.8f, col, 16, 1.3f);
                dl->AddLine(ImVec2(c.x - h * 0.8f, c.y), ImVec2(c.x + h * 0.8f, c.y), col, 1.0f);
                dl->AddEllipse(c, ImVec2(h * 0.35f, h * 0.8f), col, 0.0f, 16, 1.0f);
                break;
            case Icon::Magnet:
                dl->PathArcTo(ImVec2(c.x, c.y + h * 0.05f), h * 0.55f, 0.0f, IM_PI, 12);
                dl->PathStroke(col, 0, h * 0.32f);
                dl->AddLine(ImVec2(c.x - h * 0.55f, c.y + h * 0.05f), ImVec2(c.x - h * 0.55f, c.y - h * 0.75f), col, h * 0.32f);
                dl->AddLine(ImVec2(c.x + h * 0.55f, c.y + h * 0.05f), ImVec2(c.x + h * 0.55f, c.y - h * 0.75f), col, h * 0.32f);
                break;
            case Icon::Sphere:
                dl->AddCircleFilled(c, h * 0.78f, WithAlpha(col, 0.85f), 20);
                dl->AddCircleFilled(ImVec2(c.x - h * 0.25f, c.y - h * 0.25f), h * 0.25f, WithAlpha(IM_COL32_WHITE, 0.45f), 12);
                break;
            case Icon::Wire:
                dl->AddCircle(c, h * 0.75f, col, 16, 1.2f);
                dl->AddEllipse(c, ImVec2(h * 0.75f, h * 0.3f), col, 0.0f, 16, 1.0f);
                dl->AddEllipse(c, ImVec2(h * 0.3f, h * 0.75f), col, 0.0f, 16, 1.0f);
                break;
            case Icon::TwoD:
            {
                ImFont* bold = ImGui::GetIO().Fonts->Fonts.Size > 2 ? ImGui::GetIO().Fonts->Fonts[2] : ImGui::GetFont();
                const float size = 11.0f;
                ImVec2 ts = bold->CalcTextSizeA(size, FLT_MAX, 0.0f, "2D");
                dl->AddText(bold, size, ImVec2(ImFloor(c.x - ts.x * 0.5f), ImFloor(c.y - ts.y * 0.5f)), col, "2D");
                break;
            }
            case Icon::Audio:
                dl->AddRectFilled(ImVec2(c.x - h * 0.8f, c.y - h * 0.25f), ImVec2(c.x - h * 0.45f, c.y + h * 0.25f), col);
                dl->AddTriangleFilled(ImVec2(c.x - h * 0.5f, c.y - h * 0.25f), ImVec2(c.x + h * 0.05f, c.y - h * 0.7f), ImVec2(c.x + h * 0.05f, c.y + h * 0.7f), col);
                dl->AddTriangleFilled(ImVec2(c.x - h * 0.5f, c.y - h * 0.25f), ImVec2(c.x + h * 0.05f, c.y + h * 0.7f), ImVec2(c.x - h * 0.5f, c.y + h * 0.25f), col);
                dl->PathArcTo(ImVec2(c.x + h * 0.05f, c.y), h * 0.45f, -IM_PI * 0.3f, IM_PI * 0.3f, 6);
                dl->PathStroke(col, 0, 1.2f);
                dl->PathArcTo(ImVec2(c.x + h * 0.05f, c.y), h * 0.8f, -IM_PI * 0.3f, IM_PI * 0.3f, 8);
                dl->PathStroke(col, 0, 1.2f);
                break;
            case Icon::Effects:
            {
                auto sparkle = [&](ImVec2 p, float r)
                {
                    dl->AddQuadFilled(ImVec2(p.x, p.y - r), ImVec2(p.x + r * 0.22f, p.y), ImVec2(p.x, p.y + r), ImVec2(p.x - r * 0.22f, p.y), col);
                    dl->AddQuadFilled(ImVec2(p.x - r, p.y), ImVec2(p.x, p.y - r * 0.22f), ImVec2(p.x + r, p.y), ImVec2(p.x, p.y + r * 0.22f), col);
                };
                sparkle(ImVec2(c.x - h * 0.15f, c.y + h * 0.1f), h * 0.75f);
                sparkle(ImVec2(c.x + h * 0.6f, c.y - h * 0.55f), h * 0.35f);
                break;
            }
            case Icon::EyeOff:
                dl->PathArcTo(ImVec2(c.x, c.y + h * 0.75f), h * 1.05f, IM_PI * 1.22f, IM_PI * 1.78f, 12);
                dl->PathArcTo(ImVec2(c.x, c.y - h * 0.75f), h * 1.05f, IM_PI * 0.22f, IM_PI * 0.78f, 12);
                dl->PathStroke(col, ImDrawFlags_Closed, 1.2f);
                dl->AddCircleFilled(c, h * 0.28f, col);
                dl->AddLine(ImVec2(c.x - h * 0.75f, c.y + h * 0.75f), ImVec2(c.x + h * 0.75f, c.y - h * 0.75f), col, 1.4f);
                break;
            case Icon::Gizmo:
            {
                ImVec2 o(c.x - h * 0.45f, c.y + h * 0.4f);
                Arrow(dl, o, ImVec2(o.x + h * 1.3f, o.y), col, h * 0.3f, 1.2f);
                Arrow(dl, o, ImVec2(o.x, o.y - h * 1.3f), col, h * 0.3f, 1.2f);
                Arrow(dl, o, ImVec2(o.x + h * 0.9f, o.y - h * 0.9f), col, h * 0.3f, 1.2f);
                break;
            }
            case Icon::Scene:
                dl->AddCircleFilled(ImVec2(c.x - h * 0.3f, c.y + h * 0.3f), h * 0.45f, col);
                dl->AddTriangleFilled(ImVec2(c.x + h * 0.3f, c.y - h * 0.85f), ImVec2(c.x - h * 0.2f, c.y), ImVec2(c.x + h * 0.85f, c.y), col);
                break;
            case Icon::Camera:
                dl->AddRectFilled(ImVec2(c.x - h * 0.85f, c.y - h * 0.45f), ImVec2(c.x + h * 0.3f, c.y + h * 0.45f), col, 1.5f);
                dl->AddTriangleFilled(ImVec2(c.x + h * 0.3f, c.y), ImVec2(c.x + h * 0.9f, c.y - h * 0.45f), ImVec2(c.x + h * 0.9f, c.y + h * 0.45f), col);
                break;
            case Icon::Mesh:
            {
                ImVec2 top(c.x, c.y - h * 0.9f), l(c.x - h * 0.8f, c.y - h * 0.45f), r(c.x + h * 0.8f, c.y - h * 0.45f);
                ImVec2 m(c.x, c.y), bl(c.x - h * 0.8f, c.y + h * 0.45f), br(c.x + h * 0.8f, c.y + h * 0.45f), b(c.x, c.y + h * 0.9f);
                dl->AddQuad(top, r, m, l, col, 1.2f);
                dl->AddQuad(l, m, b, bl, col, 1.2f);
                dl->AddQuad(m, r, br, b, col, 1.2f);
                break;
            }
            case Icon::Object:
                dl->AddRectFilled(ImVec2(c.x - h * 0.6f, c.y - h * 0.6f), ImVec2(c.x + h * 0.6f, c.y + h * 0.6f), col, 2.0f);
                break;
            case Icon::Component:
                dl->AddLine(ImVec2(c.x - h * 0.7f, c.y + h * 0.7f), ImVec2(c.x + h * 0.3f, c.y - h * 0.3f), col, 3.0f);
                dl->AddCircle(ImVec2(c.x + h * 0.4f, c.y - h * 0.4f), h * 0.4f, col, 10, 2.0f);
                break;
            case Icon::Physics:
                dl->AddCircle(ImVec2(c.x, c.y - h * 0.2f), h * 0.45f, col, 12, 1.4f);
                Arrow(dl, ImVec2(c.x, c.y + h * 0.1f), ImVec2(c.x, c.y + h * 0.9f), col, h * 0.3f);
                break;
            case Icon::Data:
                dl->AddTriangle(ImVec2(c.x, c.y - h * 0.8f), ImVec2(c.x - h * 0.8f, c.y + h * 0.6f), ImVec2(c.x + h * 0.8f, c.y + h * 0.6f), col, 1.5f);
                break;
            case Icon::Material:
                dl->AddCircleFilled(c, h * 0.75f, col);
                dl->AddCircleFilled(ImVec2(c.x - h * 0.25f, c.y - h * 0.25f), h * 0.22f, WithAlpha(IM_COL32_WHITE, 0.6f));
                break;
            case Icon::Script:
                dl->AddLine(ImVec2(c.x - h * 0.35f, c.y - h * 0.5f), ImVec2(c.x - h * 0.85f, c.y), col, 1.4f);
                dl->AddLine(ImVec2(c.x - h * 0.85f, c.y), ImVec2(c.x - h * 0.35f, c.y + h * 0.5f), col, 1.4f);
                dl->AddLine(ImVec2(c.x + h * 0.35f, c.y - h * 0.5f), ImVec2(c.x + h * 0.85f, c.y), col, 1.4f);
                dl->AddLine(ImVec2(c.x + h * 0.85f, c.y), ImVec2(c.x + h * 0.35f, c.y + h * 0.5f), col, 1.4f);
                dl->AddLine(ImVec2(c.x + h * 0.15f, c.y - h * 0.7f), ImVec2(c.x - h * 0.15f, c.y + h * 0.7f), col, 1.2f);
                break;
            case Icon::Info:
                dl->AddCircleFilled(c, h * 0.85f, col, 16);
                BadgeText(dl, c, "i");
                break;
            case Icon::Warning:
                dl->AddTriangleFilled(ImVec2(c.x, c.y - h * 0.9f), ImVec2(c.x + h * 0.95f, c.y + h * 0.75f), ImVec2(c.x - h * 0.95f, c.y + h * 0.75f), col);
                BadgeText(dl, ImVec2(c.x, c.y + h * 0.1f), "!");
                break;
            case Icon::Error:
                dl->AddCircleFilled(c, h * 0.85f, col, 16);
                BadgeText(dl, c, "!");
                break;
            case Icon::WinMinimize:
                dl->AddLine(ImVec2(ImFloor(c.x - h) + 0.5f, ImFloor(c.y) + 0.5f), ImVec2(ImFloor(c.x + h) + 0.5f, ImFloor(c.y) + 0.5f), col, 1.0f);
                break;
            case Icon::WinMaximize:
                dl->AddRect(ImVec2(ImFloor(c.x - h) + 0.5f, ImFloor(c.y - h) + 0.5f), ImVec2(ImFloor(c.x + h) + 0.5f, ImFloor(c.y + h) + 0.5f), col, 1.0f, 0, 1.0f);
                break;
            case Icon::WinRestore:
            {
                const float o = ImFloor(h * 0.4f);
                ImVec2 a(ImFloor(c.x - h) + 0.5f, ImFloor(c.y - h + o) + 0.5f), b(ImFloor(c.x + h - o) + 0.5f, ImFloor(c.y + h) + 0.5f);
                dl->AddRect(a, b, col, 1.0f, 0, 1.0f);
                dl->AddLine(ImVec2(a.x + o, a.y), ImVec2(a.x + o, a.y - o), col, 1.0f);
                dl->AddLine(ImVec2(a.x + o, a.y - o), ImVec2(b.x + o, a.y - o), col, 1.0f);
                dl->AddLine(ImVec2(b.x + o, a.y - o), ImVec2(b.x + o, b.y - o), col, 1.0f);
                dl->AddLine(ImVec2(b.x + o, b.y - o), ImVec2(b.x, b.y - o), col, 1.0f);
                break;
            }
            case Icon::WinClose:
                dl->AddLine(ImVec2(c.x - h, c.y - h), ImVec2(c.x + h, c.y + h), col, 1.1f);
                dl->AddLine(ImVec2(c.x + h, c.y - h), ImVec2(c.x - h, c.y + h), col, 1.1f);
                break;
            }
        }

        static bool IconButton(const char* id, Icon icon, bool active = false, float size = 20.0f, ImU32 iconCol = IconColor)
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

        static bool StateButton(const char* id, Icon icon, bool on, const char* tip, float size, bool hasMenu = false)
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

        static bool PressedMenuCorner()
        {
            return ImGui::GetIO().MouseClickedPos[0].x >= ImGui::GetItemRectMax().x - 7.0f;
        }

        static ImVec2 s_segmentStart;
        static int s_segmentIndex = 0;

        inline float SegmentWidth(int count) { return count * SegButtonSize + (count + 1) * 1.0f; }

        static void BeginSegment(int count, ImU32 bg = SegmentBg)
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, p.y - 1.0f), ImVec2(p.x + SegmentWidth(count), p.y + SegButtonSize + 1.0f), bg, 4.0f);
            ImGui::SetCursorScreenPos(ImVec2(p.x + 1.0f, p.y));
            s_segmentStart = p;
            s_segmentIndex = 0;
        }

        static bool SegButton(const char* id, Icon icon, bool on, const char* tip, bool hasMenu = false)
        {
            if (s_segmentIndex++ > 0)
                ImGui::SameLine(0.0f, 1.0f);
            return StateButton(id, icon, on, tip, SegButtonSize, hasMenu);
        }

        static void EndSegment()
        {
            ImGui::SameLine(0.0f, 1.0f + SegmentGap);
        }

        static bool BeginAnchoredPopup(const char* id, bool openNow, ImVec2 pos)
        {
            ImGuiID popupId = ImGui::GetID(id);
            if (openNow)
                ImGui::OpenPopupEx(popupId);
            if (!ImGui::IsPopupOpen(popupId, ImGuiPopupFlags_None))
                return false;
            ImGui::SetNextWindowPos(pos);
            return ImGui::BeginPopupEx(popupId, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
        }

        inline ImVec2 BelowLastItem() { return ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 3.0f); }

        static float DropdownWidth(const char* label, Icon icon = Icon::None)
        {
            return ImGui::CalcTextSize(label, nullptr, true).x + (icon != Icon::None ? 18.0f : 0.0f) + 26.0f;
        }

        static bool BeginDropdown(const char* id, const char* label, Icon icon = Icon::None, float width = 0.0f, ImU32 iconCol = IconColor)
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

        static void SearchField(const char* id, char* buffer, size_t size, float width)
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

        static bool FlatButton(const char* label, bool active = false)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, active ? Accent : Clear);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? AccentHover : WithAlpha(IM_COL32_WHITE, 0.07f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Accent);
            bool pressed = ImGui::Button(label);
            ImGui::PopStyleColor(3);
            return pressed;
        }

        static void AlignRight(float width)
        {
            float slack = ImGui::GetContentRegionAvail().x - width;
            if (slack > 0.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + slack);
        }

        static int s_selectDefaultTabs = 0;

        static bool BeginEditor(const char* name, ImU32 bodyCol, bool defaultTab = false)
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
            if (defaultTab && s_selectDefaultTabs > 0 && window->DockNode && window->DockNode->TabBar)
                window->DockNode->TabBar->NextSelectedTabId = window->TabId;
            if (open)
            {
                const ImRect r = ImGui::GetCurrentWindow()->InnerRect;
                ImGui::GetWindowDrawList()->AddRectFilled(r.Min, r.Max, bodyCol, AreaRounding, ImDrawFlags_RoundCornersBottom);
            }
            return open;
        }

        static void BeginHeader()
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, HeaderBg);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 3.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
            ImGui::BeginChild("##header", ImVec2(0.0f, HeaderHeight), ImGuiChildFlags_AlwaysUseWindowPadding,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        }

        static void EndHeader()
        {
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }

        static void BeginBody(const char* id, ImVec2 padding = ImVec2(8.0f, 6.0f), ImGuiWindowFlags flags = 0)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
            ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding, flags);
            ImGui::PopStyleVar();
        }

        static bool VerticalIconTabs(const Icon* icons, const ImU32* colors, const char* const* tips, int count, int& active)
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

        static bool BeginPanel(const char* title, Icon icon, ImU32 iconCol, bool defaultOpen = true)
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

        static void EndPanel()
        {
            ImGui::EndChild();
            ImGui::PopID();
            ImGui::Dummy(ImVec2(0.0f, 1.0f));
        }

        static void PropertyLabel(const char* label, float labelWidth = PropertyLabelWidth)
        {
            const float x0 = ImGui::GetCursorPosX();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ToVec4(TextDim), "%s", label);
            ImGui::SameLine(x0 + labelWidth);
            ImGui::SetNextItemWidth(-FLT_MIN);
        }

        static void PropertyVec3(const char* name, float* v, float speed, const char* format = "%.2f")
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

        struct ViewCamera
        {
            Vec3 Eye;
            Vec3 Right;
            Vec3 Up;
            Vec3 Forward;
            ImVec2 Center;
            float Focal = 1.0f;

            static ViewCamera LookAt(const Vec3& eye, const Vec3& target, ImVec2 center, float viewHeight, float fovY)
            {
                ViewCamera cam;
                cam.Eye = eye;
                cam.Forward = (target - eye).Normalized();
                cam.Right = cam.Forward.Cross(Vec3(0.0f, 0.0f, 1.0f)).Normalized();
                cam.Up = cam.Right.Cross(cam.Forward);
                cam.Center = center;
                cam.Focal = 0.5f * viewHeight / tanf(fovY * 0.5f);
                return cam;
            }

            Vec3 ToView(const Vec3& p) const
            {
                Vec3 d = p - Eye;
                return Vec3(d.Dot(Right), d.Dot(Up), d.Dot(Forward));
            }

            ImVec2 ProjectView(const Vec3& v) const
            {
                return ImVec2(Center.x + v.x / v.z * Focal, Center.y - v.y / v.z * Focal);
            }

            bool Project(const Vec3& p, ImVec2& out) const
            {
                Vec3 v = ToView(p);
                if (v.z < 0.1f)
                    return false;
                out = ProjectView(v);
                return true;
            }

            void Line(ImDrawList* dl, const Vec3& a, const Vec3& b, ImU32 col, float thickness) const
            {
                const float nearZ = 0.1f;
                Vec3 va = ToView(a), vb = ToView(b);
                if (va.z < nearZ && vb.z < nearZ)
                    return;
                if (va.z < nearZ)
                    va = va + (vb - va) * ((nearZ - va.z) / (vb.z - va.z));
                else if (vb.z < nearZ)
                    vb = vb + (va - vb) * ((nearZ - vb.z) / (va.z - vb.z));
                dl->AddLine(ProjectView(va), ProjectView(vb), col, thickness);
            }
        };

        static void DrawGrid(ImDrawList* dl, const ViewCamera& cam)
        {
            const int extent = 20;
            for (int i = -extent; i <= extent; ++i)
            {
                for (int s = -extent; s < extent; s += 2)
                {
                    float d = sqrtf(static_cast<float>(i * i + (s + 1) * (s + 1)));
                    float alpha = ImClamp(1.0f - d / extent, 0.0f, 1.0f) * 0.35f;
                    if (alpha <= 0.01f)
                        continue;
                    ImU32 col = WithAlpha(Grid, alpha);
                    float fi = static_cast<float>(i), fs = static_cast<float>(s);
                    if (i != 0)
                    {
                        cam.Line(dl, Vec3(fi, fs, 0.0f), Vec3(fi, fs + 2.0f, 0.0f), col, 1.0f);
                        cam.Line(dl, Vec3(fs, fi, 0.0f), Vec3(fs + 2.0f, fi, 0.0f), col, 1.0f);
                    }
                }
            }
            cam.Line(dl, Vec3(-50.0f, 0.0f, 0.0f), Vec3(50.0f, 0.0f, 0.0f), WithAlpha(AxisLineX, 0.85f), 1.5f);
            cam.Line(dl, Vec3(0.0f, -50.0f, 0.0f), Vec3(0.0f, 50.0f, 0.0f), WithAlpha(AxisLineY, 0.85f), 1.5f);
        }

        enum class DrawMode { Shaded, Wireframe, ShadedWireframe };

        static void DrawCube(ImDrawList* dl, const ViewCamera& cam, bool selected, DrawMode mode = DrawMode::Shaded, bool lit = true)
        {
            Vec3 v[8];
            for (int i = 0; i < 8; ++i)
                v[i] = Vec3((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f);

            static const int faces[6][4] = { {0,2,6,4}, {1,5,7,3}, {0,4,5,1}, {2,3,7,6}, {0,1,3,2}, {4,6,7,5} };
            static const Vec3 normals[6] = { Vec3(-1,0,0), Vec3(1,0,0), Vec3(0,-1,0), Vec3(0,1,0), Vec3(0,0,-1), Vec3(0,0,1) };
            const Vec3 lightDir = Vec3(0.4f, -0.3f, 0.85f).Normalized();

            if (mode == DrawMode::Wireframe)
            {
                for (int a = 0; a < 8; ++a)
                    for (int b = a + 1; b < 8; ++b)
                        if (((a ^ b) & ((a ^ b) - 1)) == 0)
                            cam.Line(dl, v[a], v[b], selected ? ActiveObject : IM_COL32(20, 20, 20, 255), 1.2f);
                return;
            }

            int edgeVisibleCount[8][8] = {};
            for (int f = 0; f < 6; ++f)
            {
                Vec3 center = (v[faces[f][0]] + v[faces[f][2]]) * 0.5f;
                if (normals[f].Dot(cam.Eye - center) <= 0.0f)
                    continue;

                ImVec2 pts[4];
                bool ok = true;
                for (int k = 0; k < 4; ++k)
                    ok &= cam.Project(v[faces[f][k]], pts[k]);
                if (!ok)
                    continue;

                float area = 0.0f;
                for (int k = 0; k < 4; ++k)
                    area += pts[k].x * pts[(k + 1) % 4].y - pts[(k + 1) % 4].x * pts[k].y;
                if (area < 0.0f)
                    ImSwap(pts[1], pts[3]);

                float shade = lit ? 0.45f + 0.55f * ImMax(0.0f, normals[f].Dot(lightDir)) : 0.8f;
                int g = static_cast<int>(182 * shade);
                dl->AddConvexPolyFilled(pts, 4, IM_COL32(g, g, g, 255));
                dl->AddPolyline(pts, 4, mode == DrawMode::ShadedWireframe ? IM_COL32(20, 20, 20, 255) : WithAlpha(Outline, 0.35f),
                    ImDrawFlags_Closed, 1.0f);

                for (int k = 0; k < 4; ++k)
                {
                    int a = faces[f][k], b = faces[f][(k + 1) % 4];
                    edgeVisibleCount[ImMin(a, b)][ImMax(a, b)]++;
                }
            }

            if (selected)
            {
                for (int a = 0; a < 8; ++a)
                    for (int b = a + 1; b < 8; ++b)
                        if (edgeVisibleCount[a][b] == 1)
                            cam.Line(dl, v[a], v[b], ActiveObject, 2.0f);
            }
        }

        static void DrawGroundPlane(ImDrawList* dl, const ViewCamera& cam, float z, float extent, ImU32 col)
        {
            const Vec3 corners[4] = { Vec3(-extent, -extent, z), Vec3(extent, -extent, z), Vec3(extent, extent, z), Vec3(-extent, extent, z) };
            ImVec2 pts[4];
            for (int k = 0; k < 4; ++k)
                if (!cam.Project(corners[k], pts[k]))
                    return;
            float area = 0.0f;
            for (int k = 0; k < 4; ++k)
                area += pts[k].x * pts[(k + 1) % 4].y - pts[(k + 1) % 4].x * pts[k].y;
            if (area < 0.0f)
                ImSwap(pts[1], pts[3]);
            dl->AddConvexPolyFilled(pts, 4, col);
        }

        static void DrawSceneCamera(ImDrawList* dl, const ViewCamera& view, const Vec3& pos, const Vec3& target)
        {
            Vec3 f = (target - pos).Normalized();
            Vec3 r = f.Cross(Vec3(0.0f, 0.0f, 1.0f)).Normalized();
            Vec3 u = r.Cross(f);
            Vec3 c = pos + f * 1.2f;
            Vec3 corners[4] = { c + r * 0.8f + u * 0.45f, c - r * 0.8f + u * 0.45f, c - r * 0.8f - u * 0.45f, c + r * 0.8f - u * 0.45f };
            ImU32 col = IM_COL32(16, 16, 16, 255);
            for (int i = 0; i < 4; ++i)
            {
                view.Line(dl, pos, corners[i], col, 1.2f);
                view.Line(dl, corners[i], corners[(i + 1) % 4], col, 1.2f);
            }
            ImVec2 t0, t1, t2;
            if (view.Project(c + u * 0.5f + r * 0.25f, t0) && view.Project(c + u * 0.5f - r * 0.25f, t1) && view.Project(c + u * 0.75f, t2))
                dl->AddTriangleFilled(t0, t1, t2, col);
        }

        static void DrawSceneLight(ImDrawList* dl, const ViewCamera& view, const Vec3& pos)
        {
            ImVec2 p, ground;
            if (!view.Project(pos, p))
                return;
            ImU32 col = IM_COL32(16, 16, 16, 255);
            dl->AddCircle(p, 8.0f, col, 16, 1.2f);
            dl->AddCircle(p, 4.0f, col, 12, 1.2f);
            if (view.Project(Vec3(pos.x, pos.y, 0.0f), ground))
            {
                float dx = ground.x - p.x, dy = ground.y - p.y;
                float len = sqrtf(dx * dx + dy * dy);
                for (float t = 10.0f; t < len; t += 8.0f)
                {
                    float t2 = ImMin(t + 4.0f, len);
                    dl->AddLine(ImVec2(p.x + dx * t / len, p.y + dy * t / len), ImVec2(p.x + dx * t2 / len, p.y + dy * t2 / len), WithAlpha(col, 0.8f), 1.0f);
                }
            }
        }

        static void DrawToolHandle(ImDrawList* dl, const ViewCamera& view, int tool)
        {
            const Vec3 axes[3] = { Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f) };
            const ImU32 cols[3] = { AxisX, AxisY, AxisZ };
            ImVec2 origin;
            if (!view.Project(Vec3(0.0f, 0.0f, 0.0f), origin))
                return;

            if (tool == 2 || tool == 5)
            {
                for (int i = 0; i < 3; ++i)
                {
                    const Vec3& u = axes[(i + 1) % 3];
                    const Vec3& w = axes[(i + 2) % 3];
                    const int segments = 40;
                    for (int k = 0; k < segments; ++k)
                    {
                        float a0 = k * IM_PI * 2.0f / segments, a1 = (k + 1) * IM_PI * 2.0f / segments;
                        view.Line(dl, (u * cosf(a0) + w * sinf(a0)) * 2.0f, (u * cosf(a1) + w * sinf(a1)) * 2.0f, WithAlpha(cols[i], 0.9f), 1.6f);
                    }
                }
            }
            if (tool == 1 || tool == 3 || tool == 5)
            {
                for (int i = 0; i < 3; ++i)
                {
                    ImVec2 tip;
                    if (!view.Project(axes[i] * 2.4f, tip))
                        continue;
                    if (tool == 3)
                    {
                        dl->AddLine(origin, tip, cols[i], 2.0f);
                        dl->AddRectFilled(ImVec2(tip.x - 4.0f, tip.y - 4.0f), ImVec2(tip.x + 4.0f, tip.y + 4.0f), cols[i]);
                    }
                    else
                    {
                        Arrow(dl, origin, tip, cols[i], 9.0f, 2.0f);
                    }
                }
                if (tool == 3)
                    dl->AddRectFilled(ImVec2(origin.x - 4.0f, origin.y - 4.0f), ImVec2(origin.x + 4.0f, origin.y + 4.0f), IM_COL32(200, 200, 200, 255));
            }
            if (tool == 4)
            {
                ImVec2 mn(FLT_MAX, FLT_MAX), mx(-FLT_MAX, -FLT_MAX);
                for (int i = 0; i < 8; ++i)
                {
                    ImVec2 s;
                    if (!view.Project(Vec3((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f), s))
                        return;
                    mn = ImMin(mn, s);
                    mx = ImMax(mx, s);
                }
                dl->AddRect(mn, mx, AxisZ, 0.0f, 0, 1.2f);
                const ImVec2 corners[4] = { mn, ImVec2(mx.x, mn.y), mx, ImVec2(mn.x, mx.y) };
                for (const ImVec2& k : corners)
                    dl->AddCircleFilled(k, 3.5f, AxisZ);
            }
        }

        static void DrawNavigationGizmo(ImDrawList* dl, const ViewCamera& view, ImVec2 center)
        {
            struct Axis { Vec3 Dir; ImU32 Col; const char* Label; float Depth; ImVec2 Pos; };
            Axis axes[6] = {
                { Vec3( 1, 0, 0), AxisX, "x" }, { Vec3(-1, 0, 0), AxisX, nullptr },
                { Vec3( 0, 1, 0), AxisY, "y" }, { Vec3( 0,-1, 0), AxisY, nullptr },
                { Vec3( 0, 0, 1), AxisZ, "z" }, { Vec3( 0, 0,-1), AxisZ, nullptr },
            };
            const float radius = 24.0f;
            for (Axis& a : axes)
            {
                a.Depth = a.Dir.Dot(view.Forward);
                a.Pos = ImVec2(center.x + a.Dir.Dot(view.Right) * radius, center.y - a.Dir.Dot(view.Up) * radius);
            }
            for (int i = 0; i < 6; ++i)
                for (int j = i + 1; j < 6; ++j)
                    if (axes[j].Depth > axes[i].Depth)
                        ImSwap(axes[i], axes[j]);

            dl->AddCircleFilled(center, 5.0f, IM_COL32(170, 170, 170, 255), 12);
            for (const Axis& a : axes)
            {
                if (a.Label)
                {
                    dl->AddLine(center, a.Pos, a.Col, 2.0f);
                    dl->AddCircleFilled(a.Pos, 7.5f, a.Col, 20);
                    BadgeText(dl, a.Pos, a.Label);
                }
                else
                {
                    dl->AddCircleFilled(a.Pos, 6.5f, WithAlpha(IM_COL32(170, 170, 170, 255), 0.5f), 20);
                }
            }
        }

        static void DrawHierarchy()
        {
            struct Row { const char* Name; Icon Glyph; ImU32 IconCol; int Depth; bool HasChildren; };
            static const Row rows[] = {
                { "SampleScene",       Icon::Scene,  IconColor,  0, true  },
                { "Main Camera",       Icon::Camera, IconData,   1, false },
                { "Directional Light", Icon::Light,  IconData,   1, false },
                { "Cube",              Icon::Mesh,   IconObject, 1, false },
                { "Ground",            Icon::Mesh,   IconObject, 1, false },
            };
            static int selected = 3;
            static char filter[64] = "";

            if (BeginEditor("Hierarchy", AreaBg, true))
            {
                BeginHeader();
                bool create = StateButton("##create", Icon::Plus, false, "Create", SegButtonSize, true);
                if (BeginAnchoredPopup("##createmenu", create, BelowLastItem()))
                {
                    ImGui::MenuItem("Create Empty");
                    if (ImGui::BeginMenu("3D Object"))
                    {
                        ImGui::MenuItem("Cube");
                        ImGui::MenuItem("Plane");
                        ImGui::MenuItem("Sphere");
                        ImGui::EndMenu();
                    }
                    ImGui::MenuItem("Light");
                    ImGui::MenuItem("Camera");
                    ImGui::EndPopup();
                }
                ImGui::SameLine();
                SearchField("##filter", filter, sizeof(filter), ImGui::GetContentRegionAvail().x - SegButtonSize - 4.0f);
                ImGui::SameLine();
                IconButton("##hmenu", Icon::Kebab, false, SegButtonSize, TextDim);
                EndHeader();

                BeginBody("##tree", ImVec2(0.0f, 4.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
                ImDrawList* dl = ImGui::GetWindowDrawList();
                for (int i = 0; i < IM_ARRAYSIZE(rows); ++i)
                {
                    const Row& r = rows[i];
                    ImGui::PushID(i);
                    ImVec2 rowMin = ImGui::GetCursorScreenPos();
                    const float rowW = ImGui::GetContentRegionAvail().x;
                    if (i % 2)
                        dl->AddRectFilled(rowMin, Offset(rowMin, rowW, 20.0f), RowAltBg);
                    if (ImGui::Selectable("##row", selected == i, 0, ImVec2(0.0f, 20.0f)))
                        selected = i;

                    float x = rowMin.x + 6.0f + r.Depth * 14.0f;
                    float cy = rowMin.y + 10.0f;
                    if (r.HasChildren)
                        dl->AddTriangleFilled(ImVec2(x, cy - 3.0f), ImVec2(x + 8.0f, cy - 3.0f), ImVec2(x + 4.0f, cy + 3.0f), TextDim);
                    DrawIcon(dl, r.Glyph, ImVec2(x + 19.0f, cy), 13.0f, r.IconCol);
                    dl->AddText(ImVec2(x + 30.0f, cy - ImGui::GetFontSize() * 0.5f), selected == i ? TextSelected : TextColor, r.Name);
                    ImGui::PopID();
                }
                ImGui::PopStyleVar();
                ImGui::EndChild();
            }
            ImGui::End();
        }

        static void DrawSceneTab()
        {
            static const char* pivotNames[2] = { "Center", "Pivot" };
            static const char* orientationNames[2] = { "Global", "Local" };
            static const char* drawModeNames[3] = { "Shaded", "Wireframe", "Shaded Wireframe" };
            static const char* toolNames[6] = { "View Tool (Q)", "Move Tool (W)", "Rotate Tool (E)", "Scale Tool (R)", "Rect Tool (T)", "Transform Tool (Y)" };
            static const Icon toolIcons[6] = { Icon::Hand, Icon::Move, Icon::Rotate, Icon::Scale, Icon::Rect, Icon::Transform };
            static const ImGuiKey toolKeys[6] = { ImGuiKey_Q, ImGuiKey_W, ImGuiKey_E, ImGuiKey_R, ImGuiKey_T, ImGuiKey_Y };
            static int tool = 1;
            static int pivot = 0, orientation = 0, drawMode = 0;
            static bool snapping = false, is2D = false, lighting = true, audio = false, effects = true, hiddenActors = false, gizmos = true;
            static bool skybox = true, fog = true, gizmoIcons = true, gizmoGrid = true, gizmoOutline = true;
            char tip[64];

            BeginHeader();
            BeginSegment(3);
            snprintf(tip, sizeof(tip), "Pivot: %s", pivotNames[pivot]);
            bool pivotPressed = SegButton("##pivot", pivot == 0 ? Icon::PivotCenter : Icon::PivotLocal, false, tip, true);
            if (BeginAnchoredPopup("##pivotmenu", pivotPressed, BelowLastItem()))
            {
                for (int i = 0; i < 2; ++i)
                    if (ImGui::MenuItem(pivotNames[i], nullptr, pivot == i))
                        pivot = i;
                ImGui::EndPopup();
            }
            snprintf(tip, sizeof(tip), "Orientation: %s", orientationNames[orientation]);
            bool orientPressed = SegButton("##orient", orientation == 0 ? Icon::Globe : Icon::Mesh, false, tip, true);
            if (BeginAnchoredPopup("##orientmenu", orientPressed, BelowLastItem()))
            {
                for (int i = 0; i < 2; ++i)
                    if (ImGui::MenuItem(orientationNames[i], nullptr, orientation == i))
                        orientation = i;
                ImGui::EndPopup();
            }
            if (SegButton("##snap", Icon::Magnet, snapping, "Grid Snapping"))
                snapping = !snapping;
            EndSegment();

            AlignRight(SegmentWidth(2) + SegmentGap + SegmentWidth(3) + SegmentGap + SegmentWidth(2));
            BeginSegment(2);
            snprintf(tip, sizeof(tip), "Draw Mode: %s", drawModeNames[drawMode]);
            bool drawPressed = SegButton("##drawmode", drawMode == 1 ? Icon::Wire : Icon::Sphere, false, tip, true);
            if (BeginAnchoredPopup("##drawmodemenu", drawPressed, BelowLastItem()))
            {
                for (int i = 0; i < 3; ++i)
                    if (ImGui::MenuItem(drawModeNames[i], nullptr, drawMode == i))
                        drawMode = i;
                ImGui::EndPopup();
            }
            if (SegButton("##2d", Icon::TwoD, is2D, "2D"))
                is2D = !is2D;
            EndSegment();

            BeginSegment(3);
            if (SegButton("##lighting", Icon::Light, lighting, "Lighting"))
                lighting = !lighting;
            if (SegButton("##audio", Icon::Audio, audio, "Audio"))
                audio = !audio;
            bool effectsPressed = SegButton("##effects", Icon::Effects, effects, "Effects", true);
            bool effectsMenu = effectsPressed && PressedMenuCorner();
            if (effectsPressed && !effectsMenu)
                effects = !effects;
            if (BeginAnchoredPopup("##effectsmenu", effectsMenu, BelowLastItem()))
            {
                ImGui::MenuItem("Skybox", nullptr, &skybox);
                ImGui::MenuItem("Fog", nullptr, &fog);
                ImGui::EndPopup();
            }
            EndSegment();

            BeginSegment(2);
            if (SegButton("##hidden", Icon::EyeOff, hiddenActors, "Hidden Actors"))
                hiddenActors = !hiddenActors;
            bool gizmosPressed = SegButton("##gizmos", Icon::Gizmo, gizmos, "Gizmos", true);
            bool gizmosMenu = gizmosPressed && PressedMenuCorner();
            if (gizmosPressed && !gizmosMenu)
                gizmos = !gizmos;
            if (BeginAnchoredPopup("##gizmosmenu", gizmosMenu, BelowLastItem()))
            {
                ImGui::MenuItem("3D Icons", nullptr, &gizmoIcons);
                ImGui::MenuItem("Show Grid", nullptr, &gizmoGrid);
                ImGui::MenuItem("Selection Outline", nullptr, &gizmoOutline);
                ImGui::EndPopup();
            }
            EndSegment();
            EndHeader();

            BeginBody("##canvas", ImVec2(0.0f, 0.0f), ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            const ImVec2 size = ImGui::GetContentRegionAvail();
            const ImVec2 p1(p0.x + size.x, p0.y + size.y);

            if (ImGui::IsWindowHovered() && !ImGui::GetIO().WantTextInput)
                for (int i = 0; i < 6; ++i)
                    if (ImGui::IsKeyPressed(toolKeys[i], false))
                        tool = i;

            dl->AddRectFilled(p0, p1, ViewportBg, AreaRounding, ImDrawFlags_RoundCornersBottom);
            dl->PushClipRect(p0, p1, true);

            const ImVec2 center(p0.x + size.x * 0.5f, p0.y + size.y * 0.5f);
            ViewCamera view = is2D
                ? ViewCamera::LookAt(Vec3(0.0f, -16.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), center, size.y, 0.45f)
                : ViewCamera::LookAt(Vec3(12.5f, -10.8f, 8.4f), Vec3(0.4f, 0.4f, 0.6f), center, size.y, 0.70f);
            if (gizmos && gizmoGrid && !is2D)
                DrawGrid(dl, view);
            if (gizmos && gizmoIcons)
            {
                DrawSceneCamera(dl, view, Vec3(7.36f, -6.93f, 4.96f), Vec3(0.0f, 0.0f, 0.0f));
                DrawSceneLight(dl, view, Vec3(4.08f, 1.0f, 5.9f));
            }
            DrawCube(dl, view, gizmoOutline, static_cast<DrawMode>(drawMode), lighting);
            if (gizmos)
                DrawToolHandle(dl, view, tool);

            const float tb = 24.0f;
            const ImVec2 o0(p0.x + 8.0f, p0.y + 8.0f);
            dl->AddRectFilled(o0, Offset(o0, tb + 4.0f, 6 * tb + 4.0f), OverlayBg, 5.0f);
            for (int i = 0; i < 6; ++i)
            {
                ImGui::SetCursorScreenPos(Offset(o0, 2.0f, 2.0f + i * tb));
                ImGui::PushID(i);
                if (StateButton("##tool", toolIcons[i], tool == i, toolNames[i], tb))
                    tool = i;
                ImGui::PopID();
            }

            const ImVec2 gizmo(p1.x - 40.0f, p0.y + 40.0f);
            DrawNavigationGizmo(dl, view, gizmo);
            const char* projection = is2D ? "Iso" : "< Persp";
            const ImVec2 ps = ImGui::CalcTextSize(projection);
            dl->AddText(ImVec2(gizmo.x - ps.x * 0.5f, gizmo.y + 34.0f), TextDim, projection);

            dl->PopClipRect();
            ImGui::EndChild();
        }

        static void DrawGameTab()
        {
            static int display = 0, aspect = 0;
            static bool mute = false, stats = false, gizmos = false;
            static const char* displays[] = { "Display 1", "Display 2" };
            static const char* aspects[] = { "Free Aspect", "16:9", "16:10", "4:3" };

            BeginHeader();
            if (BeginDropdown("##display", displays[display]))
            {
                for (int i = 0; i < IM_ARRAYSIZE(displays); ++i)
                    if (ImGui::MenuItem(displays[i], nullptr, display == i))
                        display = i;
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            if (BeginDropdown("##aspect", aspects[aspect], Icon::None, 110.0f))
            {
                for (int i = 0; i < IM_ARRAYSIZE(aspects); ++i)
                    if (ImGui::MenuItem(aspects[i], nullptr, aspect == i))
                        aspect = i;
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            AlignRight(SegmentWidth(3));
            BeginSegment(3);
            if (SegButton("##mute", Icon::Audio, mute, "Mute Audio"))
                mute = !mute;
            if (SegButton("##stats", Icon::Stats, stats, "Stats"))
                stats = !stats;
            if (SegButton("##ggizmos", Icon::Gizmo, gizmos, "Gizmos"))
                gizmos = !gizmos;
            EndSegment();
            EndHeader();

            BeginBody("##gameview", ImVec2(0.0f, 0.0f), ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImVec2 size = ImGui::GetContentRegionAvail();
            ImVec2 p1(p0.x + size.x, p0.y + size.y);
            dl->AddRectFilled(p0, p1, IM_COL32(24, 24, 24, 255), AreaRounding, ImDrawFlags_RoundCornersBottom);

            static const float ratios[] = { 0.0f, 16.0f / 9.0f, 16.0f / 10.0f, 4.0f / 3.0f };
            if (ratios[aspect] > 0.0f && size.y > 0.0f)
            {
                float w = size.x, h = size.y;
                if (w / h > ratios[aspect]) w = h * ratios[aspect]; else h = w / ratios[aspect];
                p0 = ImVec2(ImFloor(p0.x + (size.x - w) * 0.5f), ImFloor(p0.y + (size.y - h) * 0.5f));
                size = ImVec2(w, h);
                p1 = ImVec2(p0.x + w, p0.y + h);
            }

            dl->PushClipRect(p0, p1, true);
            const Vec3 eye(7.36f, -6.93f, 4.96f);
            ViewCamera cam = ViewCamera::LookAt(eye, Vec3(0.0f, 0.0f, 0.0f), ImVec2(p0.x + size.x * 0.5f, p0.y + size.y * 0.5f), size.y, 0.69f);
            Vec3 flat(cam.Forward.x, cam.Forward.y, 0.0f);
            ImVec2 horizon(0.0f, p0.y + size.y * 0.5f);
            cam.Project(eye + flat.Normalized() * 1000.0f, horizon);
            const float hy = ImClamp(horizon.y, p0.y, p1.y);
            dl->AddRectFilledMultiColor(p0, ImVec2(p1.x, hy), IM_COL32(70, 82, 98, 255), IM_COL32(70, 82, 98, 255),
                IM_COL32(126, 130, 136, 255), IM_COL32(126, 130, 136, 255));
            dl->AddRectFilled(ImVec2(p0.x, hy), p1, IM_COL32(52, 50, 48, 255));
            DrawGroundPlane(dl, cam, -1.0f, 6.0f, IM_COL32(92, 92, 92, 255));
            DrawCube(dl, cam, false);
            if (gizmos)
                DrawSceneLight(dl, cam, Vec3(4.08f, 1.0f, 5.9f));
            if (stats)
            {
                char text[128];
                snprintf(text, sizeof(text), "Statistics\nFPS: %.0f (%.1f ms)\nBatches: 3\nTris: 14", ImGui::GetIO().Framerate,
                    1000.0f / ImMax(ImGui::GetIO().Framerate, 1.0f));
                ImVec2 ts = ImGui::CalcTextSize(text);
                ImVec2 b0(p1.x - ts.x - 22.0f, p0.y + 8.0f);
                dl->AddRectFilled(b0, ImVec2(p1.x - 8.0f, b0.y + ts.y + 12.0f), OverlayBg, 5.0f);
                dl->AddText(Offset(b0, 7.0f, 6.0f), TextColor, text);
            }
            dl->PopClipRect();
            ImGui::EndChild();
        }

        static void DrawSceneView()
        {
            if (BeginEditor("Scene", ViewportBg, true))
                DrawSceneTab();
            ImGui::End();
        }

        static void DrawGameView()
        {
            if (BeginEditor("Game", ViewportBg))
                DrawGameTab();
            ImGui::End();
        }

        static void DrawInspectorActor()
        {
            static const Icon tabIcons[6] = { Icon::Object, Icon::Component, Icon::Physics, Icon::Data, Icon::Material, Icon::Script };
            static const ImU32 tabColors[6] = { IconObject, IconComponent, IconComponent, IconData, IconMaterial, IconColor };
            static const char* tabNames[6] = { "Actor", "Components", "Physics", "Mesh", "Material", "Scripts" };
            static int vtab = 0;
            static int scrollTo = -1;
            static bool active = true, isStatic = false, receiveShadows = true, isTrigger = false;
            static char name[64] = "Cube";
            static int tag = 0, layer = 0, castShadows = 1;
            static float position[3] = { 0.0f, 0.0f, 0.0f };
            static float rotation[3] = { 0.0f, 0.0f, 0.0f };
            static float scale[3] = { 1.0f, 1.0f, 1.0f };
            static float colliderCenter[3] = { 0.0f, 0.0f, 0.0f };
            static float colliderSize[3] = { 1.0f, 1.0f, 1.0f };
            static const char* tags[] = { "Untagged", "Player", "MainCamera", "EditorOnly" };
            static const char* layers[] = { "Default", "UI", "Water", "Ignore Raycast" };
            static const char* shadowModes[] = { "Off", "On", "Two Sided", "Shadows Only" };

            if (VerticalIconTabs(tabIcons, tabColors, tabNames, 6, vtab))
                scrollTo = vtab;

            BeginBody("##inspector", ImVec2(6.0f, 6.0f));
            auto scrollAnchor = [](int target)
            {
                if (scrollTo == target)
                {
                    ImGui::SetScrollHereY(0.0f);
                    scrollTo = -1;
                }
            };

            scrollAnchor(0);
            {
                ImVec2 p = ImGui::GetCursorScreenPos();
                DrawIcon(ImGui::GetWindowDrawList(), Icon::Mesh, ImVec2(p.x + 9.0f, p.y + ImGui::GetFrameHeight() * 0.5f), 15.0f, IconObject);
                ImGui::Dummy(ImVec2(18.0f, ImGui::GetFrameHeight()));
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::Checkbox("##active", &active);
                ImGui::SetItemTooltip("Active");
                ImGui::SameLine();
                const ImGuiStyle& style = ImGui::GetStyle();
                const float staticW = style.ItemSpacing.x + ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize("Static").x + 4.0f;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - staticW);
                ImGui::InputText("##name", name, sizeof(name));
                ImGui::SameLine();
                ImGui::Checkbox("Static", &isStatic);

                const float labelGap = 6.0f;
                const float tagW = ImGui::CalcTextSize("Tag").x, layerW = ImGui::CalcTextSize("Layer").x;
                const float fieldW = (ImGui::GetContentRegionAvail().x - tagW - layerW - 2.0f * labelGap - 12.0f) * 0.5f;
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ToVec4(TextDim), "Tag");
                ImGui::SameLine(0.0f, labelGap);
                if (BeginDropdown("##tag", tags[tag], Icon::None, fieldW))
                {
                    for (int i = 0; i < IM_ARRAYSIZE(tags); ++i)
                        if (ImGui::MenuItem(tags[i], nullptr, tag == i))
                            tag = i;
                    ImGui::EndPopup();
                }
                ImGui::SameLine(0.0f, 12.0f);
                ImGui::TextColored(ToVec4(TextDim), "Layer");
                ImGui::SameLine(0.0f, labelGap);
                if (BeginDropdown("##layer", layers[layer], Icon::None, fieldW))
                {
                    for (int i = 0; i < IM_ARRAYSIZE(layers); ++i)
                        if (ImGui::MenuItem(layers[i], nullptr, layer == i))
                            layer = i;
                    ImGui::EndPopup();
                }
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
            }

            scrollAnchor(1);
            if (BeginPanel("Transform", Icon::Move, IconColor))
            {
                PropertyVec3("Position", position, 0.01f);
                PropertyVec3("Rotation", rotation, 0.5f, "%.1f");
                PropertyVec3("Scale", scale, 0.01f);
                EndPanel();
            }
            scrollAnchor(3);
            if (BeginPanel("Mesh Filter", Icon::Data, IconData))
            {
                PropertyLabel("Mesh");
                if (BeginDropdown("##mesh", "Cube", Icon::Data, ImGui::GetContentRegionAvail().x, IconData))
                {
                    ImGui::MenuItem("Cube");
                    ImGui::MenuItem("Plane");
                    ImGui::MenuItem("Sphere");
                    ImGui::EndPopup();
                }
                EndPanel();
            }
            scrollAnchor(4);
            if (BeginPanel("Mesh Renderer", Icon::Mesh, IconData))
            {
                PropertyLabel("Material");
                if (BeginDropdown("##material", "Default-Material", Icon::Material, ImGui::GetContentRegionAvail().x, IconMaterial))
                {
                    ImGui::MenuItem("Default-Material");
                    ImGui::MenuItem("Steel");
                    ImGui::MenuItem("Water");
                    ImGui::EndPopup();
                }
                PropertyLabel("Cast Shadows");
                if (BeginDropdown("##shadows", shadowModes[castShadows], Icon::None, ImGui::GetContentRegionAvail().x))
                {
                    for (int i = 0; i < IM_ARRAYSIZE(shadowModes); ++i)
                        if (ImGui::MenuItem(shadowModes[i], nullptr, castShadows == i))
                            castShadows = i;
                    ImGui::EndPopup();
                }
                PropertyLabel("Receive");
                ImGui::Checkbox("##receive", &receiveShadows);
                ImGui::SetItemTooltip("Receive Shadows");
                EndPanel();
            }
            scrollAnchor(2);
            if (BeginPanel("Box Collider", Icon::Physics, IconComponent, false))
            {
                PropertyLabel("Is Trigger");
                ImGui::Checkbox("##trigger", &isTrigger);
                PropertyVec3("Center", colliderCenter, 0.01f);
                PropertyVec3("Size", colliderSize, 0.01f);
                EndPanel();
            }

            scrollAnchor(5);
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            const float buttonW = ImMin(220.0f, ImGui::GetContentRegionAvail().x * 0.7f);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - buttonW) * 0.5f);
            if (ImGui::Button("Add Component", ImVec2(buttonW, 0.0f)))
                ImGui::OpenPopup("##addcomponent");
            if (ImGui::BeginPopup("##addcomponent"))
            {
                ImGui::MenuItem("Rigidbody");
                ImGui::MenuItem("Audio Source");
                ImGui::MenuItem("Script...");
                ImGui::EndPopup();
            }
            ImGui::EndChild();
        }

        static void DrawInspectorLighting()
        {
            static int skybox = 0, sun = 0;
            static float intensity = 1.0f;
            static bool realtimeGI = false, bakedGI = true, autoGenerate = false;
            static const char* skyboxes[] = { "Default-Skybox", "None" };
            static const char* suns[] = { "None", "Directional Light" };

            BeginBody("##lighting", ImVec2(6.0f, 6.0f));
            if (BeginPanel("Environment", Icon::Globe, IconMaterial))
            {
                PropertyLabel("Skybox");
                if (BeginDropdown("##skybox", skyboxes[skybox], Icon::Material, ImGui::GetContentRegionAvail().x, IconMaterial))
                {
                    for (int i = 0; i < IM_ARRAYSIZE(skyboxes); ++i)
                        if (ImGui::MenuItem(skyboxes[i], nullptr, skybox == i))
                            skybox = i;
                    ImGui::EndPopup();
                }
                PropertyLabel("Sun Source");
                if (BeginDropdown("##sun", suns[sun], Icon::None, ImGui::GetContentRegionAvail().x))
                {
                    for (int i = 0; i < IM_ARRAYSIZE(suns); ++i)
                        if (ImGui::MenuItem(suns[i], nullptr, sun == i))
                            sun = i;
                    ImGui::EndPopup();
                }
                PropertyLabel("Intensity");
                ImGui::SliderFloat("##intensity", &intensity, 0.0f, 8.0f, "%.2f");
                EndPanel();
            }
            if (BeginPanel("Global Illumination", Icon::Light, IconData))
            {
                ImGui::Checkbox("Realtime GI", &realtimeGI);
                ImGui::Checkbox("Baked GI", &bakedGI);
                EndPanel();
            }
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::Checkbox("Auto Generate", &autoGenerate);
            ImGui::SameLine();
            AlignRight(130.0f);
            ImGui::Button("Generate Lighting", ImVec2(130.0f, 0.0f));
            ImGui::EndChild();
        }

        static void DrawInspector()
        {
            if (BeginEditor("Inspector", HeaderBg, true))
                DrawInspectorActor();
            ImGui::End();
        }

        static void DrawLighting()
        {
            if (BeginEditor("Lighting", HeaderBg))
                DrawInspectorLighting();
            ImGui::End();
        }

        static void DrawProjectTab()
        {
            struct Folder { const char* Name; Icon Glyph; int Depth; };
            static const Folder folders[] = {
                { "Favorites", Icon::Star,   0 },
                { "Assets",    Icon::Folder, 0 },
                { "Materials", Icon::Folder, 1 },
                { "Scenes",    Icon::Folder, 1 },
                { "Scripts",   Icon::Folder, 1 },
                { "Packages",  Icon::Folder, 0 },
            };
            struct Asset { const char* Name; int FolderIndex; Icon Glyph; ImU32 Col; bool Sphere; };
            static const Asset assets[] = {
                { "Materials",        1, Icon::Folder, IconColor,                     false },
                { "Scenes",           1, Icon::Folder, IconColor,                     false },
                { "Scripts",          1, Icon::Folder, IconColor,                     false },
                { "Crate",            1, Icon::Mesh,   IconObject,                    false },
                { "Steel",            2, Icon::None,   IM_COL32(132, 132, 132, 255),  true  },
                { "Water",            2, Icon::None,   IM_COL32( 85, 133, 208, 255),  true  },
                { "Copper",           2, Icon::None,   IM_COL32(196, 120,  74, 255),  true  },
                { "Marble",           2, Icon::None,   IM_COL32(210, 210, 210, 255),  true  },
                { "SampleScene",      3, Icon::Scene,  IconColor,                     false },
                { "PlayerController", 4, Icon::Script, IconColor,                     false },
                { "CameraFollow",     4, Icon::Script, IconColor,                     false },
            };
            static int folder = 2;
            static int selectedAsset = 5;
            static bool favorite = false;
            static char search[64] = "";

            BeginHeader();
            bool create = StateButton("##pcreate", Icon::Plus, false, "Create", SegButtonSize, true);
            if (BeginAnchoredPopup("##pcreatemenu", create, BelowLastItem()))
            {
                ImGui::MenuItem("Folder");
                ImGui::MenuItem("Material");
                ImGui::MenuItem("Script");
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            AlignRight(170.0f + 4.0f + SegButtonSize);
            SearchField("##assetsearch", search, sizeof(search), 170.0f);
            ImGui::SameLine();
            if (IconButton("##favorite", Icon::Star, favorite, SegButtonSize))
                favorite = !favorite;
            ImGui::SetItemTooltip("Save Search");
            EndHeader();

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            ImGui::BeginChild("##folders", ImVec2(140.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);
            {
                ImGuiWindow* window = ImGui::GetCurrentWindow();
                ImDrawList* dl = window->DrawList;
                dl->AddRectFilled(window->Pos, Offset(window->Pos, window->Size.x, window->Size.y), RowAltBg, AreaRounding, ImDrawFlags_RoundCornersBottomLeft);
                for (int i = 0; i < IM_ARRAYSIZE(folders); ++i)
                {
                    const Folder& f = folders[i];
                    ImGui::PushID(i);
                    ImVec2 rowMin = ImGui::GetCursorScreenPos();
                    if (ImGui::Selectable("##folder", folder == i, 0, ImVec2(0.0f, 20.0f)))
                        folder = i;
                    float x = rowMin.x + 6.0f + f.Depth * 14.0f;
                    float cy = rowMin.y + 10.0f;
                    if (i == 0 || i == 1)
                        dl->AddTriangleFilled(ImVec2(x, cy - 3.0f), ImVec2(x + 8.0f, cy - 3.0f), ImVec2(x + 4.0f, cy + 3.0f), TextDim);
                    DrawIcon(dl, f.Glyph, ImVec2(x + 19.0f, cy), 12.0f, IconColor);
                    dl->AddText(ImVec2(x + 30.0f, cy - ImGui::GetFontSize() * 0.5f), folder == i ? TextSelected : TextColor, f.Name);
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::SameLine(0.0f, 0.0f);

            BeginBody("##assets", ImVec2(10.0f, 6.0f));
            if (folders[folder].Depth > 0)
                ImGui::TextColored(ToVec4(TextDim), "Assets  \xE2\x80\xBA  %s", folders[folder].Name);
            else
                ImGui::TextColored(ToVec4(TextDim), "%s", folders[folder].Name);
            ImGui::Dummy(ImVec2(0.0f, 2.0f));

            const float tile = 64.0f, gap = 12.0f;
            const int columns = ImMax(1, static_cast<int>((ImGui::GetContentRegionAvail().x + gap) / (tile + gap)));
            int shown = 0;
            for (int i = 0; i < IM_ARRAYSIZE(assets); ++i)
            {
                const Asset& a = assets[i];
                if (a.FolderIndex != folder)
                    continue;
                if (shown > 0 && shown % columns != 0)
                    ImGui::SameLine(0.0f, gap);
                ++shown;

                ImGui::PushID(i);
                ImVec2 p = ImGui::GetCursorScreenPos();
                if (ImGui::InvisibleButton("##asset", ImVec2(tile, tile + 18.0f)))
                    selectedAsset = i;
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && a.Glyph == Icon::Folder)
                {
                    for (int f = 0; f < IM_ARRAYSIZE(folders); ++f)
                        if (strcmp(folders[f].Name, a.Name) == 0)
                            folder = f;
                }
                ImDrawList* dl = ImGui::GetWindowDrawList();
                const bool sel = (selectedAsset == i);
                const bool hov = ImGui::IsItemHovered();
                dl->AddRectFilled(p, Offset(p, tile, tile), sel ? Selection : (hov ? IM_COL32(52, 52, 52, 255) : IM_COL32(40, 40, 40, 255)), AreaRounding);
                ImVec2 c = Offset(p, tile * 0.5f, tile * 0.5f);
                if (a.Sphere)
                {
                    dl->AddCircleFilled(c, tile * 0.3f, a.Col, 32);
                    dl->AddCircleFilled(Offset(c, -tile * 0.09f, -tile * 0.09f), tile * 0.09f, WithAlpha(IM_COL32_WHITE, 0.45f), 16);
                }
                else
                {
                    DrawIcon(dl, a.Glyph, c, tile * 0.5f, a.Col);
                }
                const ImVec2 ts = ImGui::CalcTextSize(a.Name);
                const float tx = ImMax(p.x, p.x + (tile - ts.x) * 0.5f);
                dl->PushClipRect(ImVec2(p.x - gap * 0.5f, p.y), ImVec2(p.x + tile + gap * 0.5f, p.y + tile + 20.0f), true);
                dl->AddText(ImVec2(tx, p.y + tile + 3.0f), sel ? TextSelected : TextColor, a.Name);
                dl->PopClipRect();
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        static void DrawConsoleTab()
        {
            enum Level { LevelInfo, LevelWarning, LevelError };
            struct Entry { Level Kind; const char* Text; };
            static const Entry entries[] = {
                { LevelInfo,    "[12:00:01] XGE editor started" },
                { LevelInfo,    "[12:00:01] Loaded module game.kernel (xgGameKernel.dll)" },
                { LevelInfo,    "[12:00:02] Loaded module editor (xgEditor.CoreCLR.dll)" },
                { LevelWarning, "[12:00:02] Shader 'Default-Material' compiled with fallback" },
                { LevelInfo,    "[12:00:03] Scene 'SampleScene' opened" },
                { LevelError,   "[12:00:05] NullReferenceException in CameraFollow.Update()" },
            };
            static bool collapse = false, errorPause = false;
            static bool show[3] = { true, true, true };
            static int selected = -1;
            const Icon icons[3] = { Icon::Info, Icon::Warning, Icon::Error };
            const ImU32 colors[3] = { IconColor, IconWarning, IconMaterial };
            int counts[3] = {};
            for (const Entry& e : entries)
                ++counts[e.Kind];

            BeginHeader();
            FlatButton("Clear");
            ImGui::SameLine();
            if (FlatButton("Collapse", collapse))
                collapse = !collapse;
            ImGui::SameLine();
            if (FlatButton("Error Pause", errorPause))
                errorPause = !errorPause;
            ImGui::SameLine();
            const float countW = ImGui::CalcTextSize("0").x;
            AlignRight(3.0f * (SegButtonSize + 2.0f + countW) + 2.0f * 8.0f);
            for (int i = 0; i < 3; ++i)
            {
                ImGui::PushID(i);
                if (IconButton("##level", icons[i], false, SegButtonSize, show[i] ? colors[i] : WithAlpha(colors[i], 0.3f)))
                    show[i] = !show[i];
                ImGui::PopID();
                ImGui::SameLine(0.0f, 2.0f);
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ToVec4(show[i] ? TextColor : TextDim), "%d", counts[i]);
                if (i < 2)
                    ImGui::SameLine(0.0f, 8.0f);
            }
            EndHeader();

            BeginBody("##log", ImVec2(0.0f, 2.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            int row = 0;
            for (int i = 0; i < IM_ARRAYSIZE(entries); ++i)
            {
                const Entry& e = entries[i];
                if (!show[e.Kind])
                    continue;
                ImGui::PushID(i);
                ImVec2 rowMin = ImGui::GetCursorScreenPos();
                if (row++ % 2)
                    dl->AddRectFilled(rowMin, Offset(rowMin, ImGui::GetContentRegionAvail().x, 22.0f), RowAltBg);
                if (ImGui::Selectable("##entry", selected == i, 0, ImVec2(0.0f, 22.0f)))
                    selected = i;
                DrawIcon(dl, icons[e.Kind], ImVec2(rowMin.x + 16.0f, rowMin.y + 11.0f), 14.0f, colors[e.Kind]);
                dl->AddText(ImVec2(rowMin.x + 30.0f, rowMin.y + 11.0f - ImGui::GetFontSize() * 0.5f), selected == i ? TextSelected : TextColor, e.Text);
                ImGui::PopID();
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        static void DrawProject()
        {
            if (BeginEditor("Project", AreaBg, true))
                DrawProjectTab();
            ImGui::End();
        }

        static void DrawConsole()
        {
            if (BeginEditor("Console", AreaBg))
                DrawConsoleTab();
            ImGui::End();
        }

        static void DrawMainToolbar()
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
            if (SegButton("##play", Icon::Play, s_playing, s_playing ? "Stop" : "Play"))
            {
                s_playing = !s_playing;
                if (!s_playing)
                    s_paused = false;
            }
            if (SegButton("##pause", Icon::Pause, s_paused, "Pause"))
                s_paused = !s_paused;
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
                    s_resetLayout = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Reset All Layouts"))
                    s_resetLayout = true;
                ImGui::EndPopup();
            }

            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }

        static void DrawTitleBarControls(Window* window, const ImRect& bar, float dragStartX)
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

        static void DrawMenus(Window* mainWindow)
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
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Services"))
            {
                ImGui::MenuItem("Cloud Build");
                ImGui::MenuItem("Version Control");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window"))
            {
                if (ImGui::BeginMenu("Layouts"))
                {
                    if (ImGui::MenuItem("Reset Layout"))
                        s_resetLayout = true;
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                for (const char* editor : { "Scene", "Game", "Inspector", "Lighting", "Hierarchy", "Project", "Console" })
                    if (ImGui::MenuItem(editor))
                        ImGui::SetWindowFocus(editor);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                ImGui::MenuItem("About XGE");
                ImGui::MenuItem("Documentation");
                ImGui::EndMenu();
            }
        }

        static void DrawStatusBar()
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
                ImGui::TextColored(ToVec4(TextDim), "%s", s_playing ? (s_paused ? "Paused" : "Playing") : "Ready");

                const char* version = "XGE 0.1";
                ImGui::SameLine();
                AlignRight(ImGui::CalcTextSize(version).x);
                ImGui::TextColored(ToVec4(TextDim), "%s", version);
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }
    }

    void DrawMainDockspace(Window* mainWindow)
    {
        ui::DrawStatusBar();

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

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::GapBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, ui::TopBarHeight * 0.5f - 7.0f));

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
            const float iconSize = static_cast<float>(ui::AppIconSize);
            if (ui::s_appIcon)
            {
                const ImVec2 i0(iconX, ImFloor(barCenterY - iconSize * 0.5f));
                dl->AddImage(ui::s_appIcon->GetTexRef(),
                    i0, ui::Offset(i0, iconSize, iconSize), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ui::TextColor);
            }
            else
            {
                dl->AddCircleFilled(ImVec2(iconX + iconSize * 0.5f, barCenterY), 8.0f, ui::Accent, 20);
                dl->AddText(ImVec2(iconX + iconSize * 0.5f - 3.5f, barCenterY - ImGui::GetFontSize() * 0.5f), IM_COL32_WHITE, "X");
            }

            const float textTop = ImFloor(barCenterY - ImGui::GetFontSize() * 0.5f);
            ImGui::SetCursorScreenPos(ImVec2(iconX + iconSize + 4.0f, textTop - ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, ImFloor((barH - ImGui::GetFontSize()) * 0.5f)));
            ui::DrawMenus(mainWindow);
            ImGui::PopStyleVar();
            const float menusEndX = ImGui::GetCursorScreenPos().x;

            if (mainWindow && mainWindow->HasCustomFrame())
                ui::DrawTitleBarControls(mainWindow, bar, menusEndX + 12.0f);

            ImGui::EndMenuBar();
        }

        ui::DrawMainToolbar();

        ImGuiID dockspaceID = ImGui::GetID("MainDockspaceID");
        const ImGuiWindow* host = ImGui::GetCurrentWindow();
        const ImVec2 dockPos(host->Pos.x + 2.0f, ImGui::GetItemRectMax().y + 2.0f);
        const ImVec2 dockSize(ImMax(host->Size.x - 4.0f, 1.0f), ImMax(host->Pos.y + host->Size.y - dockPos.y - 2.0f, 1.0f));
        ImGui::SetCursorScreenPos(dockPos);
        ImGui::DockSpace(dockspaceID, dockSize, ImGuiDockNodeFlags_None);

        if (ui::s_resetLayout)
        {
            ui::s_resetLayout = false;
            ImGui::DockBuilderRemoveNode(dockspaceID);
            ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceID, dockSize);

            ImGuiID dockMain = dockspaceID;
            ImGuiID dockInspector, dockProject, dockHierarchy;
            ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.22f, &dockInspector, &dockMain);
            ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, &dockProject, &dockMain);
            ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.20f, &dockHierarchy, &dockMain);

            ImGui::DockBuilderDockWindow("Scene", dockMain);
            ImGui::DockBuilderDockWindow("Game", dockMain);
            ImGui::DockBuilderDockWindow("Hierarchy", dockHierarchy);
            ImGui::DockBuilderDockWindow("Inspector", dockInspector);
            ImGui::DockBuilderDockWindow("Lighting", dockInspector);
            ImGui::DockBuilderDockWindow("Project", dockProject);
            ImGui::DockBuilderDockWindow("Console", dockProject);
            ui::s_selectDefaultTabs = 2;

            ImGui::DockBuilderFinish(dockspaceID);
        }

        if (mainWindow && mainWindow->HasCustomFrame() && !mainWindow->IsMaximized())
        {
            ImGui::GetForegroundDrawList(viewport)->AddRect(viewport->Pos,
                ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), IM_COL32(58, 58, 58, 255));
        }

        ImGui::End();
    }

    void DrawEditorPanels()
    {
        ui::DrawHierarchy();
        ui::DrawSceneView();
        ui::DrawGameView();
        ui::DrawInspector();
        ui::DrawLighting();
        ui::DrawProject();
        ui::DrawConsole();
        if (ui::s_selectDefaultTabs > 0 && --ui::s_selectDefaultTabs == 0)
            ImGui::SetWindowFocus("Scene");
    }

    //
    // ------------------------------------------------------------
    // EditorKernelModule
    // ------------------------------------------------------------
    //

    EditorKernelModule::EditorKernelModule(const char* id, int moduleID, ScriptHost* host, const char* group)
        : ScriptModule(id, moduleID, host, group)
    {}

    EditorKernelModule::~EditorKernelModule()
    {
        Shutdown();
    }

    static const xg::FieldSchema TestFields[] =
    {
        { "Health", "Int32", xg::ValueType::Int32, 0, sizeof(int32_t), alignof(int32_t), {nullptr} },
        { "Name",   "String", xg::ValueType::String, 0, sizeof(const char*), alignof(const char*), {nullptr} }
    };

    static const xg::TypeSchema TestSchema =
    {
        "TestType",
        1,
        false,              // IMPORTANT: dynamic layout
        TestFields,
        2,
        0,
        0
    };

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

        engine->GetSystem<EventDispatcher>()->AddListener(_eventListener.get());

        //
        // ImGui setup
        //
        InitImGui();

		/*auto sceneGraphFactory = engine->GetSystem<SceneGraphFactory>(this);
		auto actorRegistry = engine->GetSystem<ActorRegistry>(this);
        auto sceneGraph = sceneGraphFactory->Create("tree", *actorRegistry);
		engine->RegisterSystem<SceneGraph>(nullptr, sceneGraph);

        auto sceneRoot = xg::CreateNode<HierarchyNode>(*sceneGraph);
        sceneRoot->Name = "mr root";*/
        //
        // Load managed editor module
        //
        const char* group = GetGroup();
        _gameKernel = _engine->AddScriptModule("game.kernel", "xgGameKernel.dll", this, group);
        _editorModule = _engine->AddScriptModule("editor", "xgEditor.CoreCLR.dll", _gameKernel, group);
        _engine->AddScriptModule("game.demo", "GameDemoModule.dll", _gameKernel, group);

       /* auto componentSG = sceneGraphFactory->Create("compset", *actorRegistry);
        auto componentSet = static_cast<ComponentSceneGraph*>(componentSG);
        engine->RegisterSystemWithAlias(nullptr, componentSet, "PlayerComponent");
        
        componentSet->InitializeComponentSet("PlayerComponent", 12);
        ComponentSetNode* co = componentSet->CreateComponent<ComponentSetNode>(55);*/
        //xg::TypeRegistry::Register(&xg::TypeInfo<PlayerState>::Instance);

        CodecRegistry* registry = _host->GetCodecRegistry();

        // Register generic JSON codec
        //registry

        PlayerState state{ 100, 10.0f, 20.0f };

        // Broadcast to all modules
        //const TypeSchema* schema =
            //engine->GetTypeRegistry()->Get("*");

        /*ScriptMessage msg{};
        registry->Encode("PlayerState", PayloadMode::JSON, &state, schema, msg);

        printf("Encoded JSON: %s\n", msg.Payload);

        PlayerState dst{};
        registry->Decode("PlayerState", PayloadMode::JSON, msg, schema, &dst);*/

        auto obj = _engine->CreateDynamic(&TestSchema);

        obj->FindField("Health")->Value.Int32Value = 150;
        obj->FindField("Name")->Value.StringValue = "John";
        
        xg::ScriptMessage msg;
        msg.Payload = nullptr;
        msg.PayloadSize = 0;
        msg.SourceID = GetModuleId();
        bool ok = registry->Encode("TestType", obj.get(), &TestSchema, msg);

        if (!ok)
            printf("Encode failed\n");
        else
        {
            printf("Encoded JSON (%d bytes):\n", msg.PayloadSize);
            fwrite(msg.Payload, 1, msg.PayloadSize, stdout);
            printf("\n");
        }

		//TODO release dynamic object

        _engine->GetSystem<Messenger>(this)->SendToAll(msg);
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
            if (auto dispatcher = _engine->GetSystem<EventDispatcher>())
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

        DrawMainDockspace(static_cast<Engine*>(_engine)->MainWindow.get());
        DrawEditorPanels();

        EndImGuiFrame();
    }

    SceneGraph* EditorKernelModule::GetSceneGraph()
    {
        return _engine->GetSystem<SceneGraph>(this);
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

        ui::ApplyTheme();

        auto* engine = static_cast<Engine*>(_engine);
        engine->MainWindow->SetCustomFrame(true);
        SDL_Window* window = static_cast<SDL_Window*>(engine->MainWindow->GetPlatformWindow());

        if (engine->Renderer.valid())
            fprintf(stderr, "EditorKernel: an engine renderer owns the main window; the editor's swapchain may fail\n");

        SDL_GPUDevice* device = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
            false, nullptr);
        if (!device || !SDL_ClaimWindowForGPUDevice(device, window))
        {
            fprintf(stderr, "EditorKernel: SDL GPU setup failed: %s\n", SDL_GetError());
            if (device)
                SDL_DestroyGPUDevice(device);
            return;
        }
        SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
        _gpuDevice = device;

        ImGui_ImplSDL3_InitForSDLGPU(window);
        ImGui_ImplSDLGPU3_InitInfo initInfo = {};
        initInfo.Device = device;
        initInfo.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
        initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        initInfo.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        initInfo.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
        ImGui_ImplSDLGPU3_Init(&initInfo);

        SDL_Surface* icon = SDL_LoadBMP("../editor/xge-icon.bmp");
        if (icon)
        {
            SDL_SetWindowIcon(window, icon);
            ui::s_appIcon = ui::CreateIconTexture(icon, ui::AppIconSize);
            SDL_DestroySurface(icon);
        }
    }

    void EditorKernelModule::ShutdownImGui()
    {
        SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(_gpuDevice);
        if (device)
        {
            SDL_WaitForGPUIdle(device);
            ImGui_ImplSDL3_Shutdown();
            ImGui_ImplSDLGPU3_Shutdown();
        }

        if (ui::s_appIcon)
        {
            ImGui::UnregisterUserTexture(ui::s_appIcon);
            IM_DELETE(ui::s_appIcon);
            ui::s_appIcon = nullptr;
        }

        if (device)
        {
            SDL_ReleaseWindowFromGPUDevice(device,
                static_cast<SDL_Window*>(static_cast<Engine*>(_engine)->MainWindow->GetPlatformWindow()));
            SDL_DestroyGPUDevice(device);
            _gpuDevice = nullptr;
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
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void EditorKernelModule::EndImGuiFrame()
    {
        ImGui::Render();

        SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(_gpuDevice);
        if (!device)
            return;
        SDL_Window* window = static_cast<SDL_Window*>(static_cast<Engine*>(_engine)->MainWindow->GetPlatformWindow());

        ImDrawData* drawData = ImGui::GetDrawData();
        const bool minimized = drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f;
        SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUTexture* swapchain = nullptr;
        SDL_WaitAndAcquireGPUSwapchainTexture(commands, window, &swapchain, nullptr, nullptr);
        if (swapchain && !minimized)
        {
            ImGui_ImplSDLGPU3_PrepareDrawData(drawData, commands);

            SDL_GPUColorTargetInfo target = {};
            target.texture = swapchain;
            target.clear_color = SDL_FColor{ 13.0f / 255.0f, 13.0f / 255.0f, 13.0f / 255.0f, 1.0f };
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
            ImGui_ImplSDLGPU3_RenderDrawData(drawData, commands, pass);
            SDL_EndGPURenderPass(pass);
        }

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        SDL_SubmitGPUCommandBuffer(commands);
    }

    //
    // ------------------------------------------------------------
    // Event Handling
    // ------------------------------------------------------------
    //
    void EditorKernelModule::OnEvent(const xgEvent& e)
    {
        if (e.Native)
        {
            ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(e.Native));
            return;
        }
        if (e.Type == xgEventType::Platform)
            return;

        SDL_Window* window = static_cast<SDL_Window*>(
            static_cast<Engine*>(_engine)->MainWindow->GetPlatformWindow()
            );

        SDL_Event sdl = xg::ToSDL(e, window);
        ImGui_ImplSDL3_ProcessEvent(&sdl);
    }
    void EditorKernelModule::OnMessage(const ScriptMessage& msg)
    {    
		xg::CodecRegistry* codecs = _host->GetCodecRegistry();
        auto obj2 = _engine->CreateDynamic(&TestSchema);

        bool ok2 = codecs->Decode("TestType", msg, &TestSchema, &obj2);

        if (!ok2)
            printf("Decode failed\n");
        else
        {
            printf("Decoded Health = %d\n", obj2->FindField("Health")->Value.Int32Value);
            printf("Decoded Name   = %s\n", obj2->FindField("Name")->Value.StringValue);
        }
    }
}

//
// ------------------------------------------------------------
// ScriptHostNative factory
// ------------------------------------------------------------
//
extern "C"
XG_MODULE_EXPORT xg::ScriptModule* CreateScriptModule(const char* id, int moduleID, xg::ScriptHost* host, const char* group)
{
    if (host && host->GetEngine())
        xg::SetLogger(host->GetEngine()->GetLogger());
    return new xg::EditorKernelModule(id, moduleID, host, group);
}
