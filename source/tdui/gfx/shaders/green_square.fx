texture sTexture;
sampler TextureSampler = sampler_state
{
    Texture = <sTexture>;
    MinFilter = Linear;
    MagFilter = Linear;
    MipFilter = Linear;
    AddressU = Clamp;
    AddressV = Clamp;
};

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
float FillLevel : FILLLEVEL;

PSInput VertexShaderFunction(VSInput VS)
{
    PSInput PS = (PSInput)0;
    float4 pos = float4(VS.Position, 1.0);
    PS.Position = mul(pos, WorldViewProj);
    PS.Color = VS.Color;
    PS.TexCoord = VS.TexCoord;
    return PS;
}

float4 PixelShaderFunction(PSInput PS) : COLOR0
{
    float2 uv = PS.TexCoord - 0.5;
    float dist = length(uv);
    float edge = 0.4925;
    float smoothness = 0.001;
    float t = saturate((dist - (edge - smoothness)) / (smoothness * 2.0));
    float circleAlpha = 1.0 - (t * t * (3.0 - 2.0 * t));
    float yCoord = PS.TexCoord.y;
    float fillThreshold = 1.0 - FillLevel;
    float yAlpha = 1.0;
    if (yCoord < fillThreshold)
        yAlpha = 0.0;
    else
    {
        float ySmoothness = 0.01;
        if (yCoord < fillThreshold + ySmoothness)
        {
            float yT = saturate((yCoord - fillThreshold) / ySmoothness);
            yAlpha = yT * yT * (3.0 - 2.0 * yT);
        }
        else
            yAlpha = 1.0;
    }
    float4 texColor = tex2D(TextureSampler, PS.TexCoord);
    float4 result = texColor * PS.Color;
    result.a *= circleAlpha * yAlpha;
    return saturate(result);
}

technique GreenSquareFill
{
    pass P0
    {
        VertexShader = compile vs_2_0 VertexShaderFunction();
        PixelShader = compile ps_2_0 PixelShaderFunction();
        ZEnable = true;
        ZWriteEnable = true;
        ZFunc = LessEqual;
        AlphaBlendEnable = true;
        SrcBlend = SrcAlpha;
        DestBlend = InvSrcAlpha;
        CullMode = None;
        Lighting = false;
        FogEnable = false;
    }
}
