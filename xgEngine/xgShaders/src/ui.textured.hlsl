// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// UI geometry (xg::UIVertex): projection in push/root constants, one texture per draw.

struct UIConstants
{
    float4x4 ProjectionMatrix;
};

[[vk::push_constant]] ConstantBuffer<UIConstants> _constants : register(b0);

struct VSInput
{
    [[vk::location(0)]] float2 pos : POSITION;
    [[vk::location(1)]] float2 uv  : TEXCOORD0;
    [[vk::location(2)]] float4 col : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    [[vk::location(0)]] float4 col : COLOR0;
    [[vk::location(1)]] float2 uv  : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.pos = mul(_constants.ProjectionMatrix, float4(input.pos.xy, 0.0f, 1.0f));
    output.col = input.col;
    output.uv = input.uv;
    return output;
}

[[vk::binding(0, 0)]] Texture2D Texture0 : register(t0);
[[vk::binding(1, 0)]] SamplerState Sampler0 : register(s0);

float4 PSMain(PSInput input) : SV_Target
{
    return input.col * Texture0.Sample(Sampler0, input.uv);
}
