// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "ImUIInternal.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace xg::imui::detail
{
    void DrawSceneTab()
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

    void DrawGameTab()
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

    void DrawSceneView()
    {
        if (BeginStyledPanel("Scene", ViewportBg, true))
            DrawSceneTab();
        ImGui::End();
    }

    void DrawGameView()
    {
        if (BeginStyledPanel("Game", ViewportBg))
            DrawGameTab();
        ImGui::End();
    }

    void DrawInspectorLighting()
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

    void DrawLighting()
    {
        if (BeginStyledPanel("Lighting", HeaderBg))
            DrawInspectorLighting();
        ImGui::End();
    }

    void DrawProjectTab()
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

    void DrawProject()
    {
        if (BeginStyledPanel("Project", AreaBg, true))
            DrawProjectTab();
        ImGui::End();
    }

}
