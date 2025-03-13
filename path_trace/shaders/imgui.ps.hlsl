struct PS_INPUT
{
  float4 pos : SV_POSITION;
  float4 col : COLOR0;
  float2 uv  : TEXCOORD0;
};

struct PushConstants {
    uint2 buffer;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pushConstants : register(b0);

[[vk::binding(0, 1)]]Texture2D texture_ui[] : register(t0, space1);
[[vk::binding(0, 2)]]SamplerState sampler_ui[] : register(s0, space2);

float4 main(PS_INPUT input) : SV_Target
{
  float4 out_col = input.col * texture_ui[pushConstants.buffer.x].Sample(sampler_ui[pushConstants.buffer.y], input.uv); 
  return out_col; 
}