// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgBase.h"

namespace xg {

    // Tightly packed 8-bit RGBA pixels, row pitch = Width * 4.
    struct ImageRGBA
    {
        int Width;
        int Height;
        uint8_t* Pixels;
    };

    // Implemented in xgPlatform. Returns false and leaves the image empty on failure.
    XG_API bool LoadImageRGBA(const char* path, ImageRGBA* image);
    XG_API void FreeImageRGBA(ImageRGBA* image);

} // namespace xg
