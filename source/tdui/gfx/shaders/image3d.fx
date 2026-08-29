texture sTexColor;
sampler SamplerColor = sampler_state
{
    Texture = <sTexColor>;
    MinFilter = Anisotropic;
    MagFilter = Linear;
    MipFilter = Linear;
    MaxAnisotropy = 4;
    AddressU = Clamp;
    AddressV = Clamp;
};

struct VSInput
{
    float3 Position : POSITION0;
    float2 TexCoord : TEXCOORD0;
    float4 Diffuse : COLOR0;
};

struct PSInput
{
    float4 Position : POSITION0;
    float2 TexCoord : TEXCOORD0;
    float4 Diffuse : COLOR0;
};

float3 sElementPosition : ELEMENTPOSITION;
float3 sElementRotation : ELEMENTROTATION;
float2 sElementSize : ELEMENTSIZE;
float2 sScrRes : SCRRES;
float3 sCameraInputPosition : CAMERAPOSITION;
float3 sCameraInputRotation : CAMERAROTATION;
float sFov : FOV;
float2 sClip : CLIP;
float sProjectionAspect : PROJECTIONASPECT;
float sFlatProjection : FLATPROJECTION;
float4x4 WorldViewProj : WORLDVIEWPROJ;

static const float PI = 3.14159265f;

float4x4 createWorldMatrix(float3 pos, float3 rot)
{
    float4x4 eleMatrix =
    {
        float4(cos(rot.z) * cos(rot.y) - sin(rot.z) * sin(rot.x) * sin(rot.y),
               cos(rot.y) * sin(rot.z) + cos(rot.z) * sin(rot.x) * sin(rot.y),
               -cos(rot.x) * sin(rot.y), 0),
        float4(-cos(rot.x) * sin(rot.z), cos(rot.z) * cos(rot.x), sin(rot.x), 0),
        float4(cos(rot.z) * sin(rot.y) + cos(rot.y) * sin(rot.z) * sin(rot.x),
               sin(rot.z) * sin(rot.y) - cos(rot.z) * cos(rot.y) * sin(rot.x),
               cos(rot.x) * cos(rot.y), 0),
        float4(pos.x, pos.y, pos.z, 1)
    };
    return eleMatrix;
}

PSInput VertexShaderFunction(VSInput VS)
{
    PSInput PS = (PSInput)0;
    VS.Position.xy /= float2(sScrRes.x, sScrRes.y);
    VS.Position.xy = -0.5 + VS.Position.xy;
    VS.Position.xy *= sElementSize.xy;
    VS.TexCoord.y = 1.0 - VS.TexCoord.y;

    float3 camRot = float3(sCameraInputRotation.x, 0.0, sCameraInputRotation.z);
    float4x4 sCamInv = createWorldMatrix(sCameraInputPosition, camRot);

    float3 viewPos;
    float3 zaxis;
    float3 xaxis;
    float3 yaxis;
    if (sFlatProjection > 0.5)
    {
        float yaw = sCameraInputRotation.z;
        float sy = sin(yaw);
        float cy = cos(yaw);
        viewPos = sCamInv[3].xyz;
        zaxis = float3(0.0, 0.0, -1.0);
        float3 up = float3(-sy, cy, 0.0);
        xaxis = normalize(cross(-up, zaxis));
        yaxis = cross(xaxis, zaxis);
    }
    else
    {
        float rotOff = 600.0 * acos(dot(float3(0, 0, -1), sCamInv[1].xyz)) / (0.5 * PI);
        float3 offX = float3(
            sCamInv[0][0] + sCamInv[1][0] - rotOff * sCamInv[2][0],
            sCamInv[0][1] + sCamInv[1][1] - rotOff * sCamInv[2][1],
            sCamInv[0][2] + sCamInv[1][2] - rotOff * sCamInv[2][2]);
        viewPos = sCamInv[3].xyz + offX;
        zaxis = normalize(sCamInv[1].xyz);
        xaxis = normalize(cross(-sCamInv[2].xyz, zaxis));
        yaxis = cross(xaxis, zaxis);
    }

    float ex = cos(sElementRotation.x);
    float ey = cos(sElementRotation.y);
    float ez = cos(sElementRotation.z);
    float fx = sin(sElementRotation.x);
    float fy = sin(sElementRotation.y);
    float fz = sin(sElementRotation.z);

    float4x4 sWorld;
    sWorld[0] = float4(ez * ey - fz * fx * fy, ey * fz + ez * fx * fy, -ex * fy, 0);
    sWorld[1] = float4(-ex * fz, ez * ex, fx, 0);
    sWorld[2] = float4(ez * fy + ey * fz * fx, fz * fy - ez * ey * fx, ex * ey, 0);
    sWorld[3] = float4(sElementPosition.x, sElementPosition.y, sElementPosition.z, 1);

    float4x4 sView;
    sView[0] = float4(xaxis.x, yaxis.x, zaxis.x, 0);
    sView[1] = float4(xaxis.y, yaxis.y, zaxis.y, 0);
    sView[2] = float4(xaxis.z, yaxis.z, zaxis.z, 0);
    sView[3] = float4(-dot(xaxis, viewPos), -dot(yaxis, viewPos), -dot(zaxis, viewPos), 1);

    float aspect = (sFlatProjection > 0.5) ? 1.0
        : ((sProjectionAspect > 0.0) ? sProjectionAspect : (sScrRes.y / sScrRes.x));
    float w = 1.0 / tan(sFov * 0.5);
    float h = w / aspect;
    float Q = sClip[1] / (sClip[1] - sClip[0]);

    float4x4 sProjection;
    sProjection[0] = float4(w, 0, 0, 0);
    sProjection[1] = float4(0, h, 0, 0);
    sProjection[2] = float4(0, 0, Q, 1);
    sProjection[3] = float4(0, 0, -Q * sClip[0], 0);

    float4 wPos = mul(float4(VS.Position, 1.0), sWorld);
    float4 vPos = mul(wPos, sView);
    PS.Position = mul(vPos, sProjection);
    PS.TexCoord = VS.TexCoord;
    PS.Diffuse = VS.Diffuse;
    return PS;
}

float4 PixelShaderFunction(PSInput PS) : COLOR0
{
    float4 finalColor = tex2D(SamplerColor, PS.TexCoord.xy);
    finalColor *= PS.Diffuse;
    finalColor.rgb *= finalColor.a;
    return saturate(finalColor);
}

technique Image3D
{
    pass P0
    {
        VertexShader = compile vs_2_0 VertexShaderFunction();
        PixelShader = compile ps_2_0 PixelShaderFunction();
        ZEnable = false;
        ZWriteEnable = false;
        AlphaBlendEnable = true;
        SrcBlend = One;
        DestBlend = InvSrcAlpha;
        CullMode = None;
        Lighting = false;
        FogEnable = false;
    }
}
