/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/InputManager/InputManager.h
 *  PURPOSE:     Keyboard / text input manager, inspired by MTA CKeyBinds
 *
 *****************************************************************************/

#pragma once

#include <windows.h>
#include <d3d9.h>
#include <string>

class InputManager
{
public:
    InputManager();
    ~InputManager();

    bool Initialize(LPDIRECT3DDEVICE9 pDevice);
    void Shutdown();

    bool IsInitialized() const { return m_bInitialized; }

    // Window message pump (MTA-style)
    bool ProcessMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    bool ProcessCharacter(WPARAM wChar);
    bool ProcessKeyStroke(WPARAM wKey, bool bDown);

    // Text input focus
    void EnableTextInput(bool bEnable = true);
    void DisableTextInput() { EnableTextInput(false); }
    bool IsTextInputEnabled() const { return m_bTextInputEnabled; }

    // Text buffer
    void        SetText(const char* szText);
    void        SetText(const std::string& strText);
    const char* GetText() const { return m_strText.c_str(); }
    std::string GetTextString() const { return m_strText; }
    void        ClearText();

    void SetMaxLength(size_t nMaxLength) { m_nMaxLength = nMaxLength; }
    size_t GetMaxLength() const { return m_nMaxLength; }

    size_t GetCaretPos() const { return m_nCaretPos; }
    void   SetCaretPos(size_t nPos);

    bool WasEnterPressed() const { return m_bEnterPressed; }
    bool WasEscapePressed() const { return m_bEscapePressed; }
    void ClearFrameFlags();

    // Keep OS arrow visible; swallow game WM_SETCURSOR (stops move-flicker)
    void SetCursorOverride(bool enable);
    bool IsCursorOverride() const { return m_bCursorOverride; }

    // Accumulated in WndProc (ImGui/AZ2-style) — consume once per frame
    int  ConsumeMouseWheelDelta();
    static InputManager* GetInstance() { return s_pInstance; }

    HWND GetWindow() const { return m_hWnd; }

private:
    static LRESULT CALLBACK WndProcHook(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void AttachWndProc();
    void DetachWndProc();
    void InsertChar(wchar_t wChar);
    void DeleteBackward();
    void DeleteForward();
    void MoveCaretLeft();
    void MoveCaretRight();
    void MoveCaretHome();
    void MoveCaretEnd();
    void ClampCaret();

    static InputManager* s_pInstance;

    LPDIRECT3DDEVICE9 m_pDevice;
    HWND              m_hWnd;
    WNDPROC           m_pOriginalWndProc;
    bool              m_bInitialized;
    bool              m_bTextInputEnabled;
    bool              m_bCursorOverride;

    std::string m_strText;
    size_t      m_nCaretPos;
    size_t      m_nMaxLength;

    bool m_bEnterPressed;
    bool m_bEscapePressed;
    int  m_wheelDeltaAcc;
};
