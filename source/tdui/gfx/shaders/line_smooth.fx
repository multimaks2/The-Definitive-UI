struct VSInput
{
    float3 Position : POSITION0;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

struct PSInput
{
    float4 Position : POSITION0;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

float4x4 WorldViewProj : WORLDVIEWPROJ;

PSInput VertexShaderFunction(VSInput VS)
{
    PSInput PS = (PSInput)0;
    PS.Position = mul(float4(VS.Position, 1.0), WorldViewProj);
    PS.Color = VS.Color;
    PS.TexCoord = VS.TexCoord;
    return PS;
}

float4 PixelShaderFunction(PSInput PS) : COLOR0
{
    float s = abs(PS.TexCoord.y);
    float edge = 0.5;
    float alpha = 1.0 - smoothstep(edge, 1.0, s);
    float4 col = PS.Color;
    col.a *= alpha;
    return saturate(col);
}

technique LineSmooth
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
