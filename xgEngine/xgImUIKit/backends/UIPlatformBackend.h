// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

namespace xg {
    class Window;
    class PlatformServices;
    struct xgEvent;
}

// ImGui platform backend over xg::Window and xg::xgEvent (xgBase only, no OS or SDL headers).
namespace xg::imui::detail::platform
{
    // services non-null enables multi-viewport platform callbacks (secondary OS windows).
    bool Init(Window* window, PlatformServices* services);
    void Shutdown();
    void NewFrame(float deltaTime);
    void ProcessEvent(const xgEvent& e);
    // Shows viewport windows ImGui asked to show this frame, after they have been rendered once.
    void ShowPendingWindows();
    // Content scale of the main window (or the XG_UI_DPI_SCALE override); 1 before Init.
    float DisplayScale();
}
