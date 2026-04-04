RWTexture2D<float2> LUT : register(u0);

static const float3 N = float3(0, 0, 1);

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
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    // Spherical ¡æ Cartesian (tangent space)
    float3 H = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    
    return H;
}

// Smith G (Schlick-GGX, IBL¿ë k = ¥á©÷/2)
float G_SchlickGGX_IBL(float NdotX, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0; // IBL¿ë! direct light´Â (a+1)©÷/8
    return NdotX / (NdotX * (1.0 - k) + k);
}

float G_Smith(float roughness, float NdotV, float NdotL)
{
    return G_SchlickGGX_IBL(NdotV, roughness) * G_SchlickGGX_IBL(NdotL, roughness);
}

[numthreads(16, 16, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float2 dimensions;
    LUT.GetDimensions(dimensions.x, dimensions.y);
    
    float NdotV = max((id.x + 0.5) / dimensions.x, 0.001);
    float roughness = max((id.y + 0.5) / dimensions.y, 0.001);
    
    float3 V = float3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    
    float A = 0.0;
    float B = 0.0;
    
    const uint SAMPLE_COUNT = 1024;
    
    for (uint i = 0; i < SAMPLE_COUNT; i++)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, roughness, N);
        float3 L = 2.0 * dot(V, H) * H - V;
        
        float NdotL = saturate(L.z);
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));
        
        if (NdotL > 0.0)
        {
            float G = G_Smith(roughness, NdotV, NdotL);
            float G_Vis = G * VdotH / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);
            
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    LUT[id.xy] = float2(A, B) / float(SAMPLE_COUNT);
}