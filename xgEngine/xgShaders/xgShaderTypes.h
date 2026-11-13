// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// CPU-side layouts that match the built-in shaders in xgShaders/src. Shared by every renderer.
#pragma once
#include <cstdint>
#include "xgColor.h"

namespace xg::shaders {

    // src/scene.unlit.hlsl VSInput: POSITION (float3), COLOR0 (RGBA8 unorm).
    struct SceneVertex
    {
        float X, Y, Z;
        Color32 Color;
    };
    static_assert(sizeof(SceneVertex) == 16, "SceneVertex must match the scene.unlit.hlsl input layout");

    // Both shaders take one float4x4 in push constants (Vulkan) or root constants at b0 (DX12).
    inline constexpr uint32_t ConstantsSize = 64;

}
