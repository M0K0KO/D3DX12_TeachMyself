Texture2D<float> DepthTex : register(t0);
Texture2D<float4> NormalTex : register(t1);

RWTexture2D<float4> Output : register(u0);

SamplerState PointClamp : register(s0);

cbuffer GTAOCB : register(b0)
{
    float4x4 View;
    float4x4 InvProj;
    float2 InvRes;
    float Radius;
    float FalloffStart;
    float FalloffEnd;
    int NumSlices;
    int NumSteps;
    int FrameIndex;
};

static const float PI = 3.14159265;

float3 ReconstructViewPos(float2 uv, float depth, float4x4 invProj)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = (1.0f - uv.y) * 2.0f - 1.0f;

    float4 positionNDC = float4(x, y, depth, 1.0f);
    float4 positionVS = mul(positionNDC, invProj);

    return positionVS.xyz / positionVS.w;
}

float InterleavedGradientNoise(float2 pos)
{
    return frac(52.9829189 * frac(dot(pos, float2(0.06711056, 0.00583715))));
}

float SignedAngle(float3 a, float3 b, float3 axis)
{
    float c = dot(a, b);
    float s = dot(axis, cross(a, b));
    return atan2(s, c);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    float2 uv = (DTid.xy + 0.5) * InvRes;
    
    // 1) View-space position 복원
    float depth = DepthTex.Load(int3(DTid.xy, 0)).r;
    if (depth >= 1.0)
    {
        Output[DTid.xy] = 1.0;
        return;
    } // sky
    
    float3 unpackedNormal = NormalTex[DTid.xy].xyz * 2.0 - 1.0;
    
    float3 P = ReconstructViewPos(uv, depth, InvProj);
    float3 V = normalize(-P);
    float3 N = normalize(mul(unpackedNormal, (float3x3)View)); // world→view
    
    // 2) Noise (Interleaved Gradient Noise)
    float noise = InterleavedGradientNoise(DTid.xy);
    
    // 3) Screen-space radius (perspective 보정)
    float proj00 = 1.0 / InvProj._m00_m00;
    float pixelRadius = Radius * proj00 / P.z * 0.5 / InvRes.x;
    pixelRadius = clamp(pixelRadius, 2.0, 256.0); // clamp
    float stepSize = pixelRadius / NumSteps;
    
    // 4) Slice loop
    float visibility = 0.0;
    for (uint s = 0; s < NumSlices; ++s)
    {
        float phi = (s + noise) * (PI / NumSlices);
        float3 sliceDir = float3(cos(phi), sin(phi), 0);
        
        // Slice plane에 N 투영
        float3 sliceN = cross(sliceDir, V); // slice plane normal
        float3 projN = N - dot(N, sliceN) * sliceN;
        float projNLen = length(projN);
        
        float n = SignedAngle(projN / projNLen, V, sliceN);
        
        // Horizon search
        float cosH1 = -1.0; // +방향
        float cosH2 = -1.0; // -방향
        
        for (uint step = 1; step <= NumSteps; ++step)
        {
            float2 offset = sliceDir.xy * (step * stepSize) * InvRes;
            
            // + direction
            float dp = DepthTex.SampleLevel(PointClamp, uv + offset, 0).r;
            float3 Sp = ReconstructViewPos(uv + offset, dp, InvProj);
            float3 Dp = Sp - P;
            float lenP = length(Dp);
            float falloffP = saturate((FalloffEnd - lenP) / (FalloffEnd - FalloffStart));
            cosH1 = max(cosH1, dot(Dp / lenP, V) * falloffP);
            
            // - direction
            float dm = DepthTex.SampleLevel(PointClamp, uv - offset, 0).r;
            float3 Sm = ReconstructViewPos(uv - offset, dm, InvProj);
            float3 Dm = Sm - P;
            float lenM = length(Dm);
            float falloffM = saturate((FalloffEnd - lenM) / (FalloffEnd - FalloffStart));
            cosH2 = max(cosH2, dot(Dm / lenM, V) * falloffM);
        }
        
        // Clamp to normal hemisphere
        float h1 = -acos(cosH1);
        float h2 = acos(cosH2);
        h1 = n + max(h1 - n, -PI * 0.5);
        h2 = n + min(h2 - n, PI * 0.5);
        
        // Analytic integral
        float sliceVis = 0.25 * (
            (-cos(2 * h1 - n) + cos(n) + 2 * h1 * sin(n)) +
            (-cos(2 * h2 - n) + cos(n) + 2 * h2 * sin(n))
        );
        visibility += sliceVis;
    }
    
    visibility /= NumSlices;
    Output[DTid.xy] = saturate(visibility);
}