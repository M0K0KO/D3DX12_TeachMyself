Texture2D<float4> g_sceneColor : register(t0);
SamplerState g_linearClamp : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

float3 ACESFilm(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 LinearToSRGB(float3 linearColor)
{
    return pow(saturate(linearColor), 1.0 / 2.2);
}

float4 main(VSOutput input) : SV_TARGET
{
    float3 hdr = g_sceneColor.Sample(g_linearClamp, input.uv).rgb;

    float3 ldr = ACESFilm(hdr);
    float3 outColor = LinearToSRGB(ldr);

    return float4(outColor, 1.0);
}