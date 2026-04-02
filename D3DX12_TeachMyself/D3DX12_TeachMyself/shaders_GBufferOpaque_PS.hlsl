cbuffer MaterialConstants : register(b2)
{
    float alphaCutoff;
    float metallicFactor;
    float roughnessFactor;
    float _pad;
};

Texture2D baseColorTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D metallicRoughnessTex : register(t2);
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
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 mr     : SV_TARGET2;
};

PSOutput main(PSInput input)
{
    PSOutput output;
    
    output.albedo = baseColorTex.Sample(samp, input.uv);

    
    float3 N = normalize(input.worldNormal);
    float3 T = normalize(input.worldTangent.xyz);
    float3 B = cross(N, T) * input.worldTangent.w;
    
    float2 nxy = normalTex.Sample(samp, input.uv).rg * 2.0f - 1.0f;
    float nz = sqrt(saturate(1.0f - dot(nxy, nxy)));
    float3 tangentNormal = float3(nxy, nz);
    float3 worldNormal = normalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z);
    output.normal = float4(worldNormal * 0.5f + 0.5f, 1.0f);
    
    float4 mrSample = metallicRoughnessTex.Sample(samp, input.uv);
    output.mr = float4(0,
        mrSample.g * roughnessFactor,
        mrSample.b * metallicFactor,
        0
    );
    
    return output;
}