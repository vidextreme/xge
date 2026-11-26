// The built-in unlit surface: albedo texture times colour and vertex colour, without lighting.

#include "surface.common.hlsli"

//@group Surface
//@param Color color 1,1,1,1
//@texture Albedo color

PSInput VSMain(VSInput input)
{
    return XGSurfaceVS(input);
}

float4 PSMain(PSInput input) : SV_Target
{
    return Texture0.Sample(Sampler0, input.uv) * input.col * Params[0];
}
