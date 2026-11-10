// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgBase.h"
#include "xgWindow.h"

namespace xg {

    enum WindowCreateFlags : uint32_t
    {
        WindowCreateFlagsNone       = 0,
        WindowCreateFlagsBorderless = 1u << 0,
        WindowCreateFlagsNoTaskbar  = 1u << 1,
        WindowCreateFlagsHidden     = 1u << 2,
        WindowCreateFlagsTopMost    = 1u << 3,
    };

    struct WindowDesc
    {
        const char* Title;
        int X;
        int Y;
        int Width;
        int Height;
        uint32_t Flags;
    };

    struct MonitorInfo
    {
        WindowRect Bounds;
        WindowRect WorkBounds;
        float DpiScale;
    };

    // OS services beyond the main window: secondary windows, monitors, global mouse.
    // Windows created here share the main window's event pump; their events carry xgEvent::Source.
    class PlatformServices {
    public:
        virtual ~PlatformServices() = default;

        virtual Window* CreateSecondaryWindow(const WindowDesc& desc) = 0;
        virtual void DestroySecondaryWindow(Window* window) = 0;
        virtual int GetMonitors(MonitorInfo* monitors, int maxCount) = 0;
        virtual bool GetGlobalMousePosition(float* x, float* y) = 0;
    };

    // Implemented in xgPlatform. Valid while the main window exists.
    XG_API PlatformServices* GetPlatformServices();

} // namespace xg
