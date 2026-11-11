// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "ImUIInternal.h"
#include <cmath>

namespace xg::imui::detail
{
    void Arrow(ImDrawList* dl, ImVec2 from, ImVec2 to, ImU32 col, float head, float thickness)
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

    void BadgeText(ImDrawList* dl, ImVec2 c, const char* text)
    {
        ImVec2 ts = ImGui::CalcTextSize(text);
        dl->AddText(ImVec2(ImFloor(c.x - ts.x * 0.5f), ImFloor(c.y - ts.y * 0.5f)), IM_COL32(24, 24, 24, 255), text);
    }

    void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float s, ImU32 col)
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
        case Icon::Image:
            dl->AddRect(ImVec2(c.x - h * 0.9f, c.y - h * 0.7f), ImVec2(c.x + h * 0.9f, c.y + h * 0.7f), col, 1.5f, 0, 1.3f);
            dl->AddCircleFilled(ImVec2(c.x + h * 0.4f, c.y - h * 0.3f), h * 0.16f, col);
            dl->AddTriangleFilled(ImVec2(c.x - h * 0.7f, c.y + h * 0.5f), ImVec2(c.x - h * 0.2f, c.y - h * 0.15f), ImVec2(c.x + h * 0.3f, c.y + h * 0.5f), col);
            dl->AddTriangleFilled(ImVec2(c.x, c.y + h * 0.5f), ImVec2(c.x + h * 0.35f, c.y + h * 0.1f), ImVec2(c.x + h * 0.7f, c.y + h * 0.5f), col);
            break;
        case Icon::Trash:
            dl->AddLine(ImVec2(c.x - h * 0.85f, c.y - h * 0.55f), ImVec2(c.x + h * 0.85f, c.y - h * 0.55f), col, 1.4f);
            dl->AddRectFilled(ImVec2(c.x - h * 0.3f, c.y - h * 0.85f), ImVec2(c.x + h * 0.3f, c.y - h * 0.6f), col, 1.0f);
            dl->AddQuad(ImVec2(c.x - h * 0.65f, c.y - h * 0.4f), ImVec2(c.x + h * 0.65f, c.y - h * 0.4f),
                ImVec2(c.x + h * 0.5f, c.y + h * 0.85f), ImVec2(c.x - h * 0.5f, c.y + h * 0.85f), col, 1.3f);
            dl->AddLine(ImVec2(c.x - h * 0.2f, c.y - h * 0.15f), ImVec2(c.x - h * 0.17f, c.y + h * 0.6f), col, 1.2f);
            dl->AddLine(ImVec2(c.x + h * 0.2f, c.y - h * 0.15f), ImVec2(c.x + h * 0.17f, c.y + h * 0.6f), col, 1.2f);
            break;
        }
    }
}
