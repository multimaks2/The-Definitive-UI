struct VSInput
{
    float3 Position : POSITION0;
    float4 Color : COLOR0;
};

struct PSInput
{
    float4 Position : POSITION0;
    float4 Color : COLOR0;
};

float4x4 WorldViewProj : WORLDVIEWPROJ;

PSInput VertexShaderFunction(VSInput VS)
{
    PSInput PS = (PSInput)0;
    float4 pos = float4(VS.Position, 1.0);
    PS.Position = mul(pos, WorldViewProj);
    PS.Color = VS.Color;
    return PS;
}

float4 PixelShaderFunction(PSInput PS) : COLOR0
{
    return saturate(PS.Color);
}

technique Line
{
    pass P0
    {
        VertexShader = compile vs_2_0 VertexShaderFunction();
        PixelShader = compile ps_2_0 PixelShaderFunction();
        ZEnable = false;
        ZWriteEnable = false;
        AlphaBlendEnable = true;
        SrcBlend = SrcAlpha;
        DestBlend = InvSrcAlpha;
        CullMode = None;
        Lighting = false;
        FogEnable = false;
    }
}
