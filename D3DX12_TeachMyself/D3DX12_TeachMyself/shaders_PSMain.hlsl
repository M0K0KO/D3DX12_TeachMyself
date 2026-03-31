Texture2D baseColorTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D metallicRoughnessTex : register(t2);
SamplerState samp : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET
{
    return baseColorTex.Sample(samp, input.uv);
}