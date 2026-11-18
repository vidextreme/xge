// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgHandles.h"

namespace xg
{
    // Startup splash: a small borderless window shown while the engine loads, before the main window appears.
    // Implemented in xgPlatform; every string is UTF-8 and copied.
    struct SplashDesc
    {
        uint32_t Size;
        const char* Title;
        const char* Tagline;
        const char* Version;
        const char* Config;
        const char* Renderer;
        // Project name shown under the version chips; null or empty hides the line.
        const char* Project;
        const char* Copyright;
        // White mark on a transparent background (PNG or BMP).
        const char* LogoPath;
        // Folder with the Noto Sans fonts (DejaVu Sans as fallback).
        const char* FontDirectory;
    };

    class SplashScreen
    {
    public:
        virtual ~SplashScreen() = default;

        // Current step and progress in [0, 1]; a negative progress shows an indeterminate bar. Null text keeps the step.
        virtual void SetStep(const char* text, float progress) = 0;
        // Second line under the bar (a file, a build output line); null or empty clears it.
        virtual void SetDetail(const char* text) = 0;
        // Handles the splash window's events and redraws it, at most once every 16 ms.
        virtual void Pump() = 0;
    };

    XG_DECLARE_UNIQUE(SplashScreen)
    // Null when there is no display or the window could not be created.
    XG_API SplashScreenUnique NewSplashScreen(const SplashDesc& desc);
}
