/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Shader/ShaderCode.cpp
 *  PURPOSE:     Embedded HLSL source for the radar and fog effects
 *
 *****************************************************************************/

#include "ShaderCode.h"

const char* GetCircleShaderCode()
{
    return R"(
texture sTexture;
sampler TextureSampler = sampler_state { Texture = <sTexture>; MinFilter = Linear; MagFilter = Linear; MipFilter = Linear; AddressU = Clamp; AddressV = Clamp; };
struct VSInput { float3 Position : POSITION0; float4 Color : COLOR0; float2 TexCoord : TEXCOORD0; };
struct PSInput { float4 Position : POSITION0; float4 Color : COLOR0; float2 TexCoord : TEXCOORD0; };
float4x4 WorldViewProj : WORLDVIEWPROJ;
PSInput VertexShaderFunction(VSInput VS) { PSInput PS = (PSInput)0; float4 pos = float4(VS.Position, 1.0); PS.Position = mul(pos, WorldViewProj); PS.Color = VS.Color; PS.TexCoord = VS.TexCoord; return PS; }
float4 PixelShaderFunction(PSInput PS) : COLOR0 { float4 texColor = tex2D(TextureSampler, PS.TexCoord); float4 result = texColor * PS.Color; return saturate(result); }
float4 PixelShaderFunctionCircle(PSInput PS) : COLOR0 { float2 uv = PS.TexCoord - 0.5; float dist = length(uv); float edge = 0.4925; float smoothness = 0.001; float t = saturate((dist - (edge - smoothness)) / (smoothness * 2.0)); float alpha = 1.0 - (t * t * (3.0 - 2.0 * t)); float4 texColor = tex2D(TextureSampler, PS.TexCoord); float4 result = texColor * PS.Color; result.a *= alpha; return saturate(result); }
technique Circle { pass P0 { VertexShader = compile vs_2_0 VertexShaderFunction(); PixelShader = compile ps_2_0 PixelShaderFunctionCircle(); ZEnable = true; ZWriteEnable = true; ZFunc = LessEqual; AlphaBlendEnable = true; SrcBlend = SrcAlpha; DestBlend = InvSrcAlpha; CullMode = None; Lighting = false; FogEnable = false; } }
technique SimpleTexture { pass P0 { VertexShader = compile vs_2_0 VertexShaderFunction(); PixelShader = compile ps_2_0 PixelShaderFunction(); ZEnable = false; ZWriteEnable = false; AlphaBlendEnable = true; SrcBlend = SrcAlpha; DestBlend = InvSrcAlpha; CullMode = None; Lighting = false; FogEnable = false; } }
)";
}

const char* GetBorderShaderCode()
{
    return R"(
texture sTexture;
sampler TextureSampler = sampler_state { Texture = <sTexture>; MinFilter = Linear; MagFilter = Linear; MipFilter = Linear; AddressU = Clamp; AddressV = Clamp; };
struct VSInput { float3 Position : POSITION0; float4 Color : COLOR0; float2 TexCoord : TEXCOORD0; };
struct PSInput { float4 Position : POSITION0; float4 Color : COLOR0; float2 TexCoord : TEXCOORD0; };
float4x4 WorldViewProj : WORLDVIEWPROJ;
float2 BorderSize : BORDERSIZE;
float BorderThicknessPixels : BORDERTHICKNESSPIXELS;
PSInput VertexShaderFunction(VSInput VS) { PSInput PS = (PSInput)0; float4 pos = float4(VS.Position, 1.0); PS.Position = mul(pos, WorldViewProj); PS.Color = VS.Color; PS.TexCoord = VS.TexCoord; return PS; }
float4 PixelShaderFunction(PSInput PS) : COLOR0 {
    float2 uv = PS.TexCoord - 0.5; float dist = length(uv);
    float outerRadius = 0.5;
    float minSize = min(BorderSize.x, BorderSize.y);
    float innerRadius = (minSize > 0.001) ? (0.5 - 2.0 * BorderThicknessPixels / minSize) : 0.455;
    if (innerRadius < 0.0) innerRadius = 0.0;
    float smoothness = 0.002;
    float t1 = saturate((dist - (innerRadius - smoothness)) / (smoothness * 2.0)); float smooth1 = t1 * t1 * (3.0 - 2.0 * t1);
    float t2 = saturate((dist - (outerRadius - smoothness)) / (smoothness * 2.0)); float smooth2 = t2 * t2 * (3.0 - 2.0 * t2);
    float border = smooth1 * (1.0 - smooth2); float4 borderColor = float4(1.0, 1.0, 1.0, 1.0);
    float4 result = borderColor * PS.Color; result.a *= border; return saturate(result);
}
float4 PixelShaderFunctionSquareBorder(PSInput PS) : COLOR0 {
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
    float4 result = borderColor * PS.Color; result.a *= alpha; return saturate(result);
}
technique Border { pass P0 { VertexShader = compile vs_2_0 VertexShaderFunction(); PixelShader = compile ps_2_0 PixelShaderFunction(); ZEnable = true; ZWriteEnable = true; ZFunc = LessEqual; AlphaBlendEnable = true; SrcBlend = SrcAlpha; DestBlend = InvSrcAlpha; CullMode = None; Lighting = false; FogEnable = false; } }
technique SquareBorder { pass P0 { VertexShader = compile vs_2_0 VertexShaderFunction(); PixelShader = compile ps_2_0 PixelShaderFunctionSquareBorder(); ZEnable = true; ZWriteEnable = true; ZFunc = LessEqual; AlphaBlendEnable = true; SrcBlend = SrcAlpha; DestBlend = InvSrcAlpha; CullMode = None; Lighting = false; FogEnable = false; } }
)";
}

const char* GetImage3DShaderCode()
{
    return R"(
texture sTexColor;
sampler SamplerColor = sampler_state { Texture = <sTexColor>; MinFilter = Anisotropic; MagFilter = Linear; MipFilter = Linear; MaxAnisotropy = 4; AddressU = Clamp; AddressV = Clamp; };
struct VSInput { float3 Position : POSITION0; float2 TexCoord : TEXCOORD0; float4 Diffuse : COLOR0; };
struct PSInput { float4 Position : POSITION0; float2 TexCoord : TEXCOORD0; float4 Diffuse : COLOR0; };
float3 sElementPosition : ELEMENTPOSITION; float3 sElementRotation : ELEMENTROTATION; float2 sElementSize : ELEMENTSIZE; float2 sScrRes : SCRRES;
float3 sCameraInputPosition : CAMERAPOSITION; float3 sCameraInputRotation : CAMERAROTATION; float sFov : FOV; float2 sClip : CLIP; float sProjectionAspect : PROJECTIONASPECT;
float4x4 WorldViewProj : WORLDVIEWPROJ;
static const float PI = 3.14159265f;
float4x4 createWorldMatrix(float3 pos, float3 rot) {
    float4x4 eleMatrix = {
        float4( cos(rot.z) * cos(rot.y) - sin(rot.z) * sin(rot.x) * sin(rot.y), cos(rot.y) * sin(rot.z) + cos(rot.z) * sin(rot.x) * sin(rot.y), -cos(rot.x) * sin(rot.y), 0),
        float4( -cos(rot.x) * sin(rot.z), cos(rot.z) * cos(rot.x), sin(rot.x), 0),
        float4( cos(rot.z) * sin(rot.y) + cos(rot.y) * sin(rot.z) * sin(rot.x), sin(rot.z) * sin(rot.y) - cos(rot.z) * cos(rot.y) * sin(rot.x), cos(rot.x) * cos(rot.y), 0),
        float4( pos.x, pos.y, pos.z, 1), }; return eleMatrix;
}
float4x4 createViewMatrix(float3 pos, float3 fwVec, float3 upVec) {
    float3 zaxis = normalize(fwVec); float3 xaxis = normalize(cross(-upVec, zaxis)); float3 yaxis = cross(xaxis, zaxis);
    float4x4 viewMatrix = { float4(xaxis.x, yaxis.x, zaxis.x, 0), float4(xaxis.y, yaxis.y, zaxis.y, 0), float4(xaxis.z, yaxis.z, zaxis.z, 0), float4(-dot(xaxis, pos), -dot(yaxis, pos), -dot(zaxis, pos), 1) };
    return viewMatrix;
}
float4x4 createProjectionMatrix(float nearPlane, float farPlane, float fovHoriz, float fovAspect) {
    float w = 1.0 / tan(fovHoriz * 0.5); float h = w / fovAspect; float Q = farPlane / (farPlane - nearPlane);
    float4x4 projectionMatrix = { float4(w, 0, 0, 0), float4(0, h, 0, 0), float4(0, 0, Q, 1), float4(0, 0, -Q * nearPlane, 0) };
    return projectionMatrix;
}
PSInput VertexShaderFunction(VSInput VS) {
    PSInput PS = (PSInput)0;
    VS.Position.xy /= float2(sScrRes.x, sScrRes.y); VS.Position.xy = -0.5 + VS.Position.xy; VS.Position.xy *= sElementSize.xy; VS.TexCoord.y = 1.0 - VS.TexCoord.y;
    float3 camRot = float3(sCameraInputRotation.x, 0.0, sCameraInputRotation.z); float4x4 sCamInv = createWorldMatrix(sCameraInputPosition, camRot);
    float rotOff = 600.0 * acos(dot(float3(0, 0, -1), sCamInv[1].xyz)) / (0.5 * PI);
    float3 offX = float3(sCamInv[0][0] + sCamInv[1][0] - rotOff * sCamInv[2][0], sCamInv[0][1] + sCamInv[1][1] - rotOff * sCamInv[2][1], sCamInv[0][2] + sCamInv[1][2] - rotOff * sCamInv[2][2]);
    float ex = cos(sElementRotation.x); float ey = cos(sElementRotation.y); float ez = cos(sElementRotation.z);
    float fx = sin(sElementRotation.x); float fy = sin(sElementRotation.y); float fz = sin(sElementRotation.z);
    float4x4 sWorld; sWorld[0] = float4(ez * ey - fz * fx * fy, ey * fz + ez * fx * fy, -ex * fy, 0); sWorld[1] = float4(-ex * fz, ez * ex, fx, 0);
    sWorld[2] = float4(ez * fy + ey * fz * fx, fz * fy - ez * ey * fx, ex * ey, 0); sWorld[3] = float4(sElementPosition.x, sElementPosition.y, sElementPosition.z, 1);
    float3 viewPos = sCamInv[3].xyz + offX; float3 zaxis = normalize(sCamInv[1].xyz); float3 xaxis = normalize(cross(-sCamInv[2].xyz, zaxis)); float3 yaxis = cross(xaxis, zaxis);
    float4x4 sView; sView[0] = float4(xaxis.x, yaxis.x, zaxis.x, 0); sView[1] = float4(xaxis.y, yaxis.y, zaxis.y, 0); sView[2] = float4(xaxis.z, yaxis.z, zaxis.z, 0); sView[3] = float4(-dot(xaxis, viewPos), -dot(yaxis, viewPos), -dot(zaxis, viewPos), 1);
    float aspect = (sProjectionAspect > 0.0) ? sProjectionAspect : (sScrRes.y / sScrRes.x); float w = 1.0 / tan(sFov * 0.5); float h = w / aspect; float Q = sClip[1] / (sClip[1] - sClip[0]);
    float4x4 sProjection; sProjection[0] = float4(w, 0, 0, 0); sProjection[1] = float4(0, h, 0, 0); sProjection[2] = float4(0, 0, Q, 1); sProjection[3] = float4(0, 0, -Q * sClip[0], 0);
    float4 wPos = mul(float4(VS.Position, 1.0), sWorld); float4 vPos = mul(wPos, sView); PS.Position = mul(vPos, sProjection);
    PS.TexCoord = VS.TexCoord; PS.Diffuse = VS.Diffuse; return PS;
}
float4 PixelShaderFunction(PSInput PS) : COLOR0 {
    float4 finalColor = tex2D(SamplerColor, PS.TexCoord.xy); finalColor *= PS.Diffuse;
    finalColor.rgb *= finalColor.a;
    return saturate(finalColor);
}
technique Image3D { pass P0 { VertexShader = compile vs_2_0 VertexShaderFunction(); PixelShader = compile ps_2_0 PixelShaderFunction(); ZEnable = false; ZWriteEnable = false; AlphaBlendEnable = true; SrcBlend = One; DestBlend = InvSrcAlpha; CullMode = None; Lighting = false; FogEnable = false; } }
)";
}

const char* GetLineShaderCode()
{
    return R"(
struct VSInput { float3 Position : POSITION0; float4 Color : COLOR0; };
struct PSInput { float4 Position : POSITION0; float4 Color : COLOR0; };
float4x4 WorldViewProj : WORLDVIEWPROJ;
PSInput VertexShaderFunction(VSInput VS) { PSInput PS = (PSInput)0; float4 pos = float4(VS.Position, 1.0); PS.Position = mul(pos, WorldViewProj); PS.Color = VS.Color; return PS; }
float4 PixelShaderFunction(PSInput PS) : COLOR0 { return saturate(PS.Color); }
technique Line { pass P0 { VertexShader = compile vs_2_0 VertexShaderFunction(); PixelShader = compile ps_2_0 PixelShaderFunction(); ZEnable = false; ZWriteEnable = false; AlphaBlendEnable = true; SrcBlend = SrcAlpha; DestBlend = InvSrcAlpha; CullMode = None; Lighting = false; FogEnable = false; } }
)";
}

const char* GetLineSmoothShaderCode()
{
    return R"(
struct VSInput { float3 Position : POSITION0; float4 Color : COLOR0; float2 TexCoord : TEXCOORD0; };
struct PSInput { float4 Position : POSITION0; float4 Color : COLOR0; float2 TexCoord : TEXCOORD0; };
float4x4 WorldViewProj : WORLDVIEWPROJ;
PSInput VertexShaderFunction(VSInput VS) {
    PSInput PS = (PSInput)0;
    PS.Position = mul(float4(VS.Position, 1.0), WorldViewProj);
    PS.Color = VS.Color;
    PS.TexCoord = VS.TexCoord;
    return PS;
}
float4 PixelShaderFunction(PSInput PS) : COLOR0 {
    float s = abs(PS.TexCoord.y);
    float edge = 0.5;
    float alpha = 1.0 - smoothstep(edge, 1.0, s);
    float4 col = PS.Color;
    col.a *= alpha;
    return saturate(col);
}
technique LineSmooth { pass P0 { VertexShader = compile vs_2_0 VertexShaderFunction(); PixelShader = compile ps_2_0 PixelShaderFunction(); ZEnable = false; ZWriteEnable = false; AlphaBlendEnable = true; SrcBlend = SrcAlpha; DestBlend = InvSrcAlpha; CullMode = None; Lighting = false; FogEnable = false; } }
)";
}

const char* GetGreenSquareShaderCode()
{
    return R"(
texture sTexture;
sampler TextureSampler = sampler_state { Texture = <sTexture>; MinFilter = Linear; MagFilter = Linear; MipFilter = Linear; AddressU = Clamp; AddressV = Clamp; };
struct VSInput { float3 Position : POSITION0; float4 Color : COLOR0; float2 TexCoord : TEXCOORD0; };
struct PSInput { float4 Position : POSITION0; float4 Color : COLOR0; float2 TexCoord : TEXCOORD0; };
float4x4 WorldViewProj : WORLDVIEWPROJ; float FillLevel : FILLLEVEL;
PSInput VertexShaderFunction(VSInput VS) { PSInput PS = (PSInput)0; float4 pos = float4(VS.Position, 1.0); PS.Position = mul(pos, WorldViewProj); PS.Color = VS.Color; PS.TexCoord = VS.TexCoord; return PS; }
float4 PixelShaderFunction(PSInput PS) : COLOR0 {
    float2 uv = PS.TexCoord - 0.5; float dist = length(uv); float edge = 0.4925; float smoothness = 0.001;
    float t = saturate((dist - (edge - smoothness)) / (smoothness * 2.0)); float circleAlpha = 1.0 - (t * t * (3.0 - 2.0 * t));
    float yCoord = PS.TexCoord.y; float fillThreshold = 1.0 - FillLevel; float yAlpha = 1.0;
    if (yCoord < fillThreshold) yAlpha = 0.0;
    else { float ySmoothness = 0.01; if (yCoord < fillThreshold + ySmoothness) { float yT = saturate((yCoord - fillThreshold) / ySmoothness); yAlpha = yT * yT * (3.0 - 2.0 * yT); } else yAlpha = 1.0; }
    float4 texColor = tex2D(TextureSampler, PS.TexCoord); float4 result = texColor * PS.Color; result.a *= circleAlpha * yAlpha; return saturate(result);
}
technique GreenSquareFill { pass P0 { VertexShader = compile vs_2_0 VertexShaderFunction(); PixelShader = compile ps_2_0 PixelShaderFunction(); ZEnable = true; ZWriteEnable = true; ZFunc = LessEqual; AlphaBlendEnable = true; SrcBlend = SrcAlpha; DestBlend = InvSrcAlpha; CullMode = None; Lighting = false; FogEnable = false; } }
)";
}
