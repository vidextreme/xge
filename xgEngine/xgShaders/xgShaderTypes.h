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

    // src/scene.textured.hlsl VSInput: POSITION (float3), TEXCOORD0 (float2), COLOR0 (RGBA8 unorm).
    struct SceneTexturedVertex
    {
        float X, Y, Z;
        float U, V;
        Color32 Color;
    };
    static_assert(sizeof(SceneTexturedVertex) == 24, "SceneTexturedVertex must match the scene.textured.hlsl input layout");

    // src/scene.textured.hlsl SceneConstants: the view-projection, then the texture mode, padded to 16 bytes.
    struct SceneTexturedConstants
    {
        float ViewProjection[16];
        uint32_t Mode;
        uint32_t Padding[3];
    };
    static_assert(sizeof(SceneTexturedConstants) == 80, "SceneTexturedConstants must match scene.textured.hlsl");

}
