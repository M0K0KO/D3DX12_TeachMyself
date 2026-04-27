struct GPUMaterialData
{
    uint baseColorIdx;
    uint normalIdx;
    uint mrIdx;
    uint emissiveIdx;
    uint occlusionIdx;
    uint3 _pad0;
    
    float4 baseColorFactor;
    float3 emissiveFactor;
    float occlusionStrength;
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    uint alphaMode;
};

struct DrawConstants
{
    uint objIdx;
    uint matIdx;
};
ConstantBuffer<DrawConstants> g_DrawConstants : register(b2);

StructuredBuffer<GPUMaterialData> g_Materials : register(t0);

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
    GPUMaterialData mat = g_Materials[g_DrawConstants.matIdx];
    
    Texture2D baseColorTex = ResourceDescriptorHeap[mat.baseColorIdx];
    Texture2D normalTex    = ResourceDescriptorHeap[mat.normalIdx];
    Texture2D mrTex        = ResourceDescriptorHeap[mat.mrIdx];
    Texture2D emissiveTex  = ResourceDescriptorHeap[mat.emissiveIdx];
    Texture2D occlusionTex = ResourceDescriptorHeap[mat.occlusionIdx];
    
    PSOutput output;
    
    output.albedo = baseColorTex.Sample(samp, input.uv) * mat.baseColorFactor;
    float aoSample = occlusionTex.Sample(samp, input.uv);
    output.albedo.a = lerp(1.0, aoSample, mat.occlusionStrength);
    
    float3 N = SafeNormalize(input.worldNormal, float3(0.0f, 1.0f, 0.0f));
    float3 T = input.worldTangent.xyz;
    if (dot(T, T) <= 1e-8f)
    {
        float3 up = (abs(N.y) < 0.999f) ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
        T = normalize(cross(up, N));
    }
    else baseColorTex.Sample(samp, input.uv);
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
    
    float4 mrSample = mrTex.Sample(samp, input.uv);
    output.mr =
        float4
        (
            0,
            mrSample.g * mat.roughnessFactor,
            mrSample.b * mat.metallicFactor,
            0
        );
    
    float3 emissiveSample = emissiveTex.Sample(samp, input.uv);
    output.emissive = emissiveSample * mat.emissiveFactor;
    
    return output;
}