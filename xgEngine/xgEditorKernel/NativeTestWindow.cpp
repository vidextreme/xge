// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "NativeTestWindow.h"
#include "xgLog.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace xg
{
    NativeTestWindow::NativeTestWindow()
    {
        char* value = nullptr;
        size_t length = 0;
        if (_dupenv_s(&value, &length, "XG_UI_TEST_UNBALANCED") == 0 && value)
        {
            _unbalanced = value[0] == '1';
            free(value);
        }

        Settings settings;
        settings.Title = "Native Test";
        settings.MenuPath = "Tests/Native Test";
        settings.Slot = imui::DockSlot::Right;
        settings.PanelIcon = imui::Icon::Script;
        Register(settings);

        if (_dupenv_s(&value, &length, "XG_UI_TEST_FOCUS") == 0 && value)
        {
            if (std::strcmp(value, "native") == 0)
                Focus();
            free(value);
        }
    }

    void NativeTestWindow::OnGUI(imui::Gui& gui)
    {
        gui.Label("Native xg::imui panel");
        if (gui.Button("Click me"))
            xg::Log(MessageType::Info, "[Debug] Native Test: button clicked (%d)", ++_clicks);
        if (gui.Field("Speed", _speed))
            xg::Log(MessageType::Info, "[Debug] Native Test: speed changed to %.2f", _speed);

        auto canvas = gui.Canvas("##native_canvas", Vec2(0.0f, 160.0f));
        const imui::Painter draw = canvas.Draw();
        const Vec2 size = canvas.Size();
        const Color32 gridColor(255, 255, 255, 24);
        draw.DrawRectFilled(Rect(Vec2(0.0f, 0.0f), size), imui::Style(imui::StyleColor::ViewportBg), 4.0f);
        for (float x = 16.0f; x < size.x; x += 16.0f)
            draw.Line(Vec2(x, 0.0f), Vec2(x, size.y), gridColor);
        for (float y = 16.0f; y < size.y; y += 16.0f)
            draw.Line(Vec2(0.0f, y), Vec2(size.x, y), gridColor);
        draw.Circle(size * 0.5f, 28.0f + _speed * 4.0f, imui::Style(imui::StyleColor::Accent), 2.0f);
        draw.CircleFilled(size * 0.5f, 6.0f, imui::Style(imui::StyleColor::ActiveObject));
        draw.DrawIcon(imui::Icon::Script, Vec2(20.0f, 20.0f), 16.0f, imui::Style(imui::StyleColor::Icon));
        draw.Text(Vec2(34.0f, 12.0f), imui::Style(imui::StyleColor::Text), "Painter (native)");

        if (!_reportedDraw)
        {
            _reportedDraw = true;
            xg::Log(MessageType::Info, "[Debug] Native Test: drew canvas %.0fx%.0f", size.x, size.y);
        }

        if (_unbalanced)
        {
            XG_ImUIPushID(gui.Native(), "leaked-id");
            const Vec2 leakedSize(10.0f, 10.0f);
            Rect leaked;
            XG_ImUIBeginCanvas(gui.Native(), "##leaked_canvas", &leakedSize, &leaked);
            if (!_reportedRecovery)
            {
                _reportedRecovery = true;
                xg::Log(MessageType::Info, "[Debug] Native Test: left scopes open, frame must recover");
            }
        }
    }

    bool TestFloatOutside()
    {
        char* value = nullptr;
        size_t length = 0;
        bool outside = false;
        if (_dupenv_s(&value, &length, "XG_UI_TEST_FLOAT_OUTSIDE") == 0 && value)
        {
            outside = value[0] == '1';
            free(value);
        }
        return outside;
    }

    NativeFloatingWindow::NativeFloatingWindow()
    {
        Settings settings;
        settings.Title = "Native Floating";
        settings.MenuPath = "Tests/Native Floating";
        settings.Slot = imui::DockSlot::Floating;
        settings.PanelIcon = imui::Icon::Script;
        settings.InitialRect = TestFloatOutside() ? Rect::FromXYWH(1100.0f, 60.0f, 300.0f, 230.0f) : Rect::FromXYWH(250.0f, 140.0f, 300.0f, 230.0f);
        Register(settings);
    }

    void NativeFloatingWindow::OnGUI(imui::Gui& gui)
    {
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if (_lastTime > 0.0)
            _angle += static_cast<float>(now - _lastTime) * 1.5f;
        _lastTime = now;

        gui.Label("Floating native panel");
        if (gui.Button("Count"))
            xg::Log(MessageType::Info, "[Debug] Native Floating: count %d", ++_clicks);

        auto canvas = gui.Canvas("##native_floating_canvas");
        const imui::Painter draw = canvas.Draw();
        const Vec2 size = canvas.Size();
        const Vec2 center = size * 0.5f;
        const float radius = (std::min)(size.x, size.y) * 0.4f;
        draw.DrawRectFilled(Rect(Vec2(0.0f, 0.0f), size), imui::Style(imui::StyleColor::ViewportBg), 4.0f);
        draw.Circle(center, radius, imui::Style(imui::StyleColor::Grid), 1.0f);
        draw.Line(center, center + Vec2(std::cos(_angle), std::sin(_angle)) * radius, imui::Style(imui::StyleColor::Accent), 3.0f);
        draw.CircleFilled(center, 4.0f, imui::Style(imui::StyleColor::ActiveObject));
    }
}
