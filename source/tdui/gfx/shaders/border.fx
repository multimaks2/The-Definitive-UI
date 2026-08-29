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
float2 BorderSize : BORDERSIZE;
float BorderThicknessPixels : BORDERTHICKNESSPIXELS;

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
    float outerRadius = 0.5;
    float minSize = min(BorderSize.x, BorderSize.y);
    float innerRadius = (minSize > 0.001) ? (0.5 - 2.0 * BorderThicknessPixels / minSize) : 0.455;
    if (innerRadius < 0.0) innerRadius = 0.0;
    float smoothness = 0.002;
    float t1 = saturate((dist - (innerRadius - smoothness)) / (smoothness * 2.0));
    float smooth1 = t1 * t1 * (3.0 - 2.0 * t1);
    float t2 = saturate((dist - (outerRadius - smoothness)) / (smoothness * 2.0));
    float smooth2 = t2 * t2 * (3.0 - 2.0 * t2);
    float border = smooth1 * (1.0 - smooth2);
    float4 borderColor = float4(1.0, 1.0, 1.0, 1.0);
    float4 result = borderColor * PS.Color;
    result.a *= border;
    return saturate(result);
}

float4 PixelShaderFunctionSquareBorder(PSInput PS) : COLOR0
{
    float2 uv = PS.TexCoord;
    float distToLeft = uv.x * BorderSize.x;
    float distToRight = (1.0 - uv.x) * BorderSize.x;
    float distToTop = uv.y * BorderSize.y;
    float distToBottom = (1.0 - uv.y) * BorderSize.y;
    float distToEdgePixels = min(min(distToLeft, distToRight), min(distToTop, distToBottom));
    float smoothness = 1.0;
    float alpha = 1.0 - saturate((distToEdgePixels - (BorderThicknessPixels - smoothness)) / (smoothness * 2.0));
    alpha = alpha * alpha * (3.0 - 2.0 * alpha);
    float4 borderColor = float4(1.0, 1.0, 1.0, 1.0);
    float4 result = borderColor * PS.Color;
    result.a *= alpha;
    return saturate(result);
}

technique Border
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

technique SquareBorder
{
    pass P0
    {
        VertexShader = compile vs_2_0 VertexShaderFunction();
        PixelShader = compile ps_2_0 PixelShaderFunctionSquareBorder();
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
