// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
// Rebuild the blobs in UIShaderBlobs.h with compile_ui_shaders.bat.

cbuffer UIConstants : register(b0, space1)
{
    float2 Scale;
    float2 Translate;
};

struct VSInput
{
    float2 Position : TEXCOORD0;
    float2 UV : TEXCOORD1;
    float4 Color : TEXCOORD2;
};

struct VSOutput
{
    float4 Color : TEXCOORD0;
    float2 UV : TEXCOORD1;
    float4 Position : SV_Position;
};

Texture2D<float4> UITexture : register(t0, space2);
SamplerState UISampler : register(s0, space2);

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
