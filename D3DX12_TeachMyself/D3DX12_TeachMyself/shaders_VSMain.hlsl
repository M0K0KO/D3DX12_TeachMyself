cbuffer PerFrame : register(b0)
{
    float4x4 ViewProj;
    float3 CameraPos;
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
    float2 uv : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.position = mul(worldPos, ViewProj);
    output.uv = input.uv;
    
    return output;
}