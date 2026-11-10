// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgImUIGui.hpp"

namespace xg
{
    // Test hook: exercises the native xg::imui API (widgets + immediate drawing). Mirrors the managed "Managed Test".
    // Set XG_UI_TEST_UNBALANCED=1 to leave scopes open every frame and exercise frame recovery,
    // and XG_UI_TEST_FOCUS=native to bring it to the front after startup.
    class NativeTestWindow : public imui::Panel
    {
    public:
        NativeTestWindow();

    protected:
        void OnGUI(imui::Gui& gui) override;

    private:
        float _speed = 2.5f;
        int _clicks = 0;
        bool _unbalanced = false;
        bool _reportedDraw = false;
        bool _reportedRecovery = false;
    };

    // True when XG_UI_TEST_FLOAT_OUTSIDE=1: floating test panels start partly outside the main window (own OS window).
    bool TestFloatOutside();

    // Test hook: a floating (undocked) native panel with a live animated canvas.
    class NativeFloatingWindow : public imui::Panel
    {
    public:
        NativeFloatingWindow();

    protected:
        void OnGUI(imui::Gui& gui) override;

    private:
        double _lastTime = 0.0;
        float _angle = 0.0f;
        int _clicks = 0;
    };
}
