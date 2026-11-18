// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
// Retired prototype UI, kept for reference only. It is never compiled.
#if 0
#pragma once

#include "ImUIInternal.h"

namespace xg::imui::detail
{
    inline bool _playing = false;
    inline bool _paused = false;

    inline ImVec2 _segmentStart;
    inline int _segmentIndex = 0;
    inline float SegmentWidth(int count) { return count * SegButtonSize + (count + 1) * 1.0f; }
    inline ImVec2 BelowLastItem() { return ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 3.0f); }
    inline int _selectDefaultTabs = 0;

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

    enum class DrawMode { Shaded, Wireframe, ShadedWireframe };

    bool StateButton(const char* id, Icon icon, bool on, const char* tip, float size, bool hasMenu = false);
    bool PressedMenuCorner();
    void BeginSegment(int count, ImU32 bg = SegmentBg);
    bool SegButton(const char* id, Icon icon, bool on, const char* tip, bool hasMenu = false);
    void EndSegment();
    bool BeginAnchoredPopup(const char* id, bool openNow, ImVec2 pos);
    float DropdownWidth(const char* label, Icon icon = Icon::None);
    bool BeginDropdown(const char* id, const char* label, Icon icon = Icon::None, float width = 0.0f, ImU32 iconCol = IconColor);
    bool BeginStyledPanel(const char* name, ImU32 bodyCol, bool defaultTab = false, Icon icon = Icon::None);
    bool VerticalIconTabs(const Icon* icons, const ImU32* colors, const char* const* tips, int count, int& active);
    bool BeginPanel(const char* title, Icon icon, ImU32 iconCol, bool defaultOpen = true);
    void EndPanel();
    void PropertyLabel(const char* label, float labelWidth = PropertyLabelWidth);
    void PropertyVec3(const char* name, float* v, float speed, const char* format = "%.2f");
    void DrawGrid(ImDrawList* dl, const ViewCamera& cam);
    void DrawCube(ImDrawList* dl, const ViewCamera& cam, bool selected, DrawMode mode = DrawMode::Shaded, bool lit = true);
    void DrawGroundPlane(ImDrawList* dl, const ViewCamera& cam, float z, float extent, ImU32 col);
    void DrawSceneCamera(ImDrawList* dl, const ViewCamera& view, const Vec3& pos, const Vec3& target);
    void DrawSceneLight(ImDrawList* dl, const ViewCamera& view, const Vec3& pos);
    void DrawToolHandle(ImDrawList* dl, const ViewCamera& view, int tool);
    void DrawNavigationGizmo(ImDrawList* dl, const ViewCamera& view, ImVec2 center);
    void DrawSceneTab();
    void DrawGameTab();
    void DrawSceneView();
    void DrawGameView();
    void DrawInspectorLighting();
    void DrawLighting();
    void DrawProjectTab();
    void DrawProject();
    void DrawMainToolbar();
    void DrawStatusBar();
    void DrawPrototypePanels();
}
#endif
