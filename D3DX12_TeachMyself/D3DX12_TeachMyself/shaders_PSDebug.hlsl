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
    
    float near = 0.1f;
    float far = 1000.0f;
    float value = near * far / (far - depth) * (far - near);
    
    float normalized = value / far;
    
    return float4(normalized, normalized, normalized, 1.0f);
}