cbuffer PerFrame : register(b0)
{
    float4x4 ViewProj;
    float4x4 InvViewProj;
    float3 CameraPos;
    float pad0;
    
    float2 ScreenSize;
    float2 pad1;
};

cbuffer PerObject : register(b1)
{
    float4x4 World;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    
    float3 worldNormal : TEXCOORD1;
    float4 worldTangent : TEXCOORD2;
};

float3 SafeNormalize(float3 v, float3 fallback)
{
    float lenSq = dot(v, v);
    return (lenSq > 1e-8f) ? (v * rsqrt(lenSq)) : fallback;
}

VSOutput main(VSInput input)
{
    VSOutput output;
    
    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.position = mul(worldPos, ViewProj);
    output.uv = input.uv;
    
    output.worldNormal = SafeNormalize(mul(input.normal, (float3x3) World), float3(0.0f, 1.0f, 0.0f));

    output.worldTangent = float4(
    mul(input.tangent.xyz, (float3x3) World),
    input.tangent.w
);
    
    return output;
}