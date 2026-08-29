/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/InputManager/InputManager.cpp
 *  PURPOSE:     Keyboard / text input manager, inspired by MTA CKeyBinds
 *
 *****************************************************************************/

#include "InputManager.h"
#include "RenderWare.h"
#include "WindowMode.h"

InputManager* InputManager::s_pInstance = nullptr;

InputManager::InputManager()
    : m_pDevice(nullptr)
    , m_hWnd(nullptr)
    , m_pOriginalWndProc(nullptr)
    , m_bInitialized(false)
    , m_bTextInputEnabled(false)
    , m_bCursorOverride(false)
    , m_nCaretPos(0)
    , m_nMaxLength(256)
    , m_bEnterPressed(false)
    , m_bEscapePressed(false)
    , m_wheelDeltaAcc(0)
{
}

InputManager::~InputManager()
{
    Shutdown();
}

bool InputManager::Initialize(LPDIRECT3DDEVICE9 pDevice)
{
    if (m_bInitialized)
        return true;

    if (!pDevice)
        return false;

    m_pDevice = pDevice;

    D3DDEVICE_CREATION_PARAMETERS params{};
    if (FAILED(m_pDevice->GetCreationParameters(&params)) || !params.hFocusWindow)
        return false;

    m_hWnd = params.hFocusWindow;
    s_pInstance = this;
    AttachWndProc();

    m_bInitialized = true;
    return true;
}

void InputManager::Shutdown()
{
    SetCursorOverride(false);
    DisableTextInput();
    DetachWndProc();

    if (s_pInstance == this)
        s_pInstance = nullptr;

    m_pDevice = nullptr;
    m_hWnd = nullptr;
    m_bInitialized = false;
    m_wheelDeltaAcc = 0;
    ClearText();
    ClearFrameFlags();
}

void InputManager::SetCursorOverride(bool enable)
{
    m_bCursorOverride = enable;
    if (enable)
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
}

void InputManager::AttachWndProc()
{
    if (!m_hWnd || m_pOriginalWndProc)
        return;

    m_pOriginalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
        m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook)));
}

void InputManager::DetachWndProc()
{
    if (!m_hWnd || !m_pOriginalWndProc)
        return;

    SetWindowLongPtrA(m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_pOriginalWndProc));
    m_pOriginalWndProc = nullptr;
}

LRESULT CALLBACK InputManager::WndProcHook(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    InputManager* pThis = s_pInstance;

    if (uMsg == WM_ACTIVATE || uMsg == WM_ACTIVATEAPP)
    {
        const bool active = (uMsg == WM_ACTIVATE)
            ? (LOWORD(wParam) != WA_INACTIVE)
            : (wParam != 0);
        if (active)
            WindowMode::OnGotFocus();
        else
            WindowMode::OnLostFocus();
    }

    // Before game WndProc — mouse-move otherwise clears cursor and flickers
    if (uMsg == WM_SETCURSOR)
    {
        const bool menuCursor = pThis && pThis->m_bCursorOverride;
        const bool unfocused = GetForegroundWindow() != hwnd;
        if (menuCursor || unfocused)
        {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            CURSORINFO ci{ sizeof(ci) };
            if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING))
            {
                while (ShowCursor(TRUE) < 0)
                {
                }
            }
            return TRUE;
        }
    }

    if (pThis && uMsg == WM_MOUSEWHEEL)
        pThis->m_wheelDeltaAcc += GET_WHEEL_DELTA_WPARAM(wParam);

    if (uMsg == WM_CLOSE)
    {
        RsGlobal.quit = TRUE;
        return 0;
    }

    if (uMsg == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_CLOSE)
    {
        RsGlobal.quit = TRUE;
        return 0;
    }

    if (pThis && pThis->ProcessMessage(hwnd, uMsg, wParam, lParam))
        return 0;

    if (pThis && pThis->m_pOriginalWndProc)
        return CallWindowProcA(pThis->m_pOriginalWndProc, hwnd, uMsg, wParam, lParam);

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

bool InputManager::ProcessMessage(HWND /*hwnd*/, UINT uMsg, WPARAM wParam, LPARAM /*lParam*/)
{
    if (!m_bTextInputEnabled)
        return false;

    switch (uMsg)
    {
    case WM_CHAR:
        return ProcessCharacter(wParam);

    case WM_KEYDOWN:
        return ProcessKeyStroke(wParam, true);

    case WM_KEYUP:
        return ProcessKeyStroke(wParam, false);

    default:
        break;
    }

    return false;
}

bool InputManager::ProcessCharacter(WPARAM wChar)
{
    if (!m_bTextInputEnabled)
        return false;

    // Control characters handled in ProcessKeyStroke
    if (wChar < 32)
        return false;

    InsertChar(static_cast<wchar_t>(wChar));
    return true;
}

bool InputManager::ProcessKeyStroke(WPARAM wKey, bool bDown)
{
    if (!m_bTextInputEnabled || !bDown)
        return false;

    switch (wKey)
    {
    case VK_BACK:
        DeleteBackward();
        return true;

    case VK_DELETE:
        DeleteForward();
        return true;

    case VK_LEFT:
        MoveCaretLeft();
        return true;

    case VK_RIGHT:
        MoveCaretRight();
        return true;

    case VK_HOME:
        MoveCaretHome();
        return true;

    case VK_END:
        MoveCaretEnd();
        return true;

    case VK_RETURN:
        m_bEnterPressed = true;
        return true;

    case VK_ESCAPE:
        m_bEscapePressed = true;
        return true;

    default:
        break;
    }

    return false;
}

void InputManager::EnableTextInput(bool bEnable)
{
    m_bTextInputEnabled = bEnable;
    if (!bEnable)
        ClearFrameFlags();
}

void InputManager::SetText(const char* szText)
{
    m_strText = szText ? szText : "";
    if (m_strText.size() > m_nMaxLength)
        m_strText.resize(m_nMaxLength);
    m_nCaretPos = m_strText.size();
}

void InputManager::SetText(const std::string& strText)
{
    SetText(strText.c_str());
}

void InputManager::ClearText()
{
    m_strText.clear();
    m_nCaretPos = 0;
}

void InputManager::SetCaretPos(size_t nPos)
{
    m_nCaretPos = nPos;
    ClampCaret();
}

void InputManager::ClearFrameFlags()
{
    m_bEnterPressed = false;
    m_bEscapePressed = false;
}

int InputManager::ConsumeMouseWheelDelta()
{
    const int d = m_wheelDeltaAcc;
    m_wheelDeltaAcc = 0;
    return d;
}

void InputManager::InsertChar(wchar_t wChar)
{
    if (m_strText.size() >= m_nMaxLength)
        return;

    // Keep ASCII / simple Latin for now; UTF-8 path can be added later
    if (wChar > 127)
        return;

    const char ch = static_cast<char>(wChar);
    m_strText.insert(m_nCaretPos, 1, ch);
    ++m_nCaretPos;
    ClampCaret();
}

void InputManager::DeleteBackward()
{
    if (m_nCaretPos == 0 || m_strText.empty())
        return;

    --m_nCaretPos;
    m_strText.erase(m_nCaretPos, 1);
}

void InputManager::DeleteForward()
{
    if (m_nCaretPos >= m_strText.size())
        return;

    m_strText.erase(m_nCaretPos, 1);
}

void InputManager::MoveCaretLeft()
{
    if (m_nCaretPos > 0)
        --m_nCaretPos;
}

void InputManager::MoveCaretRight()
{
    if (m_nCaretPos < m_strText.size())
        ++m_nCaretPos;
}

void InputManager::MoveCaretHome()
{
    m_nCaretPos = 0;
}

void InputManager::MoveCaretEnd()
{
    m_nCaretPos = m_strText.size();
}

void InputManager::ClampCaret()
{
    if (m_nCaretPos > m_strText.size())
        m_nCaretPos = m_strText.size();
}
