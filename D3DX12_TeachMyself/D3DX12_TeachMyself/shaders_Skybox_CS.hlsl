cbuffer Constants : register(b0)
{
    uint cubemapSize;
};

float3 GetCubeDirection(uint face, float2 uv)
{
    switch (face)
    {
        case 0:
            return float3(+1.0, -uv.y, -uv.x); // +X
        case 1:
            return float3(-1.0, -uv.y, +uv.x); // -X
        case 2:
            return float3(+uv.x, +1.0, +uv.y); // +Y
        case 3:
            return float3(+uv.x, -1.0, -uv.y); // -Y
        case 4:
            return float3(+uv.x, -uv.y, +1.0); // +Z
        case 5:
            return float3(-uv.x, -uv.y, -1.0); // -Z
        default:
            return float3(0, 0, 0);
    }
}

RWTexture2DArray<float4> CubemapOut : register(u0);
Texture2D<float4> EquirectIn : register(t0);
SamplerState LinearSampler : register(s0);

static const float PI = 3.14159265359;

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint face = dtid.z;
    float2 uv = (dtid.xy + 0.5) / cubemapSize; // [0,1]
    uv = uv * 2.0 - 1.0; // [-1,1]

    float3 dir = GetCubeDirection(face, uv);
    dir = normalize(dir);

    float2 equirectUV;
    equirectUV.x = atan2(dir.z, dir.x) / (2.0 * PI) + 0.5;
    equirectUV.y = asin(clamp(dir.y, -1.0, 1.0)) / PI + 0.5;

    float4 color = EquirectIn.SampleLevel(LinearSampler, equirectUV, 0);
    CubemapOut[dtid] = color;
}