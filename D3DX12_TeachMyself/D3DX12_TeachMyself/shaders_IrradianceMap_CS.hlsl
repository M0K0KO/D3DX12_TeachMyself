TextureCube<float4> EnvMap : register(t0);
RWTexture2DArray<float4> IrradianceMap : register(u0);
SamplerState LinearSampler : register(s0);

float3 GetCubemapDirection(uint face, float2 uv)
{
    // uv: [0,1] ¡æ [-1,1]
    float2 st = uv * 2.0 - 1.0;
    
    switch (face)
    {
        case 0:
            return normalize(float3(1, -st.y, -st.x)); // +X
        case 1:
            return normalize(float3(-1, -st.y, st.x)); // -X
        case 2:
            return normalize(float3(st.x, 1, st.y)); // +Y
        case 3:
            return normalize(float3(st.x, -1, -st.y)); // -Y
        case 4:
            return normalize(float3(st.x, -st.y, 1)); // +Z
        case 5:
            return normalize(float3(-st.x, -st.y, -1)); // -Z
        default:
            return float3(0, 0, 1);
    }
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float2 dimensions;
    float elements;
    IrradianceMap.GetDimensions(dimensions.x, dimensions.y, elements);
    
    uint face = id.z;
    float2 uv = (id.xy + 0.5) / dimensions;
    float3 N = GetCubemapDirection(face, uv);
    
    float3 up = abs(N.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);
    
    float3 irradiance = float3(0, 0, 0);
    
    const float PI = 3.14159265;
    const float sampleDelta = 0.025; 
    float sampleCount = 0.0;
    
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            float3 tangentSample = float3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );
            float3 sampleDir = tangentSample.x * T + tangentSample.y * B + tangentSample.z * N;
            
            irradiance += EnvMap.SampleLevel(LinearSampler, sampleDir, 0).rgb 
                        * cos(theta) * sin(theta);
            sampleCount++;
        }
    }
    
    irradiance = PI * irradiance / sampleCount;
    
    IrradianceMap[id] = float4(irradiance, 1.0);
}