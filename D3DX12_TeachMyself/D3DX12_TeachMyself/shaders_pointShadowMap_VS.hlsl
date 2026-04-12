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

cbuffer PerObjectCB : register(b1)
{
    float4x4 World;
};

cbuffer RootConstants : register(b2)
{
    uint lightIdx;
    uint faceIdx;
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
    float2 uv : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.position = mul(worldPos, pointShadowData[lightIdx].faceVP[faceIdx]);
    output.uv = input.uv;
    
    return output;
};