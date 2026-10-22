#include "widget_hub.h"
#include "../graphics/d2d_context.h"
#include "../app.h"

WidgetHub::WidgetHub()
    : WidgetBase(L"hub", L"WINWIDGETS HUB", 440, 52) {
    m_buttons = {
        { L"clock",    L"Clock",    D2D1::ColorF(0.22f, 0.75f, 0.98f) },
        { L"system",   L"System",   D2D1::ColorF(0.20f, 0.82f, 0.55f) },
        { L"weather",  L"Weather",  D2D1::ColorF(0.98f, 0.75f, 0.20f) },
        { L"calendar", L"Calendar", D2D1::ColorF(0.66f, 0.33f, 0.97f) },
        { L"tasks",    L"Tasks",    D2D1::ColorF(0.18f, 0.78f, 0.65f) },
        { L"news",     L"News",     D2D1::ColorF(0.98f, 0.55f, 0.25f) }
    };
}

D2D1_RECT_F WidgetHub::GetButtonRect(size_t index) const {
    float startX = 14.0f;
    float btnW = 66.0f;
    float btnH = 26.0f;
    float gap = 5.0f;
    float topY = 14.0f;

    float x = startX + index * (btnW + gap);
    return D2D1::RectF(x, topY, x + btnW, topY + btnH);
}

void WidgetHub::OnMouseMove(int x, int y) {
    int oldHover = m_hoverButtonIndex;
    m_hoverButtonIndex = -1;

    for (size_t i = 0; i < m_buttons.size(); i++) {
        D2D1_RECT_F r = GetButtonRect(i);
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
            m_hoverButtonIndex = (int)i;
            break;
        }
    }

    if (oldHover != m_hoverButtonIndex) {
        Redraw();
    }
}

void WidgetHub::OnMouseLeave() {
    if (m_hoverButtonIndex != -1) {
        m_hoverButtonIndex = -1;
        Redraw();
    }
}

bool WidgetHub::OnMouseDown(int x, int y, bool isRight) {
    if (isRight) return false;

    for (size_t i = 0; i < m_buttons.size(); i++) {
        D2D1_RECT_F r = GetButtonRect(i);
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
            if (m_pManager) {
                m_pManager->ToggleWidget(m_buttons[i].id);
                Redraw();
            }
            return true;
        }
    }
    return false;
}

void WidgetHub::OnRenderContent(ID2D1RenderTarget* rt) {
    D2DContext& ctx = D2DContext::Get();

    for (size_t i = 0; i < m_buttons.size(); i++) {
        D2D1_RECT_F r = GetButtonRect(i);
        D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(r, 6.0f, 6.0f);

        bool isActive = false;
        if (m_pManager) {
            WidgetBase* w = m_pManager->GetWidget(m_buttons[i].id);
            if (w && w->IsVisible()) {
                isActive = true;
            }
        }

        bool isHovered = (m_hoverButtonIndex == (int)i);

        // Background
        ID2D1SolidColorBrush* pBg = nullptr;
        if (isActive) {
            D2D1_COLOR_F c = m_buttons[i].activeColor;
            c.a = isHovered ? 0.35f : 0.22f;
            rt->CreateSolidColorBrush(c, &pBg);
        } else {
            rt->CreateSolidColorBrush(isHovered ? D2D1::ColorF(0.25f, 0.30f, 0.40f, 0.35f) : D2D1::ColorF(0.15f, 0.18f, 0.25f, 0.25f), &pBg);
        }

        if (pBg) {
            rt->FillRoundedRectangle(rounded, pBg);
            pBg->Release();
        }

        // Border
        ID2D1SolidColorBrush* pBorder = nullptr;
        if (isActive) {
            D2D1_COLOR_F c = m_buttons[i].activeColor;
            c.a = isHovered ? 0.90f : 0.65f;
            rt->CreateSolidColorBrush(c, &pBorder);
        } else {
            rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, isHovered ? 0.20f : 0.08f), &pBorder);
        }

        if (pBorder) {
            rt->DrawRoundedRectangle(rounded, pBorder, 1.0f);
            pBorder->Release();
        }

        // Text
        ID2D1SolidColorBrush* pText = nullptr;
        if (isActive) {
            rt->CreateSolidColorBrush(m_buttons[i].activeColor, &pText);
        } else {
            rt->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.75f, 0.85f, isHovered ? 0.95f : 0.60f), &pText);
        }

        if (pText) {
            D2D1_RECT_F textR = D2D1::RectF(r.left + 2.0f, r.top + 5.0f, r.right - 2.0f, r.bottom);
            rt->DrawText(
                m_buttons[i].label.c_str(),
                (UINT32)m_buttons[i].label.length(),
                ctx.GetFormatSmall(),
                textR,
                pText
            );
            pText->Release();
        }
    }
}
