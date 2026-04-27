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
    output.position = mul(worldPos, LightViewProj);
    output.uv = input.uv;
    
    return output;
};