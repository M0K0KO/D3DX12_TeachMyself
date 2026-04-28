#include "Vertex.hlsli"

cbuffer PerFrame : register(b0)
{
    float4x4 ViewProj;
    float4x4 InvViewProj;
    float3 CameraPos;
    float pad0;
    
    float2 ScreenSize;
    float2 pad1;
};

struct GPUTransformData
{
    float4x4 world;
    float4x4 worldInvTranspose;
};

struct DrawConstants
{
    uint objIdx;
    uint matIdx;
    uint vbIdx;
};
ConstantBuffer<DrawConstants> g_DrawConstants : register(b2);

StructuredBuffer<GPUTransformData> g_Transforms : register(t1);

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

VSOutput main(uint vid : SV_VertexID)
{
    VSOutput output;
    
    StructuredBuffer<Vertex> vb = ResourceDescriptorHeap[g_DrawConstants.vbIdx];
    Vertex v = vb[vid];
    
    GPUTransformData t = g_Transforms[g_DrawConstants.objIdx];
    
    float4 worldPos = mul(float4(v.position, 1.0f), t.world);
    
    output.position = mul(worldPos, ViewProj);
    output.uv = v.uv;
    output.worldNormal = SafeNormalize(mul(v.normal, (float3x3)t.worldInvTranspose), float3(0.0f, 1.0f, 0.0f));
    output.worldTangent = float4(
        normalize(mul(v.tangent.xyz, (float3x3) t.world)),
        v.tangent.w
        );
    
    return output;
}