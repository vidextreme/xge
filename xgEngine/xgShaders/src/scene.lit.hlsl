// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// Imported meshes in render views (xg::shaders::SceneLitVertex, the .xdv vertex layout): texture times vertex color,
// lit by a fixed directional light plus ambient. Colors are linear (render views write through an sRGB target).

struct LitConstants
{
    row_major float4x4 WorldViewProjection;
    float4 WorldRows[3];
    uint Mode;
    uint3 Padding;
};

[[vk::push_constant]] ConstantBuffer<LitConstants> _constants : register(b0);

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

float3 ToLinear(float3 c)
{
    return select(c <= 0.04045f, c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f));
}

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.pos = mul(float4(input.pos, 1.0f), _constants.WorldViewProjection);
    output.normal = input.normal.x * _constants.WorldRows[0].xyz + input.normal.y * _constants.WorldRows[1].xyz + input.normal.z * _constants.WorldRows[2].xyz;
    output.uv = input.uv;
    output.col = float4(ToLinear(input.col.rgb), input.col.a);
    return output;
}

[[vk::binding(0, 0)]] Texture2D Texture0 : register(t0);
[[vk::binding(1, 0)]] SamplerState Sampler0 : register(s0);

float4 PSMain(PSInput input) : SV_Target
{
    float3 n = normalize(input.normal);
    float3 l = normalize(float3(-0.4f, 0.8f, -0.45f));
    float light = 0.3f + 0.7f * saturate(dot(n, l));
    float4 albedo = Texture0.Sample(Sampler0, input.uv) * input.col;
    return float4(albedo.rgb * light, albedo.a);
}
