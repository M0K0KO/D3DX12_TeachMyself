cbuffer CubeConstantBuffer : register(b0)
{
    float4x4 worldViewProj;
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

PSInput main(float3 position : POSITION, float2 uv : TEXCOORD)
{
    PSInput result;
    
    result.position = mul(float4(position, 1.0), worldViewProj);
    result.uv = uv;
    
    return result;
}