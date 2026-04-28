Texture2D sceneColor : register(t0);
Texture2D gbufferAlbedo : register(t1);
Texture2D gbufferNormal : register(t2);
Texture2D gbufferMR : register(t3);
Texture2D gbufferEmissive : register(t4);
Texture2D gbufferDepth : register(t5);
Texture2D aoTex : register(t6);

cbuffer DebugConstants : register(b0)
{
    uint debugMode;
};

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
    switch (debugMode)
    {
        case 0:
            {
                float3 hdr = sceneColor.Sample(g_linearClamp, input.uv).rgb;
                float3 ldr = ACESFilm(hdr);
                return float4(ldr, 1.0);
            };
        case 1:
            {
                float3 color = gbufferAlbedo.Sample(g_linearClamp, input.uv).rgb;
                return float4(color, 1);
            }
        case 2:
            {
                float3 color = gbufferNormal.Sample(g_linearClamp, input.uv).rgb;
                return float4(color, 1);
            }
        case 3:
            {
                float3 color = gbufferMR.Sample(g_linearClamp, input.uv).rgb;
                return float4(color, 1);
            }
        case 4:
            {
                float3 color = gbufferEmissive.Sample(g_linearClamp, input.uv).rgb;
                return float4(color, 1);
            }
        case 5:
            {
                float depth = gbufferDepth.Sample(g_linearClamp, input.uv).r;

                float nearZ = 0.1f;
                float farZ = 300.0f;

                float linearDepth = nearZ * farZ / (farZ - depth * (farZ - nearZ));

                float vis = saturate(linearDepth / 25.0f);
                vis = pow(vis, 1.0f);

                return float4(vis, 0.0f, 0.0f, 1.0f);
            }
        case 6:
            {
                float3 color = aoTex.Sample(g_linearClamp, input.uv).rgb;
                return float4(color, 1);
            }
        default:
            {
                float3 hdr = sceneColor.Sample(g_linearClamp, input.uv).rgb;
                float3 ldr = ACESFilm(hdr);
                return float4(ldr, 1.0);
            };
    }

}