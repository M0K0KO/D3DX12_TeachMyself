Texture2D<float> gDepth : register(t0);
Texture2D<float4> gNormal : register(t1);
Texture2D<float4> gNoise : register(t2);

SamplerState pointS : register(s0);
SamplerState linearS : register(s1);

cbuffer PerFrame : register(b0)
{
    float4x4 ViewProj;
    float4x4 InvViewProj;
    float3 CameraPos;
    float2 ScreenSize;
};

cbuffer SSAO : register(b1)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 InvProj;
    float Radius;
    float Bias;
    float Power;
    int KernelSize;

    float2 NoiseScale;
    float2 _pad;

    float4 Samples[32];
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float3 GetPositionVS(float2 uv, float depth, float4x4 invProj)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = (1.0f - uv.y) * 2.0f - 1.0f;

    float4 positionNDC = float4(x, y, depth, 1.0f);
    float4 positionVS = mul(positionNDC, invProj);

    return positionVS.xyz / positionVS.w;
}

float4 main(PSIn input) : SV_TARGET
{
    float depth = gDepth.SampleLevel(pointS, input.uv, 0);
    if (depth >= 0.999f)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    float3 viewPos = GetPositionVS(input.uv, depth, InvProj);

    float3 normal = gNormal.Sample(linearS, input.uv).xyz;
    normal = normalize(normal * 2.0f - 1.0f); 

    float3 normalVS = normalize(mul(normal, (float3x3)View));
    
    float2 noiseUV = input.uv * NoiseScale;
    
    
    float3 randomVec = gNoise.SampleLevel(pointS, noiseUV, 0).xyz;
    randomVec = randomVec * 2.0f - 1.0f;
    
    float3 tangent = normalize(randomVec - normalVS * dot(randomVec, normalVS));
    float3 bitangent = cross(normalVS, tangent);

    float occlusion = 0.0f;
    for (int i = 0; i < KernelSize; ++i)
    {
        float3 sampleOffset = (tangent * Samples[i].x) + (bitangent * Samples[i].y) + (normalVS * Samples[i].z);
        float3 samplePosVS = viewPos + (sampleOffset * Radius);
        
        float4 offsetCS = mul(float4(samplePosVS, 1.0f), Proj);
        offsetCS.xy /= offsetCS.w;

        float2 sampleUV = offsetCS.xy * 0.5f + 0.5f;
        sampleUV.y = 1.0f - sampleUV.y;
        
        float sampleDepth = gDepth.SampleLevel(pointS, sampleUV, 0);
        float3 actualPosVS = GetPositionVS(sampleUV, sampleDepth, InvProj);

        float depthDiff = abs(viewPos.z - actualPosVS.z);
        float rangeCheck = smoothstep(0.0f, 1.0f, 1.0f - (depthDiff / Radius));

        occlusion += (samplePosVS.z >= actualPosVS.z + Bias ? 1.0f : 0.0f) * rangeCheck;
    }

    occlusion = 1.0f - (occlusion / (float) KernelSize);
    
    
    float ao = pow(abs(occlusion), Power);
    
    float2 fade = max(1.0f - input.uv, input.uv); 
    float edgeFade = max(fade.x, fade.y);
    edgeFade = smoothstep(0.95f, 1.0f, edgeFade); 

    ao = lerp(ao, 1.0f, edgeFade);

    return float4(ao, ao, ao, 1.0f);
    
    //return float4(normalVS * 0.5f + 0.5f, 1.0f);
    //return float4(frac(viewPos.z * 0.1f).xxx, 1.0f);

}