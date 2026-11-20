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

    // src/scene.lit.hlsl VSInput, the .xdv vertex layout: POSITION, NORMAL (float3), TANGENT (float4, w = handedness),
    // TEXCOORD0 (float2), COLOR0 (RGBA8 unorm).
    struct SceneLitVertex
    {
        float X, Y, Z;
        float NX, NY, NZ;
        float TX, TY, TZ, TW;
        float U, V;
        Color32 Color;
    };
    static_assert(sizeof(SceneLitVertex) == 52, "SceneLitVertex must match scene.lit.hlsl and the .xdv stride");

    // src/scene.lit.hlsl LitConstants (128 bytes, Vulkan's guaranteed push constant size): world-view-projection, the
    // world matrix's first three rows (for normals), then the mode.
    struct SceneLitConstants
    {
        float WorldViewProjection[16];
        float WorldRows[12];
        uint32_t Mode;
        uint32_t Padding[3];
    };
    static_assert(sizeof(SceneLitConstants) == 128, "SceneLitConstants must match scene.lit.hlsl");

}
