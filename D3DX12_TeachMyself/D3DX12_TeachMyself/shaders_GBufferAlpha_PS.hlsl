cbuffer MaterialConstants : register(b2)
{
    float4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float3 emissiveFactor;
    float occlusionStrength;
    
    float alphaCutoff;
    uint alphaMode;
};

Texture2D baseColorTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D metallicRoughnessTex : register(t2);
Texture2D emissiveTex : register(t3);
Texture2D occlusionTex : register(t4);

SamplerState samp : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    
    float3 worldNormal : TEXCOORD1;
    float4 worldTangent : TEXCOORD2;
};

struct PSOutput
{
    float4 albedo   : SV_TARGET0;
    float4 normal   : SV_TARGET1;
    float4 mr       : SV_TARGET2;
    float3 emissive : SV_TARGET3;
};

float3 SafeNormalize(float3 v, float3 fallback)
{
    float lenSq = dot(v, v);
    return (lenSq > 1e-8f) ? (v * rsqrt(lenSq)) : fallback;
}

PSOutput main(PSInput input)
{
    PSOutput output;
    
    output.albedo = baseColorTex.Sample(samp, input.uv);
    clip(output.albedo.a < alphaCutoff ? -1 : 1);
    
    float aoSample = occlusionTex.Sample(samp, input.uv);
    output.albedo.a = lerp(1.0, aoSample, occlusionStrength);
    
    float3 N = SafeNormalize(input.worldNormal, float3(0.0f, 1.0f, 0.0f));

    float3 T = input.worldTangent.xyz;
    if (dot(T, T) <= 1e-8f)
    {
        float3 up = (abs(N.y) < 0.999f) ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
        T = normalize(cross(up, N));
    }
    else
    {
        T = normalize(T - N * dot(T, N));
    }

    float handedness = (abs(input.worldTangent.w) > 0.5f) ? input.worldTangent.w : 1.0f;
    float3 B = SafeNormalize(cross(N, T), float3(0.0f, 0.0f, 1.0f)) * handedness;
    
    float2 nxy = normalTex.Sample(samp, input.uv).rg * 2.0f - 1.0f;
    float nz = sqrt(saturate(1.0f - dot(nxy, nxy)));
    float3 tangentNormal = float3(nxy, nz);
    float3 worldNormal = SafeNormalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z, N);
    output.normal = float4(worldNormal * 0.5f + 0.5f, 1.0f);
    
    float4 mrSample = metallicRoughnessTex.Sample(samp, input.uv);
    output.mr =
        float4
        (
            0,
            mrSample.g * roughnessFactor,
            mrSample.b * metallicFactor,
            0
        );
    
    float3 emissiveSample = emissiveTex.Sample(samp, input.uv);
    output.emissive = emissiveSample * emissiveFactor;
    
    return output;
}