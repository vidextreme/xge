// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// Test scene for render views (xg::shaders::SceneVertex): unlit vertex colors.

struct SceneConstants
{
    row_major float4x4 ViewProjection;
};

[[vk::push_constant]] ConstantBuffer<SceneConstants> _constants : register(b0);

struct VSInput
{
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float4 col : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    [[vk::location(0)]] float4 col : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.pos = mul(float4(input.pos, 1.0f), _constants.ViewProjection);
    output.col = input.col;
    return output;
}

float4 PSMain(PSInput input) : SV_Target
{
    return input.col;
}
