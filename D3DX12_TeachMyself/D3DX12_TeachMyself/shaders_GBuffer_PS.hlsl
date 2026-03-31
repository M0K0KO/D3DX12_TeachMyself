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
    float3 T = normalize(input.worldTangent).xyz;
    float3 B = cross(N, T) * input.worldTangent.w;
    
    float3 tangentNormal = normalTex.Sample(samp, input.uv).rgb * 2.0f - 1.0f;
    float3 worldNormal = normalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z);
    output.normal = float4(worldNormal * 0.5f + 0.5f, 1.0f);
    
    float4 mrSample = metallicRoughnessTex.Sample(samp, input.uv);
    output.mr = float4(0.0f, mrSample.g, mrSample.b, 0.0f);
    
    return output;
}