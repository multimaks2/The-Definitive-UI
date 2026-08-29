sampler MaskTex : register(s0);
float4 FogColor : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float mask = tex2D(MaskTex, uv).r;
    float4 color = FogColor;
    color.a *= mask;
    return color;
}
