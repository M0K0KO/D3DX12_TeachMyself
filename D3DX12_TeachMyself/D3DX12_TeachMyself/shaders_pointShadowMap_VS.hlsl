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
    uint transformIdx;
    uint vertexBufferIndex;
};
struct FaceConstants
{
    uint lightIdx;
    uint faceIdx;
};
ConstantBuffer<DrawConstants> draw : register(b1);
ConstantBuffer<FaceConstants> face : register(b2);


float4 main(uint vid : SV_VertexID) : SV_Position
{
    StructuredBuffer<Vertex> vb = ResourceDescriptorHeap[draw.vertexBufferIndex];
    Vertex v = vb[vid];
    
    GPUTransformData t = g_Transforms[draw.transformIdx];
    
    float4 worldPos = mul(float4(v.position, 1.0f), t.World);
    return mul(worldPos, pointShadowData[face.lightIdx].faceVP[face.faceIdx]);
};