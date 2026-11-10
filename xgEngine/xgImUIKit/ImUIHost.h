// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "imgui.h"

namespace xg { class UIRenderTarget; }

namespace xg::imui::detail
{
    // Panel registry (Api.cpp), driven by the host frame and the shell.
    void DrawRegisteredPanels();
    void DockRegisteredPanels(ImGuiID center, ImGuiID left, ImGuiID right, ImGuiID bottom);
    void DrawRegisteredPanelsMenu();
    void DrawRegisteredMenuItems(const char* topLevel);
    void DrawExtraTopLevelMenus(const char* const* builtIn, int builtInCount);
    void ReleaseUserTextures();
    void SetDefaultDockSpace(ImGuiID id);

    // Host state (Host.cpp).
    UIRenderTarget* HostRenderTarget();
    bool HostPrototypeShell();
}
