// The error look: a magenta and dark checker in screen space, drawn for materials whose shader or data failed to load.

#include "surface.common.hlsli"

PSInput VSMain(VSInput input)
{
    return XGSurfaceVS(input);
}

float4 PSMain(PSInput input) : SV_Target
{
    uint2 cell = uint2(input.pos.xy) / 8u;
    float3 color = ((cell.x + cell.y) & 1u) != 0u ? float3(1.0f, 0.0f, 1.0f) : float3(0.12f, 0.0f, 0.12f);
    return float4(color * lerp(0.75f, 1.0f, XGLighting(input.normal)), 1.0f);
}
