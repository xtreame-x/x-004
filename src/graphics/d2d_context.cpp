#include "d2d_context.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

D2DContext& D2DContext::Get() {
    static D2DContext instance;
    return instance;
}

D2DContext::D2DContext() {}

D2DContext::~D2DContext() {
    Cleanup();
}

bool D2DContext::Initialize() {
    if (m_pD2DFactory && m_pDWriteFactory) return true;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
    );
    if (FAILED(hr)) return false;

    // Create Text Formats
    // 1. Clock Display (Bold, clean typography)
    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI Variable Display",
        NULL,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        38.0f,
        L"en-us",
        &m_pFormatClock
    );
    if (!m_pFormatClock) {
        // Fallback to Segoe UI
        m_pDWriteFactory->CreateTextFormat(
            L"Segoe UI",
            NULL,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            38.0f,
            L"en-us",
            &m_pFormatClock
        );
    }

    // 2. Clock Small / Seconds
    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        18.0f,
        L"en-us",
        &m_pFormatClockSmall
    );

    // 3. Widget Title
    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        13.5f,
        L"en-us",
        &m_pFormatTitle
    );

    // 4. Subtitle / Greeting
    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_MEDIUM,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.0f,
        L"en-us",
        &m_pFormatSubtitle
    );

    // 5. Body
    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        11.5f,
        L"en-us",
        &m_pFormatBody
    );

    // 6. Body Bold
    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        11.5f,
        L"en-us",
        &m_pFormatBodyBold
    );

    // 7. Small
    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        10.0f,
        L"en-us",
        &m_pFormatSmall
    );

    // 8. Tiny
    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_MEDIUM,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        9.0f,
        L"en-us",
        &m_pFormatTiny
    );

    return true;
}

void D2DContext::Cleanup() {
    if (m_pFormatClock) { m_pFormatClock->Release(); m_pFormatClock = nullptr; }
    if (m_pFormatClockSmall) { m_pFormatClockSmall->Release(); m_pFormatClockSmall = nullptr; }
    if (m_pFormatTitle) { m_pFormatTitle->Release(); m_pFormatTitle = nullptr; }
    if (m_pFormatSubtitle) { m_pFormatSubtitle->Release(); m_pFormatSubtitle = nullptr; }
    if (m_pFormatBody) { m_pFormatBody->Release(); m_pFormatBody = nullptr; }
    if (m_pFormatBodyBold) { m_pFormatBodyBold->Release(); m_pFormatBodyBold = nullptr; }
    if (m_pFormatSmall) { m_pFormatSmall->Release(); m_pFormatSmall = nullptr; }
    if (m_pFormatTiny) { m_pFormatTiny->Release(); m_pFormatTiny = nullptr; }
    if (m_pDWriteFactory) { m_pDWriteFactory->Release(); m_pDWriteFactory = nullptr; }
    if (m_pD2DFactory) { m_pD2DFactory->Release(); m_pD2DFactory = nullptr; }
}

void D2DContext::DrawGlassPanel(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, float radius, float alpha) {
    D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(rect, radius, radius);

    // Outer dark translucent glass background
    ID2D1SolidColorBrush* pBgBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.09f, 0.13f, alpha), &pBgBrush);
    if (pBgBrush) {
        rt->FillRoundedRectangle(roundedRect, pBgBrush);
        pBgBrush->Release();
    }

    // Outer subtle border glow
    ID2D1SolidColorBrush* pBorderBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.09f), &pBorderBrush);
    if (pBorderBrush) {
        rt->DrawRoundedRectangle(roundedRect, pBorderBrush, 1.0f);
        pBorderBrush->Release();
    }

    // Top highlight reflection line
    ID2D1SolidColorBrush* pHighlightBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f), &pHighlightBrush);
    if (pHighlightBrush) {
        rt->DrawLine(
            D2D1::Point2F(rect.left + radius, rect.top + 1.0f),
            D2D1::Point2F(rect.right - radius, rect.top + 1.0f),
            pHighlightBrush,
            1.0f
        );
        pHighlightBrush->Release();
    }

    // Bottom inner shadow gradient for depth
    ID2D1SolidColorBrush* pShadowBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.06f), &pShadowBrush);
    if (pShadowBrush) {
        rt->DrawLine(
            D2D1::Point2F(rect.left + radius, rect.bottom - 1.0f),
            D2D1::Point2F(rect.right - radius, rect.bottom - 1.0f),
            pShadowBrush,
            1.0f
        );
        pShadowBrush->Release();
    }
}

void D2DContext::DrawInnerCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, float radius, bool hovered) {
    D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(rect, radius, radius);

    float bgAlpha = hovered ? 0.35f : 0.20f;
    ID2D1SolidColorBrush* pBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.24f, 0.35f, bgAlpha), &pBrush);
    if (pBrush) {
        rt->FillRoundedRectangle(roundedRect, pBrush);
        pBrush->Release();
    }

    float borderAlpha = hovered ? 0.22f : 0.08f;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, borderAlpha), &pBrush);
    if (pBrush) {
        rt->DrawRoundedRectangle(roundedRect, pBrush, 1.0f);
        pBrush->Release();
    }
}

void D2DContext::DrawProgressBar(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, float percent, D2D1_COLOR_F barColor, D2D1_COLOR_F bgColor) {
    float radius = (rect.bottom - rect.top) / 2.0f;
    D2D1_ROUNDED_RECT bgRounded = D2D1::RoundedRect(rect, radius, radius);

    ID2D1SolidColorBrush* pBgBrush = nullptr;
    rt->CreateSolidColorBrush(bgColor, &pBgBrush);
    if (pBgBrush) {
        rt->FillRoundedRectangle(bgRounded, pBgBrush);
        pBgBrush->Release();
    }

    if (percent > 0.5f) {
        float width = (rect.right - rect.left) * (percent / 100.0f);
        if (width < radius * 2.0f) width = radius * 2.0f;
        if (width > (rect.right - rect.left)) width = (rect.right - rect.left);

        D2D1_RECT_F fillRect = D2D1::RectF(rect.left, rect.top, rect.left + width, rect.bottom);
        D2D1_ROUNDED_RECT fillRounded = D2D1::RoundedRect(fillRect, radius, radius);

        ID2D1SolidColorBrush* pFillBrush = nullptr;
        rt->CreateSolidColorBrush(barColor, &pFillBrush);
        if (pFillBrush) {
            rt->FillRoundedRectangle(fillRounded, pFillBrush);
            pFillBrush->Release();
        }
    }
}

void D2DContext::DrawCheckbox(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, bool checked, bool hovered) {
    float cx = (rect.left + rect.right) / 2.0f;
    float cy = (rect.top + rect.bottom) / 2.0f;
    float r = (rect.right - rect.left) / 2.0f - 1.0f;

    D2D1_ELLIPSE circle = D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r);

    ID2D1SolidColorBrush* pBrush = nullptr;
    if (checked) {
        // Emerald / Teal checked fill
        rt->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.78f, 0.55f, 1.0f), &pBrush);
        if (pBrush) {
            rt->FillEllipse(circle, pBrush);
            pBrush->Release();
        }

        // White checkmark tick
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &pBrush);
        if (pBrush) {
            rt->DrawLine(D2D1::Point2F(cx - 4.5f, cy), D2D1::Point2F(cx - 1.5f, cy + 3.5f), pBrush, 2.0f);
            rt->DrawLine(D2D1::Point2F(cx - 1.5f, cy + 3.5f), D2D1::Point2F(cx + 4.5f, cy - 3.5f), pBrush, 2.0f);
            pBrush->Release();
        }
    } else {
        // Unchecked ring
        float alpha = hovered ? 0.9f : 0.45f;
        rt->CreateSolidColorBrush(D2D1::ColorF(0.7f, 0.75f, 0.85f, alpha), &pBrush);
        if (pBrush) {
            rt->DrawEllipse(circle, pBrush, 1.8f);
            pBrush->Release();
        }
    }
}

void D2DContext::DrawCloseButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, bool hovered) {
    float cx = (rect.left + rect.right) / 2.0f;
    float cy = (rect.top + rect.bottom) / 2.0f;
    float r = (rect.right - rect.left) / 2.0f;

    ID2D1SolidColorBrush* pBrush = nullptr;
    if (hovered) {
        rt->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.2f, 0.25f, 0.45f), &pBrush);
        if (pBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), pBrush);
            pBrush->Release();
        }
    }

    D2D1_COLOR_F color = hovered ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f) : D2D1::ColorF(0.6f, 0.65f, 0.75f, 0.65f);
    rt->CreateSolidColorBrush(color, &pBrush);
    if (pBrush) {
        float offset = 3.5f;
        rt->DrawLine(D2D1::Point2F(cx - offset, cy - offset), D2D1::Point2F(cx + offset, cy + offset), pBrush, 1.6f);
        rt->DrawLine(D2D1::Point2F(cx + offset, cy - offset), D2D1::Point2F(cx - offset, cy + offset), pBrush, 1.6f);
        pBrush->Release();
    }
}

void D2DContext::DrawPinButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, bool pinned, bool hovered) {
    float cx = (rect.left + rect.right) / 2.0f;
    float cy = (rect.top + rect.bottom) / 2.0f;
    float r = (rect.right - rect.left) / 2.0f;

    ID2D1SolidColorBrush* pBrush = nullptr;
    if (hovered || pinned) {
        float alpha = pinned ? 0.35f : 0.20f;
        D2D1_COLOR_F c = pinned ? D2D1::ColorF(0.22f, 0.75f, 0.98f, alpha) : D2D1::ColorF(0.4f, 0.45f, 0.55f, alpha);
        rt->CreateSolidColorBrush(c, &pBrush);
        if (pBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), pBrush);
            pBrush->Release();
        }
    }

    D2D1_COLOR_F color = pinned ? D2D1::ColorF(0.22f, 0.75f, 0.98f, 1.0f) :
                         (hovered ? D2D1::ColorF(0.9f, 0.92f, 0.96f, 0.9f) : D2D1::ColorF(0.6f, 0.65f, 0.75f, 0.65f));
    rt->CreateSolidColorBrush(color, &pBrush);
    if (pBrush) {
        // Draw pin icon (pin head, needle)
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy - 2.5f), 3.0f, 3.0f), pBrush);
        rt->DrawLine(D2D1::Point2F(cx, cy - 0.5f), D2D1::Point2F(cx, cy + 4.5f), pBrush, 1.5f);
        pBrush->Release();
    }
}

void D2DContext::DrawNavArrow(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, bool isRight, bool hovered) {
    float cx = (rect.left + rect.right) / 2.0f;
    float cy = (rect.top + rect.bottom) / 2.0f;
    float r = (rect.right - rect.left) / 2.0f;

    ID2D1SolidColorBrush* pBrush = nullptr;
    if (hovered) {
        rt->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.35f, 0.45f, 0.35f), &pBrush);
        if (pBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), pBrush);
            pBrush->Release();
        }
    }

    D2D1_COLOR_F color = hovered ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f) : D2D1::ColorF(0.65f, 0.7f, 0.8f, 0.7f);
    rt->CreateSolidColorBrush(color, &pBrush);
    if (pBrush) {
        float dir = isRight ? 1.0f : -1.0f;
        rt->DrawLine(D2D1::Point2F(cx - 2.5f * dir, cy - 4.0f), D2D1::Point2F(cx + 2.5f * dir, cy), pBrush, 1.6f);
        rt->DrawLine(D2D1::Point2F(cx + 2.5f * dir, cy), D2D1::Point2F(cx - 2.5f * dir, cy + 4.0f), pBrush, 1.6f);
        pBrush->Release();
    }
}

void D2DContext::DrawWeatherVector(ID2D1RenderTarget* rt, int weatherCode, bool isDay, const D2D1_RECT_F& rect) {
    float cx = (rect.left + rect.right) / 2.0f;
    float cy = (rect.top + rect.bottom) / 2.0f;

    ID2D1SolidColorBrush* pSunBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.78f, 0.22f, 1.0f), &pSunBrush);

    ID2D1SolidColorBrush* pCloudBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.88f, 0.94f, 0.9f), &pCloudBrush);

    ID2D1SolidColorBrush* pRainBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.25f, 0.65f, 0.95f, 0.9f), &pRainBrush);

    ID2D1SolidColorBrush* pLightningBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.85f, 0.15f, 1.0f), &pLightningBrush);

    ID2D1SolidColorBrush* pSnowBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.88f, 1.0f, 0.95f), &pSnowBrush);

    // Weather code grouping:
    // 0 = Clear
    // 1-3 = Partly cloudy / overcast
    // 45, 48 = Fog
    // 51-67, 80-82 = Rain / Drizzle
    // 71-77, 85-86 = Snow
    // 95-99 = Thunderstorm

    if (weatherCode == 0) {
        // Glowing Sun or Moon
        if (isDay) {
            if (pSunBrush) {
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 13.0f, 13.0f), pSunBrush);
                // Sun rays
                for (int i = 0; i < 8; i++) {
                    float angle = (float)(i * (2.0 * M_PI / 8.0));
                    float r1 = 16.0f;
                    float r2 = 21.0f;
                    rt->DrawLine(
                        D2D1::Point2F(cx + r1 * cosf(angle), cy + r1 * sinf(angle)),
                        D2D1::Point2F(cx + r2 * cosf(angle), cy + r2 * sinf(angle)),
                        pSunBrush, 2.0f
                    );
                }
            }
        } else {
            // Crescent Moon
            if (pSunBrush) {
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 14.0f, 14.0f), pSunBrush);
                ID2D1SolidColorBrush* pMask = nullptr;
                rt->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.09f, 0.13f, 1.0f), &pMask);
                if (pMask) {
                    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 6.0f, cy - 3.0f), 12.0f, 12.0f), pMask);
                    pMask->Release();
                }
            }
        }
    } else if (weatherCode <= 3) {
        // Partly Cloudy: Sun behind Cloud
        if (pSunBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 8.0f, cy - 7.0f), 9.0f, 9.0f), pSunBrush);
        }
        if (pCloudBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 4.0f, cy + 3.0f), 10.0f, 10.0f), pCloudBrush);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 6.0f, cy + 1.0f), 13.0f, 13.0f), pCloudBrush);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 15.0f, cy + 5.0f), 8.0f, 8.0f), pCloudBrush);
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - 10.0f, cy + 5.0f, cx + 22.0f, cy + 14.0f), 4.0f, 4.0f), pCloudBrush);
        }
    } else if (weatherCode >= 95) {
        // Thunderstorm: Cloud + Lightning
        if (pCloudBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 7.0f, cy - 4.0f), 10.0f, 10.0f), pCloudBrush);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 3.0f, cy - 7.0f), 13.0f, 13.0f), pCloudBrush);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 12.0f, cy - 3.0f), 8.0f, 8.0f), pCloudBrush);
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - 12.0f, cy - 2.0f, cx + 18.0f, cy + 7.0f), 4.0f, 4.0f), pCloudBrush);
        }
        if (pLightningBrush) {
            rt->DrawLine(D2D1::Point2F(cx + 2.0f, cy + 8.0f), D2D1::Point2F(cx - 3.0f, cy + 15.0f), pLightningBrush, 2.2f);
            rt->DrawLine(D2D1::Point2F(cx - 3.0f, cy + 15.0f), D2D1::Point2F(cx + 1.0f, cy + 15.0f), pLightningBrush, 2.2f);
            rt->DrawLine(D2D1::Point2F(cx + 1.0f, cy + 15.0f), D2D1::Point2F(cx - 4.0f, cy + 22.0f), pLightningBrush, 2.2f);
        }
    } else if (weatherCode >= 71 && weatherCode <= 86) {
        // Snow: Cloud + Snowflakes
        if (pCloudBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 7.0f, cy - 4.0f), 10.0f, 10.0f), pCloudBrush);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 3.0f, cy - 7.0f), 13.0f, 13.0f), pCloudBrush);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 12.0f, cy - 3.0f), 8.0f, 8.0f), pCloudBrush);
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - 12.0f, cy - 2.0f, cx + 18.0f, cy + 7.0f), 4.0f, 4.0f), pCloudBrush);
        }
        if (pSnowBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 6.0f, cy + 13.0f), 2.2f, 2.2f), pSnowBrush);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 4.0f, cy + 15.0f), 2.2f, 2.2f), pSnowBrush);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 12.0f, cy + 13.0f), 2.2f, 2.2f), pSnowBrush);
        }
    } else {
        // Rain / Drizzle / Fog
        if (pCloudBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 7.0f, cy - 4.0f), 10.0f, 10.0f), pCloudBrush);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 3.0f, cy - 7.0f), 13.0f, 13.0f), pCloudBrush);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 12.0f, cy - 3.0f), 8.0f, 8.0f), pCloudBrush);
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - 12.0f, cy - 2.0f, cx + 18.0f, cy + 7.0f), 4.0f, 4.0f), pCloudBrush);
        }
        if (pRainBrush) {
            rt->DrawLine(D2D1::Point2F(cx - 6.0f, cy + 10.0f), D2D1::Point2F(cx - 8.0f, cy + 17.0f), pRainBrush, 2.0f);
            rt->DrawLine(D2D1::Point2F(cx + 1.0f, cy + 11.0f), D2D1::Point2F(cx - 1.0f, cy + 18.0f), pRainBrush, 2.0f);
            rt->DrawLine(D2D1::Point2F(cx + 8.0f, cy + 10.0f), D2D1::Point2F(cx + 6.0f, cy + 17.0f), pRainBrush, 2.0f);
        }
    }

    if (pSunBrush) pSunBrush->Release();
    if (pCloudBrush) pCloudBrush->Release();
    if (pRainBrush) pRainBrush->Release();
    if (pLightningBrush) pLightningBrush->Release();
    if (pSnowBrush) pSnowBrush->Release();
}

void D2DContext::DrawBatteryVector(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, int percent, bool isCharging, bool isAC) {
    float width = rect.right - rect.left;
    float height = rect.bottom - rect.top;

    D2D1_RECT_F bodyRect = D2D1::RectF(rect.left, rect.top + 2.0f, rect.right - 4.0f, rect.bottom - 2.0f);
    D2D1_ROUNDED_RECT bodyRounded = D2D1::RoundedRect(bodyRect, 3.0f, 3.0f);

    ID2D1SolidColorBrush* pBorderBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.7f, 0.75f, 0.85f, 0.8f), &pBorderBrush);
    if (pBorderBrush) {
        rt->DrawRoundedRectangle(bodyRounded, pBorderBrush, 1.5f);

        // Terminal nipple on the right
        D2D1_RECT_F termRect = D2D1::RectF(rect.right - 3.0f, rect.top + height * 0.3f, rect.right, rect.bottom - height * 0.3f);
        rt->FillRoundedRectangle(D2D1::RoundedRect(termRect, 1.5f, 1.5f), pBorderBrush);
        pBorderBrush->Release();
    }

    // Interior Fill
    float fillWidth = (bodyRect.right - bodyRect.left - 4.0f) * (percent / 100.0f);
    if (fillWidth < 2.0f) fillWidth = 2.0f;
    D2D1_RECT_F fillRect = D2D1::RectF(bodyRect.left + 2.0f, bodyRect.top + 2.0f, bodyRect.left + 2.0f + fillWidth, bodyRect.bottom - 2.0f);

    D2D1_COLOR_F fillColor = (percent > 40) ? D2D1::ColorF(0.2f, 0.8f, 0.5f, 0.9f) :
                             ((percent > 15) ? D2D1::ColorF(0.95f, 0.75f, 0.2f, 0.9f) : D2D1::ColorF(0.95f, 0.3f, 0.3f, 0.9f));

    ID2D1SolidColorBrush* pFillBrush = nullptr;
    rt->CreateSolidColorBrush(fillColor, &pFillBrush);
    if (pFillBrush) {
        rt->FillRoundedRectangle(D2D1::RoundedRect(fillRect, 2.0f, 2.0f), pFillBrush);
        pFillBrush->Release();
    }

    // Charging indicator (lightning bolt in the center)
    if (isCharging || isAC) {
        float cx = (bodyRect.left + bodyRect.right) / 2.0f;
        float cy = (bodyRect.top + bodyRect.bottom) / 2.0f;

        ID2D1SolidColorBrush* pBolt = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &pBolt);
        if (pBolt) {
            rt->DrawLine(D2D1::Point2F(cx + 2.0f, cy - 4.0f), D2D1::Point2F(cx - 2.0f, cy), pBolt, 1.6f);
            rt->DrawLine(D2D1::Point2F(cx - 2.0f, cy), D2D1::Point2F(cx + 1.0f, cy), pBolt, 1.6f);
            rt->DrawLine(D2D1::Point2F(cx + 1.0f, cy), D2D1::Point2F(cx - 2.0f, cy + 4.0f), pBolt, 1.6f);
            pBolt->Release();
        }
    }
}
void D2DContext::DrawResizeGrip(ID2D1RenderTarget* rt, float x, float y, float alpha) {
    ID2D1SolidColorBrush* pBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha), &pBrush);
    if (pBrush) {
        // Three diagonal dot-lines (bottom-right resize grip)
        // Line 1 (closest to corner)
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x - 3.0f, y - 3.0f), 1.3f, 1.3f), pBrush);
        
        // Line 2
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x - 9.0f, y - 3.0f), 1.3f, 1.3f), pBrush);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x - 3.0f, y - 9.0f), 1.3f, 1.3f), pBrush);
        
        // Line 3
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x - 15.0f, y - 3.0f), 1.3f, 1.3f), pBrush);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x - 9.0f, y - 9.0f), 1.3f, 1.3f), pBrush);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x - 3.0f, y - 15.0f), 1.3f, 1.3f), pBrush);
        
        pBrush->Release();
    }
}
