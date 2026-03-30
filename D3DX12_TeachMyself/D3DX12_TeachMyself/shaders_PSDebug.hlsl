struct VSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 main(VSOut input) : SV_Target
{
    float depth = g_texture.Sample(g_sampler, input.uv).r;

    float nearZ = 0.1f;
    float farZ = 1000.0f;

    float linearDepth = nearZ * farZ / (farZ - depth * (farZ - nearZ));

    float vis = saturate((linearDepth - 0.1f) / 500.0f);

    return float4(vis, vis, vis, 1.0f);
}