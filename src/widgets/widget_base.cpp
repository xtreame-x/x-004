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

bool WidgetBase::Create(HWND hParent) {
    HINSTANCE hInstance = GetModuleHandle(NULL);

    if (!g_classRegistered) {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = WidgetBase::WndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL; // Direct2D handles drawing
        wc.lpszClassName = WIDGET_WINDOW_CLASS;

        if (!RegisterClassExW(&wc)) {
            return false;
        }
        g_classRegistered = true;
    }

    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_LAYERED;
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

    // Setup z-order
    if (m_isPinned) {
        SetWindowPos(m_hwnd, HWND_BOTTOM, m_x, m_y, m_width, m_height, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else if (m_isAlwaysOnTop) {
        SetWindowPos(m_hwnd, HWND_TOPMOST, m_x, m_y, m_width, m_height, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    if (m_visible) {
        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    }

    return true;
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

void WidgetBase::SetPinned(bool pinned) {
    m_isPinned = pinned;
    if (pinned) {
        m_isAlwaysOnTop = false;
    }
    if (m_hwnd) {
        SetWindowPos(m_hwnd, pinned ? HWND_BOTTOM : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        Redraw();
    }
}

void WidgetBase::SetAlwaysOnTop(bool onTop) {
    m_isAlwaysOnTop = onTop;
    if (onTop) {
        m_isPinned = false;
    }
    if (m_hwnd) {
        SetWindowPos(m_hwnd, onTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
    D2DContext::DrawPinButton(m_pRenderTarget, GetPinRect(), m_isPinned, m_hoverPin);
    D2DContext::DrawCloseButton(m_pRenderTarget, GetCloseRect(), m_hoverClose);

    // Content
    OnRenderContent(m_pRenderTarget);

    // Resize grip indicator
    DrawResizeGrip(m_pRenderTarget);

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
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            Render();
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // Handle active resize drag
            if (m_isResizing) {
                POINT ptNow;
                GetCursorPos(&ptNow);
                int dx = ptNow.x - m_resizeStartMouse.x;
                int dy = ptNow.y - m_resizeStartMouse.y;

                int newX = m_resizeStartRect.left;
                int newY = m_resizeStartRect.top;
                int newW = m_resizeStartRect.right - m_resizeStartRect.left;
                int newH = m_resizeStartRect.bottom - m_resizeStartRect.top;

                // Adjust based on which edge/corner is being dragged
                if (m_resizeEdge == RESIZE_LEFT || m_resizeEdge == RESIZE_TOPLEFT || m_resizeEdge == RESIZE_BOTTOMLEFT) {
                    newX += dx;
                    newW -= dx;
                }
                if (m_resizeEdge == RESIZE_RIGHT || m_resizeEdge == RESIZE_TOPRIGHT || m_resizeEdge == RESIZE_BOTTOMRIGHT) {
                    newW += dx;
                }
                if (m_resizeEdge == RESIZE_TOP || m_resizeEdge == RESIZE_TOPLEFT || m_resizeEdge == RESIZE_TOPRIGHT) {
                    newY += dy;
                    newH -= dy;
                }
                if (m_resizeEdge == RESIZE_BOTTOM || m_resizeEdge == RESIZE_BOTTOMLEFT || m_resizeEdge == RESIZE_BOTTOMRIGHT) {
                    newH += dy;
                }

                // Clamp to min/max sizes
                if (newW < m_minWidth) {
                    if (m_resizeEdge == RESIZE_LEFT || m_resizeEdge == RESIZE_TOPLEFT || m_resizeEdge == RESIZE_BOTTOMLEFT) {
                        newX -= (m_minWidth - newW);
                    }
                    newW = m_minWidth;
                }
                if (newW > m_maxWidth) {
                    if (m_resizeEdge == RESIZE_LEFT || m_resizeEdge == RESIZE_TOPLEFT || m_resizeEdge == RESIZE_BOTTOMLEFT) {
                        newX -= (m_maxWidth - newW);
                    }
                    newW = m_maxWidth;
                }
                if (newH < m_minHeight) {
                    if (m_resizeEdge == RESIZE_TOP || m_resizeEdge == RESIZE_TOPLEFT || m_resizeEdge == RESIZE_TOPRIGHT) {
                        newY -= (m_minHeight - newH);
                    }
                    newH = m_minHeight;
                }
                if (newH > m_maxHeight) {
                    if (m_resizeEdge == RESIZE_TOP || m_resizeEdge == RESIZE_TOPLEFT || m_resizeEdge == RESIZE_TOPRIGHT) {
                        newY -= (m_maxHeight - newH);
                    }
                    newH = m_maxHeight;
                }

                // Apply new position and size
                m_x = newX;
                m_y = newY;
                m_width = newW;
                m_height = newH;

                SetWindowPos(hwnd, NULL, newX, newY, newW, newH, SWP_NOZORDER | SWP_NOACTIVATE);
                HRGN hRgn = CreateRoundRectRgn(0, 0, newW + 1, newH + 1, 18, 18);
                SetWindowRgn(hwnd, hRgn, TRUE);
                if (m_pRenderTarget) {
                    m_pRenderTarget->Resize(D2D1::SizeU(newW, newH));
                }
                Redraw();
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

            // Update cursor based on resize edge hover
            if (!m_isLocked) {
                ResizeEdge edge = HitTestResize(x, y);
                UpdateCursorForEdge(edge);
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
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            SetFocus(hwnd);

            D2D1_RECT_F pinRect = GetPinRect();
            D2D1_RECT_F closeRect = GetCloseRect();

            if (x >= closeRect.left && x <= closeRect.right && y >= closeRect.top && y <= closeRect.bottom) {
                Show(false);
                if (m_pManager) m_pManager->SaveConfig();
                return 0;
            }

            if (x >= pinRect.left && x <= pinRect.right && y >= pinRect.top && y <= pinRect.bottom) {
                SetPinned(!m_isPinned);
                if (m_pManager) m_pManager->SaveConfig();
                return 0;
            }

            // Check if subclass handled mouse down
            if (OnMouseDown(x, y, false)) {
                Redraw();
                return 0;
            }

            // Check resize hit first
            if (!m_isLocked) {
                ResizeEdge edge = HitTestResize(x, y);
                if (edge != RESIZE_NONE) {
                    m_isResizing = true;
                    m_resizeEdge = edge;
                    GetCursorPos(&m_resizeStartMouse);
                    RECT rc;
                    GetWindowRect(hwnd, &rc);
                    m_resizeStartRect = rc;
                    SetCapture(hwnd);
                    return 0;
                }

                // Otherwise, drag window
                ReleaseCapture();
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (m_isResizing) {
                ReleaseCapture();
                m_isResizing = false;
                m_resizeEdge = RESIZE_NONE;
                if (m_pManager) m_pManager->SaveConfig();
            }
            return 0;
        }

        case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT && !m_isLocked) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                ResizeEdge edge = HitTestResize(pt.x, pt.y);
                if (edge != RESIZE_NONE) {
                    UpdateCursorForEdge(edge);
                    return TRUE;
                }
            }
            break;
        }

        case WM_RBUTTONUP: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

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

        case WM_EXITSIZEMOVE: {
            if (m_pManager) {
                m_pManager->SaveConfig();
            }
            break;
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

    InsertMenuW(hMenu, 0, MF_BYPOSITION | (m_isPinned ? MF_CHECKED : MF_UNCHECKED), 101, L"Pin to Desktop (Bottom)");
    InsertMenuW(hMenu, 1, MF_BYPOSITION | (m_isAlwaysOnTop ? MF_CHECKED : MF_UNCHECKED), 102, L"Always on Top");
    InsertMenuW(hMenu, 2, MF_BYPOSITION | (m_isLocked ? MF_CHECKED : MF_UNCHECKED), 103, L"Lock Position");
    InsertMenuW(hMenu, 3, MF_SEPARATOR, 0, NULL);

    // Opacity submenu
    HMENU hOpacityMenu = CreatePopupMenu();
    InsertMenuW(hOpacityMenu, 0, MF_BYPOSITION | ((m_opacity > 0.98f) ? MF_CHECKED : MF_UNCHECKED), 201, L"100% Opacity");
    InsertMenuW(hOpacityMenu, 1, MF_BYPOSITION | ((m_opacity > 0.88f && m_opacity <= 0.98f) ? MF_CHECKED : MF_UNCHECKED), 202, L"90% Opacity");
    InsertMenuW(hOpacityMenu, 2, MF_BYPOSITION | ((m_opacity > 0.78f && m_opacity <= 0.88f) ? MF_CHECKED : MF_UNCHECKED), 203, L"80% Opacity");
    InsertMenuW(hOpacityMenu, 3, MF_BYPOSITION | ((m_opacity <= 0.78f) ? MF_CHECKED : MF_UNCHECKED), 204, L"70% Opacity");
    InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hOpacityMenu, L"Widget Opacity");

    InsertMenuW(hMenu, 5, MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, 6, MF_BYPOSITION, 104, L"Hide This Widget");
    InsertMenuW(hMenu, 7, MF_BYPOSITION, 105, L"Show All Widgets");
    InsertMenuW(hMenu, 8, MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, 9, MF_BYPOSITION, 106, L"Exit WinWidgets");

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, screenX, screenY, 0, m_hwnd, NULL);

    DestroyMenu(hMenu);

    switch (cmd) {
        case 101: // Toggle Pinned
            SetPinned(!m_isPinned);
            if (m_pManager) m_pManager->SaveConfig();
            break;
        case 102: // Toggle Always on Top
            SetAlwaysOnTop(!m_isAlwaysOnTop);
            if (m_pManager) m_pManager->SaveConfig();
            break;
        case 103: // Toggle Lock
            SetLocked(!m_isLocked);
            if (m_pManager) m_pManager->SaveConfig();
            break;
        case 201: SetOpacity(1.0f); if (m_pManager) m_pManager->SaveConfig(); break;
        case 202: SetOpacity(0.90f); if (m_pManager) m_pManager->SaveConfig(); break;
        case 203: SetOpacity(0.80f); if (m_pManager) m_pManager->SaveConfig(); break;
        case 204: SetOpacity(0.70f); if (m_pManager) m_pManager->SaveConfig(); break;
        case 104:
            Show(false);
            if (m_pManager) m_pManager->SaveConfig();
            break;
        case 105:
            if (m_pManager) m_pManager->ShowAll();
            break;
        case 106:
            PostQuitMessage(0);
            break;
    }
}

WidgetBase::ResizeEdge WidgetBase::HitTestResize(int x, int y) const {
    const int border = 6;

    bool onLeft   = (x >= 0 && x < border);
    bool onRight  = (x > m_width - border && x <= m_width);
    bool onTop    = (y >= 0 && y < border);
    bool onBottom = (y > m_height - border && y <= m_height);

    if (onTop && onLeft)     return RESIZE_TOPLEFT;
    if (onTop && onRight)    return RESIZE_TOPRIGHT;
    if (onBottom && onLeft)  return RESIZE_BOTTOMLEFT;
    if (onBottom && onRight) return RESIZE_BOTTOMRIGHT;
    if (onLeft)              return RESIZE_LEFT;
    if (onRight)             return RESIZE_RIGHT;
    if (onTop)               return RESIZE_TOP;
    if (onBottom)            return RESIZE_BOTTOM;

    return RESIZE_NONE;
}

void WidgetBase::DrawResizeGrip(ID2D1RenderTarget* rt) {
    ID2D1SolidColorBrush* pBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.3f), &pBrush);
    if (!pBrush) return;

    float bx = (float)m_width - 5.0f;
    float by = (float)m_height - 5.0f;

    // Three diagonal lines forming a grip triangle
    for (int i = 0; i < 3; i++) {
        float offset = (float)i * 4.0f;
        rt->DrawLine(
            D2D1::Point2F(bx - offset, by),
            D2D1::Point2F(bx, by - offset),
            pBrush, 1.0f
        );
    }

    pBrush->Release();
}

void WidgetBase::UpdateCursorForEdge(ResizeEdge edge) {
    LPCWSTR cursorId = (LPCWSTR)IDC_ARROW;
    switch (edge) {
        case RESIZE_LEFT:
        case RESIZE_RIGHT:
            cursorId = (LPCWSTR)IDC_SIZEWE;
            break;
        case RESIZE_TOP:
        case RESIZE_BOTTOM:
            cursorId = (LPCWSTR)IDC_SIZENS;
            break;
        case RESIZE_TOPLEFT:
        case RESIZE_BOTTOMRIGHT:
            cursorId = (LPCWSTR)IDC_SIZENWSE;
            break;
        case RESIZE_TOPRIGHT:
        case RESIZE_BOTTOMLEFT:
            cursorId = (LPCWSTR)IDC_SIZENESW;
            break;
        default:
            cursorId = (LPCWSTR)IDC_ARROW;
            break;
    }
    SetCursor(LoadCursorW(NULL, cursorId));
}
