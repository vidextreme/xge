// A lit surface: albedo texture times vertex color, tinted through an optional mask.

#include "surface.common.hlsli"

//@group Surface
//@texture Albedo color
//@param Roughness float 0.5
//@group Tint
//@param Tint_color color 1,1,1,1
//@texture Tint_mask luminance
//@param Mask_strength float 1

PSInput VSMain(VSInput input)
{
    return XGSurfaceVS(input);
}

float4 PSMain(PSInput input) : SV_Target
{
    float4 albedo = Texture0.Sample(Sampler0, input.uv) * input.col;
    float mask = Texture1.Sample(Sampler0, input.uv).r * Params[2].x;
    float3 tint = lerp(float3(1.0f, 1.0f, 1.0f), Params[1].rgb, saturate(mask));
    float light = XGLighting(input.normal) * lerp(1.0f, 0.85f, Params[0].x);
    return float4(albedo.rgb * tint * light, albedo.a);
}
