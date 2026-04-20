Texture2D<float4> g_sceneColor : register(t0);
SamplerState g_linearClamp : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(VSOutput input) : SV_TARGET
{
    return g_sceneColor.Sample(g_linearClamp, input.uv);
}