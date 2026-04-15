cbuffer BlurCB : register(b0)
{
    float2 TexelSize; // 1 / screenSize
    float DepthSigma; // ~50~100
    float NormalSigma; // ~8~16
};

float3 DecodeNormal(float4 n)
{
    return normalize(n.xyz * 2.0 - 1.0);
}

float Weight(float cd, float sd, float3 cn, float3 sn)
{
    float d = abs(cd - sd);
    float nd = 1.0 - saturate(dot(cn, sn));

    float wDepth = exp(-d * DepthSigma);
    float wNormal = exp(-nd * NormalSigma);

    return wDepth * wNormal;
}

Texture2D gSSAO : register(t0);
Texture2D gDepth : register(t1);
Texture2D gNormal : register(t2);

SamplerState linearS : register(s0);

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VSOut i) : SV_TARGET
{
    float2 uv = i.uv;

    float centerAO = gSSAO.Sample(linearS, uv).r;
    float centerDepth = gDepth.Sample(linearS, uv).r;
    float3 centerNormal = DecodeNormal(gNormal.Sample(linearS, uv));

    float sum = 0;
    float wsum = 0;

    [unroll]
    for (int y = -2; y <= 2; y++)
    {
        float2 offset = float2(0, y) * TexelSize;
        float2 suv = uv + offset;

        float ao = gSSAO.Sample(linearS, suv).r;
        float d = gDepth.Sample(linearS, suv).r;
        float3 n = DecodeNormal(gNormal.Sample(linearS, suv));

        float w = Weight(centerDepth, d, centerNormal, n);

        sum += ao * w;
        wsum += w;
    }

    return sum / max(wsum, 1e-4);
}