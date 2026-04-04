cbuffer PerFrameData : register(b0)
{
    float4x4 VP;
    
    float4x4 inverseVP;
    
    float3 cameraPos;
    float pad0;
    
    float2 screenSize;
    float2 pad1;
};

cbuffer LightData : register(b1)
{
    float3 lightDirection;
    float pad2;
    
    float3 lightColor;
    float lightIntensity;
    
    float3 ambient;
    float pad4;
};

Texture2D depthTex : register(t0);
Texture2D albedoTex : register(t1);
Texture2D normalTex : register(t2);
Texture2D mrTex : register(t3);

SamplerState pointSampler : register(s0);

float3 ReconstructWorldPos(float2 uv, float depth)
{
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;
    float4 clip = float4(ndc, depth, 1.0);
    float4 world = mul(clip, inverseVP);
    return world.xyz / world.w;
}

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

float4 main(VSOutput input) : SV_Target
{
    float depth = depthTex.Sample(pointSampler, input.uv).r;
    float3 albedo = albedoTex.Sample(pointSampler, input.uv).rgb;
    float3 normal = normalTex.Sample(pointSampler, input.uv).rgb;
   
    normal = normalize(normal * 2.0 - 1.0);

    float3 worldPos = ReconstructWorldPos(input.uv, depth);
    float3 N = normal;
    float3 V = normalize(cameraPos - worldPos);
    float3 L = normalize(-lightDirection);
    float3 H = normalize(V + L);

    // Lambert Diffuse
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse = albedo * NdotL;

    // Blinn-Phong Specular
    float shininess = 64.0;
    float NdotH = max(dot(N, H), 0.0);
    float3 specular = pow(NdotH, shininess);

    float3 Lo = (diffuse + specular) * lightColor * lightIntensity;
    Lo += ambient * albedo;

    return float4(Lo, 1.0);
}