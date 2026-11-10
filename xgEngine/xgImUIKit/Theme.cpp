// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "ImUIInternal.h"

namespace xg::imui::detail
{
    void ApplyTheme()
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

    ImTextureData* CreateIconTexture(const uint8_t* rgba, int width, int height, int size)
    {
        if (!rgba || width <= 0 || height <= 0 || size <= 0)
            return nullptr;
        ImTextureData* texture = IM_NEW(ImTextureData)();
        texture->Create(ImTextureFormat_RGBA32, size, size);
        for (int y = 0; y < size; ++y)
        {
            const int sy0 = y * height / size, sy1 = ImMax(sy0 + 1, (y + 1) * height / size);
            unsigned char* out = static_cast<unsigned char*>(texture->GetPixelsAt(0, y));
            for (int x = 0; x < size; ++x)
            {
                const int sx0 = x * width / size, sx1 = ImMax(sx0 + 1, (x + 1) * width / size);
                unsigned sum = 0, count = 0;
                for (int sy = sy0; sy < sy1; ++sy)
                {
                    const uint8_t* in = rgba + static_cast<size_t>(sy) * width * 4;
                    for (int sx = sx0; sx < sx1; ++sx, ++count)
                    {
                        const uint8_t* px = in + sx * 4;
                        sum += (px[0] * 54u + px[1] * 183u + px[2] * 19u) >> 8;
                    }
                }
                out[x * 4 + 0] = out[x * 4 + 1] = out[x * 4 + 2] = 255;
                out[x * 4 + 3] = static_cast<unsigned char>(sum / count);
            }
        }
        ImGui::RegisterUserTexture(texture);
        return texture;
    }
}
