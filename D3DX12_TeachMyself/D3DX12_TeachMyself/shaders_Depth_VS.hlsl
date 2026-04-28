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
    float4x4 World;
    float4x4 WorldInvTranspose;
};

StructuredBuffer<GPUTransformData> g_Transforms : register(t0);

struct DrawConstants
{
    uint transformIdx;
    uint vertexBufferIdx;
};
ConstantBuffer<DrawConstants> g_DrawConstants : register(b1);


float4 main(uint vid : SV_VertexID) : SV_Position
{
    StructuredBuffer<Vertex> vb = ResourceDescriptorHeap[g_DrawConstants.vertexBufferIdx];
    Vertex v = vb[vid];
    
    GPUTransformData t = g_Transforms[g_DrawConstants.transformIdx];
    
    float4 worldPos = mul(float4(v.position, 1.0f), t.World);
    return mul(worldPos, ViewProj);
}