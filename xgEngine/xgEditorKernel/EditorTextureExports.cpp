// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include <algorithm>
#include <cstdint>
#include "xgBase.h"

#define STB_DXT_IMPLEMENTATION
#include "../3rdparty/stb/stb_dxt.h"

namespace
{
    constexpr int32_t _formatBC1 = 2;
    constexpr int32_t _formatBC3 = 3;
    constexpr int32_t _formatBC4 = 4;
    constexpr int32_t _formatBC5 = 5;
    constexpr int32_t _qualityHigh = 1;

    int32_t BytesPerBlock(int32_t format)
    {
        return format == _formatBC1 || format == _formatBC4 ? 8 : format == _formatBC3 || format == _formatBC5 ? 16 : 0;
    }
}

// Compresses tightly packed RGBA8 pixels into 4x4 blocks in row order. Edge blocks repeat the last pixels.
// Returns the number of bytes written, or -1 for invalid arguments or a too small output buffer.
XG_MODULE_EXPORT int64_t XG_EditorEncodeBlocks(int32_t format, int32_t quality, const uint8_t* rgba, int32_t width, int32_t height, uint8_t* output, int64_t capacity)
{
    const int32_t blockBytes = BytesPerBlock(format);
    if (blockBytes == 0 || !rgba || !output || width <= 0 || height <= 0)
        return -1;
    const int64_t blocksX = (static_cast<int64_t>(width) + 3) / 4;
    const int64_t blocksY = (static_cast<int64_t>(height) + 3) / 4;
    const int64_t size = blocksX * blocksY * blockBytes;
    if (capacity < size)
        return -1;

    const int mode = quality == _qualityHigh ? STB_DXT_HIGHQUAL : STB_DXT_NORMAL;
    uint8_t* cursor = output;
    for (int32_t blockY = 0; blockY < blocksY; ++blockY)
    {
        for (int32_t blockX = 0; blockX < blocksX; ++blockX)
        {
            uint8_t pixels[64];
            uint8_t red[16];
            uint8_t redGreen[32];
            for (int32_t y = 0; y < 4; ++y)
            {
                const int32_t sourceY = (std::min)(blockY * 4 + y, height - 1);
                for (int32_t x = 0; x < 4; ++x)
                {
                    const int32_t sourceX = (std::min)(blockX * 4 + x, width - 1);
                    const uint8_t* source = rgba + (static_cast<int64_t>(sourceY) * width + sourceX) * 4;
                    const int32_t index = y * 4 + x;
                    std::copy(source, source + 4, pixels + index * 4);
                    red[index] = source[0];
                    redGreen[index * 2] = source[0];
                    redGreen[index * 2 + 1] = source[1];
                }
            }
            if (format == _formatBC1)
                stb_compress_dxt_block(cursor, pixels, 0, mode);
            else if (format == _formatBC3)
                stb_compress_dxt_block(cursor, pixels, 1, mode);
            else if (format == _formatBC4)
                stb_compress_bc4_block(cursor, red);
            else
                stb_compress_bc5_block(cursor, redGreen);
            cursor += blockBytes;
        }
    }
    return size;
}
