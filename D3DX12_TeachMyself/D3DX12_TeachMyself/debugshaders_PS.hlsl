struct VSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 main(VSOut input) : SV_Target
{
    float4 value = g_texture.Sample(g_sampler, input.uv);
    return float4(value);
}