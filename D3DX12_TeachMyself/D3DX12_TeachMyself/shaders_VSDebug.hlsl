struct VSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};


VSOut main(uint id : SV_VertexID)
{
    VSOut output;
    
    float2 uv = float2((id << 1) & 2, id & 2);
    output.pos = float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
    output.uv = float2(uv.x, 1.0f - uv.y);
    
    return output;
}