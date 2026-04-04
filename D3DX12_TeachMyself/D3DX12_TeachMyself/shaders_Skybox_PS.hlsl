TextureCube<float4> SkyboxCubemap : register(t0);
SamplerState LinearSampler : register(s0);

cbuffer PerFrameData : register(b0)
{
    float4x4 VP;
    
    float4x4 inverseVP;
    
    float3 cameraPos;
    float pad0;
    
    float2 screenSize;
    float2 pad1;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    // NDC ¡æ clip space (depth = 1.0 = far plane)
    float4 clip = float4(input.uv * 2.0 - 1.0, 1.0, 1.0);
    clip.y = -clip.y; // DX UV convention

    float4 worldPos = mul(inverseVP, clip);
    float3 dir = normalize(worldPos.xyz / worldPos.w);

    return SkyboxCubemap.Sample(LinearSampler, dir);
}