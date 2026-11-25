// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// The surface shader contract for project shaders (.hlsl assets compiled to .xds). Include it, then write:
//
//   PSInput VSMain(VSInput input) { return XGSurfaceVS(input); }
//   float4 PSMain(PSInput input) : SV_Target { ... }
//
// Bindings, shared by DX12 and Vulkan:
//   b0 (push constants)  engine constants: world-view-projection, world rows, mode (xg::shaders::SceneLitConstants)
//   b1 (set 0, slot 0)   XGMaterial: float4 Params[4], filled from the material by the //@param annotations
//   t0..t3 (slots 1..4)  material textures, by the //@texture annotations (white when a material leaves one empty)
//   s0 (slot 5)          linear, wrap
//
// Annotations (one per line, in declaration order; at most 4 params and 4 textures):
//   //@group Surface                  following params and textures go in this group (default "General")
//   //@param Roughness float 0.5      float, float2, float3, float4 or color; the default fills the slot's xyzw
//   //@param Tiling float2 2,2 notchtags=u,v range=0,16
//                                     editor-only options: notchtags= (one notch tag per component, quoted when it has
//                                     spaces; "near=#c8a040, far=#a060c8" sets colours) and range=<min>,<max> (clamps and
//                                     draws a range line); colour params default to notchtags=r,g,b,a range=0,1
//   //@texture Albedo color           color, normal or luminance
// Write spaces in names as underscores (Tint_color); the editor shows them as spaces.

#ifndef XGE_SURFACE_COMMON_HLSLI
#define XGE_SURFACE_COMMON_HLSLI

struct XGSurfaceConstants
{
    row_major float4x4 WorldViewProjection;
    float4 WorldRows[3];
    uint Mode;
    uint3 Padding;
};

[[vk::push_constant]] ConstantBuffer<XGSurfaceConstants> XGEngine : register(b0);

[[vk::binding(0, 0)]] cbuffer XGMaterial : register(b1)
{
    float4 Params[4];
};

[[vk::binding(1, 0)]] Texture2D Texture0 : register(t0);
[[vk::binding(2, 0)]] Texture2D Texture1 : register(t1);
[[vk::binding(3, 0)]] Texture2D Texture2 : register(t2);
[[vk::binding(4, 0)]] Texture2D Texture3 : register(t3);
[[vk::binding(5, 0)]] SamplerState Sampler0 : register(s0);

struct VSInput
{
    [[vk::location(0)]] float3 pos     : POSITION;
    [[vk::location(1)]] float3 normal  : NORMAL;
    [[vk::location(2)]] float4 tangent : TANGENT;
    [[vk::location(3)]] float2 uv      : TEXCOORD0;
    [[vk::location(4)]] float4 col     : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    [[vk::location(0)]] float3 normal : NORMAL;
    [[vk::location(1)]] float2 uv     : TEXCOORD0;
    [[vk::location(2)]] float4 col    : COLOR0;
};

float3 XGToLinear(float3 c)
{
    return select(c <= 0.04045f, c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f));
}

PSInput XGSurfaceVS(VSInput input)
{
    PSInput output;
    output.pos = mul(float4(input.pos, 1.0f), XGEngine.WorldViewProjection);
    output.normal = input.normal.x * XGEngine.WorldRows[0].xyz + input.normal.y * XGEngine.WorldRows[1].xyz + input.normal.z * XGEngine.WorldRows[2].xyz;
    output.uv = input.uv;
    output.col = float4(XGToLinear(input.col.rgb), input.col.a);
    return output;
}

// The editor's light: a fixed directional light plus ambient, as in the built-in lit shader.
float XGLighting(float3 normal)
{
    float3 n = normalize(normal);
    float3 l = normalize(float3(-0.4f, 0.8f, -0.45f));
    return 0.3f + 0.7f * saturate(dot(n, l));
}

#endif
