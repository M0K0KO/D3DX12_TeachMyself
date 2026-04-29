Texture2D<float> AOInput : register(t0);
Texture2D<float> DepthTex : register(t1);
Texture2D<float4> NormalTex : register(t2);

RWTexture2D<float> AOOutput : register(u0);

cbuffer BlurCB : register(b0)
{
    float2 InvRes;
    int2 Direction; // (1,0) or (0,1)
    float DepthSigma; // 0.1 정도
    float NormalSigma; // 32 정도 (exponent)
    int Radius; // 4~8
};

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    float2 uv = (DTid.xy + 0.5) * InvRes;
    float centerAO = AOInput[DTid.xy];
    int2 centerFullRes = int2(DTid.xy) * 2;
    float centerDepth = DepthTex.Load(int3(centerFullRes, 0));
    float3 centerN = NormalTex.Load(int3(centerFullRes, 0)).xyz * 2 - 1;
    
    float sumAO = 0;
    float sumW = 0;
    
    for (int i = -Radius; i <= Radius; ++i)
    {
        int2 p = DTid.xy + Direction * i;
        
        float sAO = AOInput[p];
        int2 pFullRes = p * 2;
        float sDepth = DepthTex.Load(int3(pFullRes, 0));
        float3 sN = NormalTex.Load(int3(pFullRes, 0)).xyz * 2 - 1;
        
        // Gaussian (공간 거리)
        float wSpatial = exp(-0.5 * (i * i) / (Radius * Radius * 0.25));
        
        // Depth weight
        float depthDiff = abs(centerDepth - sDepth);
        float wDepth = exp(-depthDiff * depthDiff / (DepthSigma * DepthSigma));
        
        // Normal weight
        float wNormal = pow(max(dot(centerN, sN), 0), NormalSigma);
        
        float w = wSpatial * wDepth * wNormal;
        sumAO += sAO * w;
        sumW += w;
    }
    
    AOOutput[DTid.xy] = sumAO / max(sumW, 1e-5);
}