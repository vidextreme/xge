// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "SceneViewWindow.h"
#include "xgLog.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace xg
{
    SceneViewWindow::SceneViewWindow(const char* title, const char* menuPath, const Rect& initialRect, float yaw, const float clearColor[3], bool forceOpen)
        : _yaw(yaw)
    {
        _clear = Color(clearColor[0], clearColor[1], clearColor[2], 1.0f);
        _state.Placeholder = "No 3D renderer (RendererModule is empty)";

        bool open = forceOpen;
        char* value = nullptr;
        size_t length = 0;
        if (_dupenv_s(&value, &length, "XG_UI_TEST_FOCUS") == 0 && value)
        {
            open = open || std::strcmp(value, "scene") == 0;
            free(value);
        }

        Settings settings;
        settings.Title = title;
        settings.MenuPath = menuPath;
        settings.Slot = imui::DockSlot::Floating;
        settings.PanelIcon = imui::Icon::SceneGrid;
        settings.Flags = imui::PanelFlags::NoPadding | imui::PanelFlags::NoScrollbar | (open ? imui::PanelFlags::None : imui::PanelFlags::StartClosed);
        settings.InitialRect = initialRect;
        Register(settings);
    }

    SceneViewWindow::~SceneViewWindow()
    {
        Unregister();
        if (_view)
            XG_EditorDestroySceneView(_view);
    }

    void SceneViewWindow::UpdateCamera()
    {
        RenderViewCamera camera = {};
        camera.Target = Vec3(0.0f, 0.5f, 0.0f);
        camera.Eye = camera.Target + Vec3(_distance * std::cos(_pitch) * std::sin(_yaw), _distance * std::sin(_pitch), -_distance * std::cos(_pitch) * std::cos(_yaw));
        camera.Up = Vec3(0.0f, 1.0f, 0.0f);
        camera.FovY = 1.0471976f;
        camera.NearZ = 0.1f;
        camera.FarZ = 100.0f;
        camera.ClearColor = _clear;
        XG_EditorSetSceneViewCamera(_view, &camera);
    }

    void SceneViewWindow::OnGUI(imui::Gui& gui)
    {
        _state.TextureId = _view;
        gui.SceneView("##scene_view", _state);

        _yaw += _state.DragLeft.x * 0.01f;
        _pitch = (std::max)(-1.4f, (std::min)(1.4f, _pitch + _state.DragLeft.y * 0.01f));
        _distance = (std::max)(1.5f, (std::min)(40.0f, _distance * std::pow(0.9f, _state.Wheel)));

        const int width = _state.PixelWidth();
        const int height = _state.PixelHeight();
        if (!XG_EditorHasSceneRenderer() || width <= 0 || height <= 0)
            return;

        if (!_view)
        {
            _view = XG_EditorCreateSceneView(width, height);
            _viewWidth = width;
            _viewHeight = height;
        }
        else if (width != _viewWidth || height != _viewHeight)
        {
            XG_EditorResizeSceneView(_view, width, height);
            _viewWidth = width;
            _viewHeight = height;
        }
        if (!_view)
            return;
        UpdateCamera();

        if (!_reported && width > 64)
        {
            _reported = true;
            xg::Log(MessageType::Info, "[Debug] Scene view: %dx%d, texture %llu", width, height, static_cast<unsigned long long>(_view));
        }
    }
}
