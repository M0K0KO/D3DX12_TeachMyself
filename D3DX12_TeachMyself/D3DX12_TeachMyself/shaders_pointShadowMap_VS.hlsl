#include "Vertex.hlsli"

#define MAX_POINT_LIGHTS 8

struct PointShadowData
{
    float4x4 faceVP[6];
    float3 lightPos;
    float lightRadius;
};

cbuffer PointShadowCB : register(b0)
{
    PointShadowData pointShadowData[MAX_POINT_LIGHTS];
};

struct GPUTransformData
{
    float4x4 World;
    float4x4 WorldInvTranspose;
};

StructuredBuffer<GPUTransformData> g_Transforms : register(t0);

struct DrawConstants
{
    uint lightIdx;
    uint faceIdx;
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
    return mul(worldPos, pointShadowData[g_DrawConstants.lightIdx].faceVP[g_DrawConstants.faceIdx]);
};