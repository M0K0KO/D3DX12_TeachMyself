struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput main(uint id : SV_VertexID)
{
    VSOutput output;
    
    float2 uv = float2(id & 2, (id << 1) & 2);
    output.position = float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
    output.uv = float2(uv.x, 1.0f - uv.y);
    
    return output;
}