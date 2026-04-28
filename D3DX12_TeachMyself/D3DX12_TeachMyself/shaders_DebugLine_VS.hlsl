cbuffer PerFrame : register(b0)
{
    float4x4 ViewProj;
    float4x4 InvViewProj;
    float3 CameraPos;
    float pad0;
    
    float2 ScreenSize;
    float2 pad1;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    o.position = mul(float4(input.position, 1), ViewProj);
    o.color = input.color;
    return o;
}