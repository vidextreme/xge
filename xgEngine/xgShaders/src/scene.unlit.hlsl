// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// Test scene for render views (xg::shaders::SceneVertex): unlit vertex colors. Render views write through an sRGB
// target, so the sRGB vertex colors are converted to linear.

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

float3 ToLinear(float3 c)
{
    return select(c <= 0.04045f, c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f));
}

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.pos = mul(float4(input.pos, 1.0f), _constants.ViewProjection);
    output.col = float4(ToLinear(input.col.rgb), input.col.a);
    return output;
}

float4 PSMain(PSInput input) : SV_Target
{
    return input.col;
}
