struct VSOut
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 main(VSOut input) : SV_TARGET
{
    //return float4(input.uv.x / input.uv.y, 0, 0, 1);
    return g_texture.Sample(g_sampler, input.uv);
}