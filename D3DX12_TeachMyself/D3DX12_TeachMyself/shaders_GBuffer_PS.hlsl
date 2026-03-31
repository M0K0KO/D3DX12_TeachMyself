Texture2D baseColorTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D metallicRoughnessTex : register(t2);
SamplerState samp : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct PSOutput
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 mr     : SV_TARGET2;
};

PSOutput main(PSInput input)
{
    PSOutput output;
    
    output.albedo = baseColorTex.Sample(samp, input.uv);
    
    output.normal = normalTex.Sample(samp, input.uv);
    
    float4 mrSample = metallicRoughnessTex.Sample(samp, input.uv);
    output.mr = float4(0.0f, mrSample.g, mrSample.b, 0.0f);
    
    return output;
}