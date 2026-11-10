// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgImUIGui.hpp"
#include "EditorSceneViewExports.h"

namespace xg
{
    // Test hook: a panel showing an offscreen 3D render view with an orbit camera (left-drag orbits, wheel zooms).
    // Several instances show independent views. Closed by default; XG_UI_TEST_FOCUS=scene opens them at startup.
    class SceneViewWindow : public imui::Panel
    {
    public:
        SceneViewWindow(const char* title, const char* menuPath, const Rect& initialRect, float yaw, const float clearColor[3], bool forceOpen = false);
        ~SceneViewWindow() override;

    protected:
        void OnGUI(imui::Gui& gui) override;

    private:
        void UpdateCamera();

        uint64_t _view = 0;
        int _viewWidth = 0;
        int _viewHeight = 0;
        float _yaw = 0.0f;
        float _pitch = 0.45f;
        float _distance = 5.0f;
        Color _clear;
        bool _reported = false;
        imui::SceneViewState _state;
    };
}
