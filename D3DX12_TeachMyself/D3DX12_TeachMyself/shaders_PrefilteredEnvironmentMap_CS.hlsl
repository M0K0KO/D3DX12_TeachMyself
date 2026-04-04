TextureCube<float4> EnvMap : register(t0);
RWTexture2DArray<float4> FilteredMap : register(u0);
SamplerState LinearSampler : register(s0);

cbuffer FilterConstants : register(b0)
{
    float Roughness;
};

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

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

// GGX importance sampling
float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 N)
{
    float a = roughness * roughness; // roughness©÷ = ¥á
    
    float phi = 2.0 * 3.14159265 * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    
    // Spherical ¡æ Cartesian (tangent space)
    float3 H = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    
    // TBN: tangent space ¡æ world space
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);
    
    return normalize(T * H.x + B * H.y + N * H.z);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float roughness = max(Roughness, 0.001);
    
    float2 dimensions;
    float elements;
    FilteredMap.GetDimensions(dimensions.x, dimensions.y, elements);
    
    uint face = id.z;
    if (id.x >= (uint) dimensions.x || id.y >= (uint) dimensions.y)
        return;
    
    float2 uv = (id.xy + 0.5) / dimensions;
    float3 N = GetCubemapDirection(face, uv);
    float3 R = N;
    float3 V = N; 
    
    float3 prefilteredColor = float3(0, 0, 0);
    float totalWeight = 0.0;
    
    const uint SAMPLE_COUNT = 1024;
    
    for (uint i = 0; i < SAMPLE_COUNT; i++)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, roughness, N);
        float3 L = 2.0 * dot(V, H) * H - V;
        
        float NdotL = saturate(dot(N, L));
        
        if (NdotL > 0.0)
        {
            prefilteredColor += EnvMap.SampleLevel(LinearSampler, L, 0).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    
    FilteredMap[id] = float4(prefilteredColor / max(totalWeight, 0.0001), 1.0);
}