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
};
ConstantBuffer<DrawConstants> g_DrawConstants : register(b1);

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
    
    float4 worldPos = mul(float4(input.position, 1.0f), g_Transforms[g_DrawConstants.transformIdx].World);
    output.position = mul(worldPos, pointShadowData[g_DrawConstants.lightIdx].faceVP[g_DrawConstants.faceIdx]);
    output.uv = input.uv;
    
    return output;
};