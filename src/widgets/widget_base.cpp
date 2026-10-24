#include "widget_base.h"
#include "../graphics/d2d_context.h"
#include "../app.h"

static const wchar_t* WIDGET_WINDOW_CLASS = L"WinWidgetWindowClass";
static bool g_classRegistered = false;

WidgetBase::WidgetBase(const std::wstring& id, const std::wstring& title, int defaultWidth, int defaultHeight)
    : m_id(id), m_title(title), m_width(defaultWidth), m_height(defaultHeight) {
}

WidgetBase::~WidgetBase() {
    Destroy();
}

HWND WidgetBase::FindDesktopHostWindow() {
    // 1. Look for Progman hosting SHELLDLL_DefView
    HWND hProgman = FindWindowW(L"Progman", L"Program Manager");
    if (hProgman) {
        HWND hDefView = FindWindowExW(hProgman, NULL, L"SHELLDLL_DefView", NULL);
        if (hDefView) {
            return hProgman;
        }
    }

    // 2. In Windows 10/11, SHELLDLL_DefView may be parented to a WorkerW
    HWND hWorkerW = NULL;
    do {
        hWorkerW = FindWindowExW(NULL, hWorkerW, L"WorkerW", NULL);
        if (hWorkerW) {
            HWND hDefView = FindWindowExW(hWorkerW, NULL, L"SHELLDLL_DefView", NULL);
            if (hDefView) {
                return hWorkerW;
            }
        }
    } while (hWorkerW);

    // 3. Fallback to Progman or GetShellWindow
    if (hProgman) return hProgman;
    return GetShellWindow();
}

bool WidgetBase::Create(HWND hParent) {
    HINSTANCE hInstance = GetModuleHandle(NULL);

    if (!g_classRegistered) {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = WidgetBase::WndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = NULL; // Direct2D handles drawing
        wc.lpszClassName = WIDGET_WINDOW_CLASS;

        if (!RegisterClassExW(&wc)) {
            return false;
        }
        g_classRegistered = true;
    }

    // Extended styles:
    // WS_EX_TOOLWINDOW: Never shows in Alt-Tab and never shows in Taskbar
    // WS_EX_LAYERED: Enables smooth alpha translucency
    // WS_EX_NOACTIVATE: Never activates as an active application
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE;
    DWORD style = WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

    m_hwnd = CreateWindowExW(
        exStyle,
        WIDGET_WINDOW_CLASS,
        m_title.c_str(),
        style,
        m_x, m_y, m_width, m_height,
        hParent,
        NULL,
        hInstance,
        this
    );

    if (!m_hwnd) return false;

    // Apply smooth rounded window clipping region
    HRGN hRgn = CreateRoundRectRgn(0, 0, m_width + 1, m_height + 1, 18, 18);
    SetWindowRgn(m_hwnd, hRgn, TRUE);

    // Apply alpha transparency
    SetLayeredWindowAttributes(m_hwnd, 0, (BYTE)(m_opacity * 255.0f), LWA_ALPHA);

    // Apply Desktop Layer or selected display mode
    ApplyDisplayMode();

    if (m_visible) {
        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    }

    return true;
}

void WidgetBase::ApplyDisplayMode() {
    if (!m_hwnd) return;

    if (m_displayMode == MODE_DESKTOP_LAYER) {
        HWND hDesktopHost = FindDesktopHostWindow();
        if (hDesktopHost) {
            SetParent(m_hwnd, hDesktopHost);
            SetWindowPos(m_hwnd, HWND_TOP, m_x, m_y, m_width, m_height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        } else {
            SetParent(m_hwnd, NULL);
            HWND hShell = GetShellWindow();
            if (hShell) {
                SetWindowLongPtrW(m_hwnd, GWLP_HWNDPARENT, (LONG_PTR)hShell);
            }
            SetWindowPos(m_hwnd, HWND_BOTTOM, m_x, m_y, m_width, m_height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
    } else if (m_displayMode == MODE_ALWAYS_ON_TOP) {
        SetParent(m_hwnd, NULL);
        SetWindowLongPtrW(m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        SetWindowPos(m_hwnd, HWND_TOPMOST, m_x, m_y, m_width, m_height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    } else { // MODE_NORMAL
        SetParent(m_hwnd, NULL);
        SetWindowLongPtrW(m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        SetWindowPos(m_hwnd, HWND_NOTOPMOST, m_x, m_y, m_width, m_height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    Redraw();
}

void WidgetBase::SetDisplayMode(DisplayMode mode) {
    m_displayMode = mode;
    ApplyDisplayMode();
    if (m_pManager) {
        m_pManager->SaveConfig();
    }
}

void WidgetBase::Destroy() {
    if (m_pRenderTarget) {
        m_pRenderTarget->Release();
        m_pRenderTarget = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }
}

void WidgetBase::Show(bool show) {
    m_visible = show;
    if (m_hwnd) {
        ShowWindow(m_hwnd, show ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (show) {
            ApplyDisplayMode();
            Redraw();
        }
    }
}

void WidgetBase::SetPosition(int x, int y) {
    m_x = x;
    m_y = y;
    if (m_hwnd) {
        SetWindowPos(m_hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void WidgetBase::SetSize(int w, int h) {
    m_width = w;
    m_height = h;
    if (m_hwnd) {
        SetWindowPos(m_hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        HRGN hRgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, 18, 18);
        SetWindowRgn(m_hwnd, hRgn, TRUE);
        if (m_pRenderTarget) {
            m_pRenderTarget->Resize(D2D1::SizeU(w, h));
        }
        Redraw();
    }
}

void WidgetBase::SetLocked(bool locked) {
    m_isLocked = locked;
}

void WidgetBase::SetOpacity(float opacity) {
    if (opacity < 0.3f) opacity = 0.3f;
    if (opacity > 1.0f) opacity = 1.0f;
    m_opacity = opacity;
    if (m_hwnd) {
        SetLayeredWindowAttributes(m_hwnd, 0, (BYTE)(m_opacity * 255.0f), LWA_ALPHA);
    }
}

void WidgetBase::Redraw() {
    if (m_hwnd && m_visible) {
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

D2D1_RECT_F WidgetBase::GetCloseRect() const {
    return D2D1::RectF((float)m_width - 24.0f, 6.0f, (float)m_width - 6.0f, 24.0f);
}

D2D1_RECT_F WidgetBase::GetPinRect() const {
    return D2D1::RectF((float)m_width - 44.0f, 6.0f, (float)m_width - 26.0f, 24.0f);
}

bool WidgetBase::EnsureRenderTarget() {
    if (m_pRenderTarget) return true;

    D2DContext& ctx = D2DContext::Get();
    if (!ctx.GetD2DFactory()) return false;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = ctx.GetD2DFactory()->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        ),
        D2D1::HwndRenderTargetProperties(m_hwnd, size),
        &m_pRenderTarget
    );

    return SUCCEEDED(hr);
}

void WidgetBase::Render() {
    if (!EnsureRenderTarget()) return;

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

    D2D1_RECT_F panelRect = D2D1::RectF(0.0f, 0.0f, (float)m_width, (float)m_height);

    // Draw background glass panel
    D2DContext::DrawGlassPanel(m_pRenderTarget, panelRect, 18.0f, m_opacity);

    if (m_showWindowHeader) {
        // Title text
        D2D1_RECT_F titleRect = D2D1::RectF(14.0f, 7.0f, (float)m_width - 50.0f, 26.0f);
        ID2D1SolidColorBrush* pTitleBrush = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.75f, 0.88f, 0.85f), &pTitleBrush);
        if (pTitleBrush) {
            m_pRenderTarget->DrawText(
                m_title.c_str(),
                (UINT32)m_title.length(),
                D2DContext::Get().GetFormatTiny(),
                titleRect,
                pTitleBrush
            );
            pTitleBrush->Release();
        }

        // Header buttons (Pin & Close)
        bool isPinned = (m_displayMode == MODE_DESKTOP_LAYER);
        D2DContext::DrawPinButton(m_pRenderTarget, GetPinRect(), isPinned, m_hoverPin);
        D2DContext::DrawCloseButton(m_pRenderTarget, GetCloseRect(), m_hoverClose);
    }

    // Content
    OnRenderContent(m_pRenderTarget);

    HRESULT hr = m_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        m_pRenderTarget->Release();
        m_pRenderTarget = nullptr;
    }
}

LRESULT CALLBACK WidgetBase::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WidgetBase* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<WidgetBase*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<WidgetBase*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT WidgetBase::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_MOUSEACTIVATE: {
            // If text input specifically needs keyboard focus (like tasks input), activate briefly.
            // Otherwise NEVER activate as an app window!
            return m_requiresKeyboardFocus ? MA_ACTIVATE : MA_NOACTIVATE;
        }

        case WM_WINDOWPOSCHANGING: {
            WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
            if (m_displayMode == MODE_DESKTOP_LAYER) {
                // Intercept and strip SWP_HIDEWINDOW so "Show Desktop" (Win+D or 3-finger swipe down)
                // NEVER hides our desktop widgets!
                wp->flags &= ~SWP_HIDEWINDOW;
            }
            break;
        }

        case WM_SYSCOMMAND: {
            if ((wParam & 0xFFF0) == SC_MINIMIZE && m_displayMode == MODE_DESKTOP_LAYER) {
                return 0; // Reject minimization like an app
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            Render();
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = (short)LOWORD(lParam);
            int y = (short)HIWORD(lParam);

            if (m_isDragging) {
                POINT curPt;
                GetCursorPos(&curPt);
                int newX = m_dragStartPos.x + (curPt.x - m_dragStartCursor.x);
                int newY = m_dragStartPos.y + (curPt.y - m_dragStartCursor.y);
                SetPosition(newX, newY);
                return 0;
            }

            // Track mouse leave
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);

            D2D1_RECT_F pinRect = GetPinRect();
            D2D1_RECT_F closeRect = GetCloseRect();

            bool oldPin = m_hoverPin;
            bool oldClose = m_hoverClose;

            m_hoverPin = (x >= pinRect.left && x <= pinRect.right && y >= pinRect.top && y <= pinRect.bottom);
            m_hoverClose = (x >= closeRect.left && x <= closeRect.right && y >= closeRect.top && y <= closeRect.bottom);

            if (oldPin != m_hoverPin || oldClose != m_hoverClose) {
                Redraw();
            }

            OnMouseMove(x, y);
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (m_hoverPin || m_hoverClose) {
                m_hoverPin = false;
                m_hoverClose = false;
                Redraw();
            }
            OnMouseLeave();
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = (short)LOWORD(lParam);
            int y = (short)HIWORD(lParam);

            if (m_showWindowHeader) {
                D2D1_RECT_F pinRect = GetPinRect();
                D2D1_RECT_F closeRect = GetCloseRect();

                if (x >= closeRect.left && x <= closeRect.right && y >= closeRect.top && y <= closeRect.bottom) {
                    Show(false);
                    if (m_pManager) m_pManager->SaveConfig();
                    return 0;
                }

                if (x >= pinRect.left && x <= pinRect.right && y >= pinRect.top && y <= pinRect.bottom) {
                    // Toggle between Desktop Layer and Normal Floating
                    SetDisplayMode(m_displayMode == MODE_DESKTOP_LAYER ? MODE_NORMAL : MODE_DESKTOP_LAYER);
                    return 0;
                }
            }

            // Check if subclass handled mouse down (interactive buttons, checkboxes, city selection)
            if (OnMouseDown(x, y, false)) {
                Redraw();
                return 0;
            }

            // Non-activating manual dragging across the desktop
            if (!m_isLocked) {
                m_isDragging = true;
                SetCapture(hwnd);
                GetCursorPos(&m_dragStartCursor);
                m_dragStartPos = { m_x, m_y };
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (m_isDragging) {
                m_isDragging = false;
                ReleaseCapture();
                if (m_pManager) {
                    m_pManager->SaveConfig();
                }
                return 0;
            }
            break;
        }

        case WM_CAPTURECHANGED: {
            m_isDragging = false;
            break;
        }

        case WM_RBUTTONUP: {
            int x = (short)LOWORD(lParam);
            int y = (short)HIWORD(lParam);

            if (!OnMouseDown(x, y, true)) {
                POINT pt = { x, y };
                ClientToScreen(hwnd, &pt);
                ShowContextMenu(pt.x, pt.y);
            }
            return 0;
        }

        case WM_WINDOWPOSCHANGED: {
            WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
            if (!(wp->flags & SWP_NOMOVE)) {
                m_x = wp->x;
                m_y = wp->y;
            }
            break;
        }

        case WM_LBUTTONDBLCLK: {
            int x = (short)LOWORD(lParam);
            int y = (short)HIWORD(lParam);
            OnDoubleClick(x, y);
            return 0;
        }

        case WM_CHAR: {
            OnCharInput(static_cast<wchar_t>(wParam));
            return 0;
        }

        case WM_KEYDOWN: {
            OnKeyDown(wParam);
            return 0;
        }

        case WM_DESTROY: {
            m_hwnd = NULL;
            break;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void WidgetBase::ShowContextMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    InsertMenuW(hMenu, 0, MF_BYPOSITION | ((m_displayMode == MODE_DESKTOP_LAYER) ? MF_CHECKED : MF_UNCHECKED), 101, L"Desktop UI Layer (Wallpaper/Icons Page)");
    InsertMenuW(hMenu, 1, MF_BYPOSITION | ((m_displayMode == MODE_NORMAL) ? MF_CHECKED : MF_UNCHECKED), 102, L"Normal Floating Window");
    InsertMenuW(hMenu, 2, MF_BYPOSITION | ((m_displayMode == MODE_ALWAYS_ON_TOP) ? MF_CHECKED : MF_UNCHECKED), 103, L"Always on Top");
    InsertMenuW(hMenu, 3, MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, 4, MF_BYPOSITION | (m_isLocked ? MF_CHECKED : MF_UNCHECKED), 104, L"Lock Position");
    InsertMenuW(hMenu, 5, MF_SEPARATOR, 0, NULL);

    // Opacity submenu
    HMENU hOpacityMenu = CreatePopupMenu();
    InsertMenuW(hOpacityMenu, 0, MF_BYPOSITION | ((m_opacity > 0.98f) ? MF_CHECKED : MF_UNCHECKED), 201, L"100% Opacity");
    InsertMenuW(hOpacityMenu, 1, MF_BYPOSITION | ((m_opacity > 0.88f && m_opacity <= 0.98f) ? MF_CHECKED : MF_UNCHECKED), 202, L"90% Opacity");
    InsertMenuW(hOpacityMenu, 2, MF_BYPOSITION | ((m_opacity > 0.78f && m_opacity <= 0.88f) ? MF_CHECKED : MF_UNCHECKED), 203, L"80% Opacity");
    InsertMenuW(hOpacityMenu, 3, MF_BYPOSITION | ((m_opacity <= 0.78f) ? MF_CHECKED : MF_UNCHECKED), 204, L"70% Opacity");
    InsertMenuW(hMenu, 6, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hOpacityMenu, L"Widget Opacity");

    InsertMenuW(hMenu, 7, MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, 8, MF_BYPOSITION, 105, L"Hide This Widget");
    InsertMenuW(hMenu, 9, MF_BYPOSITION, 106, L"Show All Widgets");
    InsertMenuW(hMenu, 10, MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, 11, MF_BYPOSITION, 107, L"Exit WinWidgets");

    // Allow widgets to append custom menu items
    OnBuildContextMenu(hMenu);

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, screenX, screenY, 0, m_hwnd, NULL);

    DestroyMenu(hMenu);

    if (OnMenuCommand(cmd)) {
        return;
    }

    switch (cmd) {
        case 101: SetDisplayMode(MODE_DESKTOP_LAYER); break;
        case 102: SetDisplayMode(MODE_NORMAL); break;
        case 103: SetDisplayMode(MODE_ALWAYS_ON_TOP); break;
        case 104:
            SetLocked(!m_isLocked);
            if (m_pManager) m_pManager->SaveConfig();
            break;
        case 201: SetOpacity(1.0f); if (m_pManager) m_pManager->SaveConfig(); break;
        case 202: SetOpacity(0.90f); if (m_pManager) m_pManager->SaveConfig(); break;
        case 203: SetOpacity(0.80f); if (m_pManager) m_pManager->SaveConfig(); break;
        case 204: SetOpacity(0.70f); if (m_pManager) m_pManager->SaveConfig(); break;
        case 105:
            Show(false);
            if (m_pManager) m_pManager->SaveConfig();
            break;
        case 106:
            if (m_pManager) m_pManager->ShowAll();
            break;
        case 107:
            PostQuitMessage(0);
            break;
    }
}
