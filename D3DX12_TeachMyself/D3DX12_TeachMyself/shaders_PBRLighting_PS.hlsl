// =============================================================
// LightingPass_PS.hlsl
// Cook-Torrance PBR Lighting (Karis, Real Shading in UE4, 2013)
// Phase 5 - Directional Light Only (IBL added later)
// =============================================================

#define MAX_POINT_LIGHTS 8

// ----- GBuffer SRVs (DescriptorTable) -----
Texture2D gDepth : register(t0); // Depth buffer (SRV)
Texture2D gAlbedoAO : register(t1); // RGB=Albedo, A=AO
Texture2D gNormalMap : register(t2); // RG=WorldNormal XY (BC5)
Texture2D gMetallicRoughness : register(t3); // R=Metallic, G=Roughness

TextureCube<float4> gIrradianceMap : register(t4);
TextureCube<float4> gPrefilteredEnvMap : register(t5);
Texture2D<float2> gBRDF_LUT : register(t6);

Texture2D<float> gShadowMap : register(t7);

Texture2D gSSAOTex : register(t8);
Texture2D gGTAOTex : register(t9);

TextureCube<float> pointShadowMaps[MAX_POINT_LIGHTS] : register(t10);


SamplerState gSamplerPoint : register(s0); 
SamplerState gSamplerLinear : register(s1);
SamplerComparisonState gComparisonSampler : register(s2);

cbuffer PerFrameData : register(b0)
{
    float4x4 VP;
    
    float4x4 inverseVP;
    
    float3 cameraPos;
    float pad0;
    
    float2 screenSize;
    float2 pad1;
};

struct PointLight
{
    float3 Position;
    float Radius;
    float3 Color;
    float intensity;
};

cbuffer LightData : register(b1)
{
    float3 lightDirection;
    float pad2;
    
    float3 lightColor;
    float lightIntensity;
    
    float3 ambient;
    int PointLightCount;
    
    PointLight PointLights[MAX_POINT_LIGHTS];
};

cbuffer ShadowData : register(b2)
{
    float4x4 LightViewProj[4];
    float4 cascadeSplits;
};

// ----- Constants -----
static const float PI = 3.14159265359;
static const float blendRegion = 0.1f;

// =============================================================
// World Position Reconstruction: depth + invViewProj
// =============================================================
float3 ReconstructWorldPos(float2 uv, float depth)
{
    // UV -> NDC: x[-1,1] y[-1,1] z[0,1]
    float4 ndc = float4(uv * 2.0 - 1.0, depth, 1.0);
    ndc.y = -ndc.y; // DX UV is top-to-bottom

    float4 worldPos = mul(inverseVP, ndc);
    return worldPos.xyz / worldPos.w;
}

// =============================================================
// BC5 Normal Reconstruction: Z = sqrt(1 - x*x - y*y)
// =============================================================
float3 DecodeNormal(float2 rg)
{
    // BC5 stored in [0,1] -> restore to [-1,1]
    float2 xy = rg * 2.0 - 1.0;
    float z = sqrt(max(1.0 - dot(xy, xy), 0.0));
    return normalize(float3(xy, z));
}

// =============================================================
// D: GGX / Trowbridge-Reitz NDF
// Ratio of microfacets aligned with H
// Karis 2013 Eq.3: alpha = roughness * roughness
// =============================================================
float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// =============================================================
// G: Smith-Schlick GGX (Geometry / Shadowing-Masking)
// Amount of microfacet self-occlusion
// Karis 2013 Eq.4: k = (roughness + 1)^2 / 8
// Used only for direct lighting
// =============================================================
float GeometrySchlickGGX(float NdotV, float k)
{
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float NdotL, float NdotV, float roughness)
{
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return GeometrySchlickGGX(NdotL, k) * GeometrySchlickGGX(NdotV, k);
}

// =============================================================
// F: Schlick Fresnel
// Reflectance change depending on view angle
// Metal: F0 = albedo, Dielectric: F0 ~= 0.04
// =============================================================
float3 FresnelSchlick(float VdotH, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

float3 FresnelSchlickRoughness(float NdotV, float3 F0, float roughness)
{
    return F0 + (max(1.0 - roughness, F0) - F0) * pow(1.0 - NdotV, 5.0);
}

float3 CookTorranceBRDF(
    float3 N,
    float3 V,
    float3 L,
    float3 albedo,
    float metallic,
    float roughness)
{
    float3 H = normalize(V + L);

    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotL, NdotV, roughness);
    float3 F = FresnelSchlick(VdotH, F0);

    float3 specular = (D * G * F) / max(4.0 * NdotL * NdotV, 0.0001);

    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);
    float3 diffuse = kD * albedo / PI;

    return diffuse + specular;
}

float SampleShadowPCF(int cascadeIndex, float3 worldPos)
{
    float4 lightSpacePos = mul(float4(worldPos, 1.0f), LightViewProj[cascadeIndex]);
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    float2 shadowUV = projCoords.xy * 0.5f + 0.5f;
    shadowUV.y = 1.0f - shadowUV.y;
    
    float2 tileOffsets[4] = { float2(0, 0), float2(0.5, 0), float2(0, 0.5), float2(0.5, 0.5) };
    shadowUV = shadowUV * 0.5f + tileOffsets[cascadeIndex];
    
    float texelSize = 1.0f / 4096.0f;
    float shadow = 0.0f;
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            shadow += gShadowMap.SampleCmpLevelZero(gComparisonSampler, shadowUV + float2(x, y) * texelSize, projCoords.z - 0.005f);
        }
    }
    return shadow / 9.0f;
}

float SamplePointShadow(int idx, float3 lightToSurface, float farZ)
{
    float dominant = max(abs(lightToSurface.x),
                     max(abs(lightToSurface.y), abs(lightToSurface.z)));
    
    float nearZ = 0.1f;
    float projDepth = (farZ * dominant - farZ * nearZ)
                    / (dominant * (farZ - nearZ));
    float bias = 0.005f;
    
    const float3 sampleOffsets[20] =
    {
        float3(1, 1, 1), float3(1, -1, 1), float3(-1, -1, 1), float3(-1, 1, 1),
        float3(1, 1, -1), float3(1, -1, -1), float3(-1, -1, -1), float3(-1, 1, -1),
        float3(1, 1, 0), float3(1, -1, 0), float3(-1, -1, 0), float3(-1, 1, 0),
        float3(1, 0, 1), float3(-1, 0, 1), float3(1, 0, -1), float3(-1, 0, -1),
        float3(0, 1, 1), float3(0, -1, 1), float3(0, -1, -1), float3(0, 1, -1)
    };
    
    float dist = length(lightToSurface);
    float diskRadius = (1.0 + dist / farZ) * 0.005; 
    
    float shadow = 0;
    [unroll]
    for (int s = 0; s < 20; ++s)
    {
        float3 sampleDir = lightToSurface + sampleOffsets[s] * diskRadius;
        shadow += pointShadowMaps[idx].SampleCmpLevelZero(
            gComparisonSampler, sampleDir, projDepth - bias);
    }
    return shadow / 20.0;
}


// =============================================================
// Fullscreen Triangle Vertex -> PS Input
// =============================================================
struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// =============================================================
// Pixel Shader Main
// =============================================================
float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.uv;

    // --- Read GBuffer ---
    float4 albedoAO = gAlbedoAO.Sample(gSamplerPoint, uv);
    float3 albedo = albedoAO.rgb;
    //float ssao = gSSAOTex.Sample(gSamplerLinear, uv).r;
    float gtao = gGTAOTex.Sample(gSamplerLinear, uv).r;
    float ao = albedoAO.a * gtao;
    //float ao = albedoAO.a;

    float3 normalRGB = gNormalMap.Sample(gSamplerPoint, uv).rgb;
    float3 N = normalize(normalRGB * 2.0f - 1.0f);
    
    float4 mr = gMetallicRoughness.Sample(gSamplerPoint, uv);
    float metallic = mr.b;
    float roughness = mr.g;

    float depth = gDepth.Sample(gSamplerPoint, uv).r;

    float3 worldPos = ReconstructWorldPos(uv, depth);
    
    
    
    
    // Directional Shadow
    float viewDepth = length(worldPos - cameraPos);
    
    int cascadeIndex = 0;
    if (viewDepth > cascadeSplits.x)
        cascadeIndex = 1;
    if (viewDepth > cascadeSplits.y)
        cascadeIndex = 2;
    if (viewDepth > cascadeSplits.z)
        cascadeIndex = 3;
    
    float shadow = SampleShadowPCF(cascadeIndex, worldPos);
    
    float cascadeFar = cascadeSplits[cascadeIndex];
    float fadeStart = cascadeFar * (1.0f - blendRegion);
    float blendFactor = saturate((viewDepth - fadeStart) / (cascadeFar - fadeStart));
    
    if (blendFactor > 0.0f && cascadeIndex < 3)
    {
        float shadowNext = SampleShadowPCF(cascadeIndex + 1, worldPos);
        shadow = lerp(shadow, shadowNext, blendFactor);
    }
    // Directional Shadow
    
    float3 totalLighting = 0.0f;
    
    float3 V = normalize(cameraPos - worldPos); // surface to eye
    float NdotV = saturate(dot(N, V));
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    
    {
        float3 L = normalize(-lightDirection);
        float NdotL = saturate(dot(N, L));

        float3 brdf = CookTorranceBRDF(N, V, L, albedo, metallic, roughness);
        float3 radiance = lightColor * lightIntensity;

        totalLighting += shadow * brdf * radiance * NdotL;
    }
    

    for (int i = 0; i < MAX_POINT_LIGHTS; i++)
    {
        if (i < PointLightCount)
        {
            float3 L = PointLights[i].Position - worldPos;
            float dist = length(L);

            if (dist <= PointLights[i].Radius)
            {
                L /= dist;
        
                float3 lightToSurface = worldPos - PointLights[i].Position;
                float shadowSample = SamplePointShadow(i, lightToSurface, 1000.0f);
                float dominant = max(abs(lightToSurface.x), max(abs(lightToSurface.y), abs(lightToSurface.z)));
                float nearZ = 0.1f;
                float farZ = PointLights[i].Radius;
                float projDepth = (farZ * dominant - farZ * nearZ) / (dominant * (farZ - nearZ));
                float pointShadow = (projDepth > shadowSample + 0.005f) ? 0.0f : 1.0f;

                float atten = saturate(1.0 - (dist * dist) / (PointLights[i].Radius * PointLights[i].Radius));
                atten *= atten;

                float3 radiance = PointLights[i].Color * PointLights[i].intensity * atten;

                float NdotL = saturate(dot(N, L));
                float3 brdf = CookTorranceBRDF(N, V, L, albedo, metallic, roughness);

                pointShadow = lerp(1.0, pointShadow, atten);
                totalLighting += pointShadow * brdf * radiance * NdotL;
            }
        }
    }

    // --- IBL: Diffuse ---
    float3 F_ibl = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD_ibl = (1.0 - F_ibl) * (1.0 - metallic);
    float3 irradiance = gIrradianceMap.SampleLevel(gSamplerLinear, N, 0).rgb;
    float3 diffuseIBL = kD_ibl * irradiance * albedo;

    // --- IBL: Specular ---
    float3 R = reflect(-V, N);
    static const float MAX_MIP = 4.0; // mipLevels - 1
    float3 prefilteredColor = gPrefilteredEnvMap.SampleLevel(gSamplerLinear, R, roughness * MAX_MIP).rgb;
    float2 envBRDF = gBRDF_LUT.Sample(gSamplerLinear, float2(NdotV, roughness)).rg;
    float3 specularIBL = prefilteredColor * (F0 * envBRDF.x + envBRDF.y);

    // --- Final ---
    float3 ambient = (diffuseIBL + specularIBL) * ao;
    float3 color = ambient + totalLighting;

    return float4(color, 1.0);
    
    //return float4(1.0 - shadow, 1.0 - shadow, 1.0 - shadow, 1.0f);
    
    //float ssao = gSSAOTex.Sample(gSamplerLinear, uv);
    //return float4(ssao, ssao, ssao, 1);
    
    //return float4(gtao, gtao, gtao, 1);
}