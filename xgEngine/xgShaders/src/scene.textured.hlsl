// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// Textured test geometry for render views (xg::shaders::SceneTexturedVertex): vertex color times one texture.
// Render views write through an sRGB target, so colors are linear here: vertex colors are converted from sRGB.
// Mode (xg::RenderViewTextureMode): 0 color, 1 tangent-space normal in RG (Z rebuilt, shown as a color), 2 gray from R.

struct SceneConstants
{
    row_major float4x4 ViewProjection;
    uint Mode;
    uint3 Padding;
};

[[vk::push_constant]] ConstantBuffer<SceneConstants> _constants : register(b0);

struct VSInput
{
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float2 uv  : TEXCOORD0;
    [[vk::location(2)]] float4 col : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    [[vk::location(0)]] float4 col : COLOR0;
    [[vk::location(1)]] float2 uv  : TEXCOORD0;
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
    output.uv = input.uv;
    return output;
}

[[vk::binding(0, 0)]] Texture2D Texture0 : register(t0);
[[vk::binding(1, 0)]] SamplerState Sampler0 : register(s0);

float4 PSMain(PSInput input) : SV_Target
{
    float4 texel = Texture0.Sample(Sampler0, input.uv);
    if (_constants.Mode == 1)
    {
        float2 xy = texel.rg * 2.0f - 1.0f;
        float3 n = float3(xy, sqrt(saturate(1.0f - dot(xy, xy))));
        texel = float4(ToLinear(n * 0.5f + 0.5f), 1.0f);
    }
    else if (_constants.Mode == 2)
    {
        texel = float4(texel.rrr, 1.0f);
    }
    return input.col * texel;
}
