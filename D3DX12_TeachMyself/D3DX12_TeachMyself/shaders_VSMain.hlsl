cbuffer PerFrame : register(b0)
{
    float4x4 ViewProj;
    float3 CameraPos;
};

cbuffer PerObject : register(b1)
{
    float4x4 World;
};

struct VSOut
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

VSOut main(float3 position : POSITION, float3 normal : NORMAL, float4 tangent : TANGENT, float2 uv : TEXCOORD)
{
    VSOut output;
    
    float3 worldPos = mul(float4(position, 1.0f), World).xyz;
    output.position = mul(float4(worldPos, 1.0f), ViewProj);
    output.uv = uv;
    //output.uv = float2(output.position.z, output.position.w);
    
    return output;
}