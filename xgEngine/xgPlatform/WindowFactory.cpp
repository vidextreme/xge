// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "WindowSDL.h"

namespace xg {

    XG_DECLARE_UNIQUE_TYPE(WindowSDL)
    XG_API WindowUnique NewWindow(const char* title, int width, int height) {
        return WindowUnique(MakeWindowSDLUniqueHandle(new WindowSDL(title, width, height)));
    }

    XG_API WindowUnique NewWindow(const char* title, int width, int height, uint32_t flags) {
        return WindowUnique(MakeWindowSDLUniqueHandle(new WindowSDL(title, width, height, flags)));
    }

} // namespace xg


