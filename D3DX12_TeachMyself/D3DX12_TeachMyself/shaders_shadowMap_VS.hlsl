#include "Vertex.hlsli"

cbuffer ShadowMap : register(b0)
{
    float4x4 LightViewProj;
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

float4 main(uint vid : SV_VertexID) : SV_POSITION
{
    StructuredBuffer<Vertex> vb = ResourceDescriptorHeap[g_DrawConstants.vertexBufferIdx];
    Vertex v = vb[vid];
    
    GPUTransformData t = g_Transforms[g_DrawConstants.transformIdx];
    float4 worldPos = mul(float4(v.position, 1.0), t.World);
    
    return mul(worldPos, LightViewProj);
};