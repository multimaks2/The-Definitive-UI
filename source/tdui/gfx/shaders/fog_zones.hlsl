sampler Zones : register(s0);
float4 FogColor : register(c0);
float4 Params : register(c1);

float explor(float2 cell)
{
    float n = Params.x;
    if (cell.x < 0 || cell.y < 0 || cell.x >= n || cell.y >= n)
        return 0;
    return tex2D(Zones, (cell + 0.5) * Params.z).r;
}

float isRound(float2 vtx)
{
    float e00 = explor(vtx + float2(-1, -1));
    float e10 = explor(vtx + float2(0, -1));
    float e01 = explor(vtx + float2(-1, 0));
    float e11 = explor(vtx);
    float cnt = e00 + e10 + e01 + e11;
    return ((cnt > 0.75 && cnt < 1.25) || (cnt > 2.75 && cnt < 3.25)) ? 1.0 : 0.0;
}

float distSeg(float2 p, float2 a, float2 b)
{
    float2 ab = b - a;
    float den = dot(ab, ab);
    if (den < 1e-8)
        return length(p - a);
    float t = saturate(dot(p - a, ab) / den);
    return length(p - (a + ab * t));
}

float distQArc(float2 p, float2 c, float2 s, float R)
{
    float2 d = p - c;
    float2 q = d * s;
    if (q.x >= 0.0 && q.y >= 0.0)
        return abs(length(d) - R);
    return min(length(d - s * float2(R, 0)), length(d - s * float2(0, R)));
}

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float n = Params.x;
    float R = Params.y;
    float softW = Params.w;
    float2 p = uv * n;
    float2 cell = floor(p);
    float fogHard = 1.0 - explor(cell);
    float sd = 1000.0;

    for (int i = 0; i < 4; i++)
    {
        float2 vtx = cell + float2(i == 1 || i == 3, i == 2 || i == 3);
        float e00 = explor(vtx + float2(-1, -1));
        float e10 = explor(vtx + float2(0, -1));
        float e01 = explor(vtx + float2(-1, 0));
        float e11 = explor(vtx);
        float cnt = e00 + e10 + e01 + e11;
        float2 center;
        float2 tip0;
        float2 s;
        float active = 0;
        float convex = 0;

        if (cnt > 0.75 && cnt < 1.25)
        {
            active = 1;
            convex = 1;
            if (e00 > 0.5) { center = vtx + float2(-R, -R); tip0 = center; s = float2(1, 1); }
            else if (e10 > 0.5) { center = vtx + float2(R, -R); tip0 = vtx + float2(0, -R); s = float2(-1, 1); }
            else if (e01 > 0.5) { center = vtx + float2(-R, R); tip0 = vtx + float2(-R, 0); s = float2(1, -1); }
            else { center = vtx + float2(R, R); tip0 = vtx; s = float2(-1, -1); }
        }
        else if (cnt > 2.75 && cnt < 3.25)
        {
            active = 1;
            if (e00 < 0.5) { center = vtx + float2(-R, -R); tip0 = center; s = float2(1, 1); }
            else if (e10 < 0.5) { center = vtx + float2(R, -R); tip0 = vtx + float2(0, -R); s = float2(-1, 1); }
            else if (e01 < 0.5) { center = vtx + float2(-R, R); tip0 = vtx + float2(-R, 0); s = float2(1, -1); }
            else { center = vtx + float2(R, R); tip0 = vtx; s = float2(-1, -1); }
        }

        if (active < 0.5)
            continue;

        sd = min(sd, distQArc(p, center, s, R));
        float2 tip1 = tip0 + float2(R, R);
        if (p.x < tip0.x || p.y < tip0.y || p.x > tip1.x || p.y > tip1.y)
            continue;

        float dArc = length(p - center) - R;
        if (convex > 0.5)
            fogHard = (dArc > 0.0) ? 1.0 : 0.0;
        else
            fogHard = (dArc < 0.0) ? 1.0 : 0.0;
    }

    {
        float2 c0 = cell;
        float2 c1 = cell + float2(1, 1);
        float eC = explor(cell);
        float eL = explor(cell + float2(-1, 0));
        float eR = explor(cell + float2(1, 0));
        float eT = explor(cell + float2(0, -1));
        float eB = explor(cell + float2(0, 1));

        if ((eC > 0.5) != (eL > 0.5))
        {
            float y0 = c0.y + (isRound(c0) > 0.5 ? R : 0.0);
            float y1 = c1.y - (isRound(float2(c0.x, c1.y)) > 0.5 ? R : 0.0);
            if (y1 > y0)
                sd = min(sd, distSeg(p, float2(c0.x, y0), float2(c0.x, y1)));
        }
        if ((eC > 0.5) != (eR > 0.5))
        {
            float y0 = c0.y + (isRound(float2(c1.x, c0.y)) > 0.5 ? R : 0.0);
            float y1 = c1.y - (isRound(c1) > 0.5 ? R : 0.0);
            if (y1 > y0)
                sd = min(sd, distSeg(p, float2(c1.x, y0), float2(c1.x, y1)));
        }
        if ((eC > 0.5) != (eT > 0.5))
        {
            float x0 = c0.x + (isRound(c0) > 0.5 ? R : 0.0);
            float x1 = c1.x - (isRound(float2(c1.x, c0.y)) > 0.5 ? R : 0.0);
            if (x1 > x0)
                sd = min(sd, distSeg(p, float2(x0, c0.y), float2(x1, c0.y)));
        }
        if ((eC > 0.5) != (eB > 0.5))
        {
            float x0 = c0.x + (isRound(float2(c0.x, c1.y)) > 0.5 ? R : 0.0);
            float x1 = c1.x - (isRound(c1) > 0.5 ? R : 0.0);
            if (x1 > x0)
                sd = min(sd, distSeg(p, float2(x0, c1.y), float2(x1, c1.y)));
        }
    }

    float fog = 0.0;
    if (fogHard > 0.5)
    {
        if (softW < 0.0001)
            fog = 1.0;
        else
            fog = smoothstep(0.0, softW, sd);
    }

    return float4(FogColor.rgb, FogColor.a * saturate(fog));
}
