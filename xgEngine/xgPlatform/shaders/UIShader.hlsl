// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
// Rebuild the blobs in UIShaderBlobs.h with compile_ui_shaders.ps1 (DXBC, DXIL and SPIR-V for SDL GPU).
// SDL GPU's SPIR-V layout: vertex uniforms in set 1, fragment texture+sampler as one combined binding in set 2.

cbuffer UIConstants : register(b0, space1)
{
    float2 Scale;
    float2 Translate;
};

#ifdef __spirv__
#define XG_LOCATION(n) [[vk::location(n)]]
#else
#define XG_LOCATION(n)
#endif

struct VSInput
{
    XG_LOCATION(0) float2 Position : TEXCOORD0;
    XG_LOCATION(1) float2 UV : TEXCOORD1;
    XG_LOCATION(2) float4 Color : TEXCOORD2;
};

struct VSOutput
{
    XG_LOCATION(0) float4 Color : TEXCOORD0;
    XG_LOCATION(1) float2 UV : TEXCOORD1;
    float4 Position : SV_Position;
};

#ifdef __spirv__
[[vk::combinedImageSampler]] [[vk::binding(0, 2)]] Texture2D<float4> UITexture : register(t0, space2);
[[vk::combinedImageSampler]] [[vk::binding(0, 2)]] SamplerState UISampler : register(s0, space2);
#else
Texture2D<float4> UITexture : register(t0, space2);
SamplerState UISampler : register(s0, space2);
#endif

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float2 p = input.Position * Scale + Translate;
    output.Position = float4(p.x, -p.y, 0.0f, 1.0f);
    output.Color = input.Color;
    output.UV = input.UV;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    return input.Color * UITexture.Sample(UISampler, input.UV);
}
