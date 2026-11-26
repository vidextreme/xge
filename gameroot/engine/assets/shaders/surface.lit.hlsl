// The built-in lit surface: albedo texture times colour and vertex colour, lit by the editor light. Smoothness softens
// the diffuse falloff until the renderer has real specular lighting.

#include "surface.common.hlsli"

//@group Surface
//@param Color color 1,1,1,1
//@texture Albedo color
//@param Smoothness float 0.5 range=0,1

PSInput VSMain(VSInput input)
{
    return XGSurfaceVS(input);
}

float4 PSMain(PSInput input) : SV_Target
{
    float4 albedo = Texture0.Sample(Sampler0, input.uv) * input.col * Params[0];
    float light = XGLighting(input.normal) * lerp(1.0f, 0.9f, Params[1].x);
    return float4(albedo.rgb * light, albedo.a);
}
