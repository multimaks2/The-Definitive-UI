/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/Draw/Draw.h
 *  PURPOSE:     2D drawing helpers (texture + text), inspired by MTA CGraphics
 *
 *****************************************************************************/

#pragma once

#include <d3d9.h>
#include <d3dx9.h>

class Draw
{
public:
    Draw();
    ~Draw();

    bool Initialize(LPDIRECT3DDEVICE9 pDevice);
    void Shutdown();

    bool IsInitialized() const { return m_bInitialized; }
    LPDIRECT3DDEVICE9 GetDevice() const { return m_pDevice; }

    // Capture/restore full D3D state around UI so world textures survive after pause
    void BeginUi();
    void EndUi();

    void DrawTexture(LPDIRECT3DTEXTURE9 pTexture, float fX, float fY, float fWidth, float fHeight,
                     DWORD dwColor = 0xFFFFFFFF, float fRotation = 0.0f);
    void DrawTexture(LPDIRECT3DTEXTURE9 pTexture, float fX, float fY, float fWidth, float fHeight,
                     float fU, float fV, float fSizeU, float fSizeV, DWORD dwColor = 0xFFFFFFFF);

    void DrawRect(float fX, float fY, float fWidth, float fHeight, DWORD dwColor);
    void DrawCircle(float fCenterX, float fCenterY, float fRadius, DWORD dwColor);
    // Soft-edged circle (fringe rings) for UI knobs
    void DrawCircleAA(float fCenterX, float fCenterY, float fRadius, DWORD dwColor);
    // Soft-edged < / > chevron (pointLeft=true → <). dir: 0=left 1=right 2=up 3=down
    void DrawArrowAA(float fCenterX, float fCenterY, float fSize, bool pointLeft, DWORD dwColor);
    void DrawArrowAA(float fCenterX, float fCenterY, float fSize, int dir, DWORD dwColor);
    // Filled sector — opaque write (blend off) for RT masks
    void DrawQuarterDisk(float fCenterX, float fCenterY, float fRadius, float fAngle0, float fAngle1,
                         DWORD dwColor, int segments = 12);
    // Continuous thick stroke along a circular arc (screen Y down)
    void DrawThickArc(float fCenterX, float fCenterY, float fRadius, float fAngle0, float fAngle1,
                      float fThickness, DWORD dwColor, int segments = 48);
    // Continuous thick stroke along a straight segment (centerline)
    void DrawThickLine(float fX0, float fY0, float fX1, float fY1, float fThickness, DWORD dwColor);

    // Rect with text glyphs cut out (background shows through letters)
    void DrawRectCutoutText(float fX, float fY, float fWidth, float fHeight, DWORD dwColor,
                            float fTextLeft, float fTextTop, float fTextRight, float fTextBottom,
                            const char* szText,
                            DWORD dwFormat = DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE);

    struct CutoutSpan
    {
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
        const char* text = nullptr;
        DWORD format = DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE;
    };

    // Same as DrawRectCutoutText, but every span is punched before the rect is composited
    void DrawRectCutoutTexts(float fX, float fY, float fWidth, float fHeight, DWORD dwColor,
                             const CutoutSpan* spans, int count);

    // Texture with text glyphs cut out (same punch as DrawRectCutoutText)
    void DrawTextureCutoutText(LPDIRECT3DTEXTURE9 pTexture, float fX, float fY, float fWidth, float fHeight,
                               float fTextLeft, float fTextTop, float fTextRight, float fTextBottom,
                               const char* szText,
                               DWORD dwFormat = DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE);

    // One full-row cutout: label + < value > (optional larger arrow fonts on hover)
    void DrawRectCutoutCycleValue(float fX, float fY, float fWidth, float fHeight, DWORD dwColor,
                                  float labelL, float labelR, const char* label,
                                  float leftL, float leftR, const char* leftArrow, ID3DXFont* leftFont,
                                  float valueL, float valueR, const char* valueText,
                                  float rightL, float rightR, const char* rightArrow, ID3DXFont* rightFont,
                                  float textTop, float textBottom,
                                  DWORD valueFormat = DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE);

    // Offscreen clip RT: draw in local (0..w, 0..h) coords, then blit to screen
    bool BeginClipRT(float screenX, float screenY, float width, float height);
    void EndClipRT();

    void DrawString(float fX, float fY, DWORD dwColor, float fScale, const char* szText);
    void DrawString(float fLeft, float fTop, float fRight, float fBottom, DWORD dwColor, const char* szText,
                    float fScaleX = 1.0f, float fScaleY = 1.0f, DWORD dwFormat = DT_LEFT | DT_TOP,
                    bool bOutline = false, DWORD dwOutlineColor = 0xFF588942,
                    float fOutlineOffsetX = 3.0f, float fOutlineOffsetY = 3.0f);

    // Same glyphs as DrawString, but from a separately created larger font (crisp hover)
    void DrawStringHover(float fLeft, float fTop, float fRight, float fBottom, DWORD dwColor, const char* szText,
                         DWORD dwFormat = DT_LEFT | DT_TOP);

    float GetTextWidth(const char* szText, float fScale = 1.0f) const;
    float GetFontHeight(float fScale = 1.0f) const;

    bool EnsureFontHeight(int height);
    bool EnsureHoverFontHeight(int height);
    void NotifyBackbufferChanged();
    ID3DXFont* GetFont() const { return m_pFont; }
    ID3DXFont* GetHoverFont() const { return m_pFontHover; }

    // Exact pixel fill (no -0.5), blend off — for writing masks into RTs
    void FillRectRaw(float fX, float fY, float fWidth, float fHeight, DWORD dwColor);

private:
    bool CreateResources();
    void ReleaseResources();
    bool CreateFontFace(int height);
    bool CreateHoverFontFace(int height);
    bool EnsureCutoutTarget(UINT width, UINT height);
    bool EnsureClipTarget(UINT width, UINT height);
    void DropDefaultPoolRts();
    void RecreateFonts();
    void SyncBackbufferSize();
    void RefreshAfterFocusRestore();
    void BlitCutout(float fX, float fY, float fWidth, float fHeight, float fU2, float fV2);
    void BlitClipToScreen();
    void DrawChevronSolid(float fCenterX, float fCenterY, float fSize, bool pointLeft, DWORD dwColor);
    void DrawChevronSolid(float fCenterX, float fCenterY, float fSize, int dir, DWORD dwColor);
    void DrawStringInternal(RECT& rect, DWORD dwColor, const char* szText, float fScaleX, float fScaleY,
                            DWORD dwFormat, bool bOutline, DWORD dwOutlineColor,
                            float fOutlineOffsetX, float fOutlineOffsetY, ID3DXFont* pFont);

    LPDIRECT3DDEVICE9  m_pDevice;
    LPD3DXSPRITE       m_pSprite;
    ID3DXFont*         m_pFont;
    ID3DXFont*         m_pFontHover;
    LPDIRECT3DTEXTURE9 m_pCutoutTex;
    LPDIRECT3DSURFACE9 m_pCutoutSurf;
    LPDIRECT3DTEXTURE9 m_pMaskTex;
    LPDIRECT3DSURFACE9 m_pMaskSurf;
    UINT               m_nCutoutW;
    UINT               m_nCutoutH;
    LPDIRECT3DTEXTURE9 m_pClipTex;
    LPDIRECT3DSURFACE9 m_pClipSurf;
    LPDIRECT3DSURFACE9 m_pClipSavedRT;
    LPDIRECT3DSURFACE9 m_pClipSavedDS;
    LPDIRECT3DSURFACE9 m_pClipSavedRwRT;
    LPDIRECT3DSURFACE9 m_pClipSavedRwDS;
    D3DVIEWPORT9       m_clipSavedVp;
    UINT               m_nClipTexW;
    UINT               m_nClipTexH;
    float              m_clipScreenX;
    float              m_clipScreenY;
    float              m_clipW;
    float              m_clipH;
    bool               m_bClipActive;
    LPDIRECT3DSTATEBLOCK9 m_pStateBlock;
    int                m_nUiDepth;
    int                m_nFontHeight;
    int                m_nFontHoverHeight;
    UINT               m_nBackW;
    UINT               m_nBackH;
    bool               m_bUiFocused;
    bool               m_bInitialized;
};
