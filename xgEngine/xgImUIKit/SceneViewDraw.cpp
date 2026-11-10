// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "ImUIInternal.h"
#include <cmath>

namespace xg::imui::detail
{
    void DrawGrid(ImDrawList* dl, const ViewCamera& cam)
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

    void DrawCube(ImDrawList* dl, const ViewCamera& cam, bool selected, DrawMode mode, bool lit)
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

    void DrawGroundPlane(ImDrawList* dl, const ViewCamera& cam, float z, float extent, ImU32 col)
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

    void DrawSceneCamera(ImDrawList* dl, const ViewCamera& view, const Vec3& pos, const Vec3& target)
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

    void DrawSceneLight(ImDrawList* dl, const ViewCamera& view, const Vec3& pos)
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

    void DrawToolHandle(ImDrawList* dl, const ViewCamera& view, int tool)
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

    void DrawNavigationGizmo(ImDrawList* dl, const ViewCamera& view, ImVec2 center)
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
}
