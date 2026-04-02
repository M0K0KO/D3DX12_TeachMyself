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
    float farZ = 300.0f;

    float linearDepth = nearZ * farZ / (farZ - depth * (farZ - nearZ));

    float vis = saturate(linearDepth / 35.0f);
    vis = pow(vis, 1.2f);

    return float4(vis, 0.0f, 0.0f, 1.0f);
}