#include "shared.hlsli"
struct PS_INPUT {
    float2 uv : TEXCOORD0;
    float4 pos : SV_POSITION;
};

[[vk::push_constant]] ConstantBuffer<FrameIndex> pushConstants : register(b4);
[[vk::binding(0, 3)]] RWTexture2D<float4> texture_rt[] : register(u0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    uint width, height;
    texture_rt[pushConstants.frameIndex].GetDimensions(width, height);

    // Convert UV coordinates to integer texel coordinates
    int2 texelCoords = int2(input.uv * float2(width, height));
    float4 out_col = texture_rt[pushConstants.frameIndex].Load(texelCoords);
    return pow(out_col, 1.0 / 2.2);
}