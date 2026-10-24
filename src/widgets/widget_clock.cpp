#include "widget_clock.h"
#include "../graphics/d2d_context.h"
#include <ctime>
#include <algorithm>
#include <cmath>

WidgetClock::WidgetClock()
    : WidgetBase(L"clock", L"Clock & Date", 340, 130) {
    SetMinMaxSize(200, 80, 800, 600);
    m_showWindowHeader = false; // Pure modern headerless mobile widget
    OnUpdate();
}

WidgetClock::~WidgetClock() {
    if (m_pDynamicClockFormat) {
        m_pDynamicClockFormat->Release();
        m_pDynamicClockFormat = nullptr;
    }
}

void WidgetClock::CycleTheme() {
    m_theme = static_cast<ClockTheme>((static_cast<int>(m_theme) + 1) % THEME_COUNT);
    Redraw();
}

IDWriteTextFormat* WidgetClock::GetOrCreateClockFormat(float size, DWRITE_FONT_WEIGHT weight) {
    if (m_pDynamicClockFormat && std::abs(m_cachedFontSize - size) < 0.5f) {
        return m_pDynamicClockFormat;
    }

    if (m_pDynamicClockFormat) {
        m_pDynamicClockFormat->Release();
        m_pDynamicClockFormat = nullptr;
    }

    IDWriteFactory* dwrite = D2DContext::Get().GetDWriteFactory();
    if (!dwrite) return nullptr;

    HRESULT hr = dwrite->CreateTextFormat(
        L"Segoe UI Variable Display",
        NULL,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"en-us",
        &m_pDynamicClockFormat
    );

    if (FAILED(hr) || !m_pDynamicClockFormat) {
        dwrite->CreateTextFormat(
            L"Segoe UI",
            NULL,
            weight,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size,
            L"en-us",
            &m_pDynamicClockFormat
        );
    }

    m_cachedFontSize = size;
    return m_pDynamicClockFormat;
}

void WidgetClock::OnUpdate() {
    time_t now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);

    int hour = localTime.tm_hour;
    int min = localTime.tm_min;
    int sec = localTime.tm_sec;

    // Greeting and icon based on hour
    if (hour >= 5 && hour < 12) {
        m_greetingString = L"Good Morning";
        m_greetingIcon = L"\u2600"; // ☀ Sun
    } else if (hour >= 12 && hour < 17) {
        m_greetingString = L"Good Afternoon";
        m_greetingIcon = L"\u2600"; // ☀ Sun
    } else if (hour >= 17 && hour < 22) {
        m_greetingString = L"Good Evening";
        m_greetingIcon = L"\u2728"; // ✨ Sparkles
    } else {
        m_greetingString = L"Good Night";
        m_greetingIcon = L"\u263E"; // ☾ Moon
    }

    // Time string
    wchar_t timeBuf[32];
    wchar_t secBuf[16];
    wchar_t ampmBuf[16] = L"";

    if (m_is24Hour) {
        swprintf_s(timeBuf, L"%02d:%02d", hour, min);
        swprintf_s(secBuf, L":%02d", sec);
    } else {
        int displayHour = hour % 12;
        if (displayHour == 0) displayHour = 12;
        swprintf_s(timeBuf, L"%02d:%02d", displayHour, min);
        swprintf_s(secBuf, L":%02d", sec);
        swprintf_s(ampmBuf, L"%s", (hour >= 12) ? L"PM" : L"AM");
    }

    m_timeString = timeBuf;
    m_secString = secBuf;
    m_ampmString = ampmBuf;

    // Date strings
    static const wchar_t* days[] = { L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday" };
    static const wchar_t* shortDays[] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
    static const wchar_t* months[] = { L"January", L"February", L"March", L"April", L"May", L"June",
                                       L"July", L"August", L"September", L"October", L"November", L"December" };
    static const wchar_t* shortMonths[] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
                                            L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };

    m_dayString = days[localTime.tm_wday];

    wchar_t fullDateBuf[128];
    swprintf_s(fullDateBuf, L"%s, %s %d, %d",
        days[localTime.tm_wday],
        months[localTime.tm_mon],
        localTime.tm_mday,
        localTime.tm_year + 1900);
    m_dateString = fullDateBuf;

    wchar_t shortDateBuf[64];
    swprintf_s(shortDateBuf, L"%s, %s %d",
        shortDays[localTime.tm_wday],
        shortMonths[localTime.tm_mon],
        localTime.tm_mday);
    m_shortDateString = shortDateBuf;

    // Day progress percent (0.0 to 100.0)
    float secondsInDay = (float)(hour * 3600 + min * 60 + sec);
    m_dayProgressPercent = (secondsInDay / 86400.0f) * 100.0f;

    Redraw();
}

bool WidgetClock::OnMouseDown(int x, int y, bool isRight) {
    if (!isRight) {
        // Toggle 12h/24h on click
        m_is24Hour = !m_is24Hour;
        OnUpdate();
        return true;
    }
    return false;
}

void WidgetClock::OnDoubleClick(int x, int y) {
    // Instant theme switch on double click!
    CycleTheme();
}

void WidgetClock::OnBuildContextMenu(HMENU hMenu) {
    // 1. Submenu: Theme Picker
    HMENU hThemeMenu = CreatePopupMenu();
    InsertMenuW(hThemeMenu, 0, MF_BYPOSITION | (m_theme == THEME_FROST_GLASS ? MF_CHECKED : MF_UNCHECKED), 401, L"Frosted Glass (Windows 11 Acrylic)");
    InsertMenuW(hThemeMenu, 1, MF_BYPOSITION | (m_theme == THEME_MIDNIGHT_OLED ? MF_CHECKED : MF_UNCHECKED), 402, L"Midnight OLED (Minimalist Dark)");
    InsertMenuW(hThemeMenu, 2, MF_BYPOSITION | (m_theme == THEME_SUNSET_HORIZON ? MF_CHECKED : MF_UNCHECKED), 403, L"Sunset Horizon (Warm Glow)");
    InsertMenuW(hThemeMenu, 3, MF_BYPOSITION | (m_theme == THEME_AURORA_EMERALD ? MF_CHECKED : MF_UNCHECKED), 404, L"Aurora Emerald (Calm Mint)");
    InsertMenuW(hThemeMenu, 4, MF_BYPOSITION | (m_theme == THEME_FLOATING_GHOST ? MF_CHECKED : MF_UNCHECKED), 405, L"Floating Ghost (Transparent)");
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hThemeMenu, L"Widget Theme (or Double-Click)");

    // 2. Submenu: Clock Options
    HMENU hClockMenu = CreatePopupMenu();
    InsertMenuW(hClockMenu, 0, MF_BYPOSITION | (m_is24Hour ? MF_CHECKED : MF_UNCHECKED), 501, L"24-Hour Format");
    InsertMenuW(hClockMenu, 1, MF_BYPOSITION | (m_showSeconds ? MF_CHECKED : MF_UNCHECKED), 502, L"Show Seconds");
    InsertMenuW(hClockMenu, 2, MF_BYPOSITION | (m_showGreeting ? MF_CHECKED : MF_UNCHECKED), 503, L"Show Greeting Capsule");
    InsertMenuW(hClockMenu, 3, MF_BYPOSITION | (m_showProgress ? MF_CHECKED : MF_UNCHECKED), 504, L"Show Day Progress Bar");
    InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hClockMenu, L"Clock Options");
}

bool WidgetClock::OnMenuCommand(int cmd) {
    switch (cmd) {
        case 401: m_theme = THEME_FROST_GLASS; Redraw(); return true;
        case 402: m_theme = THEME_MIDNIGHT_OLED; Redraw(); return true;
        case 403: m_theme = THEME_SUNSET_HORIZON; Redraw(); return true;
        case 404: m_theme = THEME_AURORA_EMERALD; Redraw(); return true;
        case 405: m_theme = THEME_FLOATING_GHOST; Redraw(); return true;

        case 501: m_is24Hour = !m_is24Hour; OnUpdate(); return true;
        case 502: m_showSeconds = !m_showSeconds; Redraw(); return true;
        case 503: m_showGreeting = !m_showGreeting; Redraw(); return true;
        case 504: m_showProgress = !m_showProgress; Redraw(); return true;
    }
    return false;
}

void WidgetClock::DrawBackgroundCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& cardRect) {
    if (m_theme == THEME_FLOATING_GHOST) {
        // Floating Ghost has no card background (clean floating text on desktop)
        return;
    }

    float radius = 22.0f;
    D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(cardRect, radius, radius);

    D2D1_COLOR_F bgColor;
    D2D1_COLOR_F borderColor;
    D2D1_COLOR_F glowColor;

    switch (m_theme) {
        case THEME_MIDNIGHT_OLED:
            bgColor = D2D1::ColorF(0.04f, 0.04f, 0.06f, 0.90f * m_opacity);
            borderColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.09f);
            glowColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.03f);
            break;
        case THEME_SUNSET_HORIZON:
            bgColor = D2D1::ColorF(0.12f, 0.07f, 0.14f, 0.82f * m_opacity);
            borderColor = D2D1::ColorF(0.98f, 0.57f, 0.24f, 0.22f);
            glowColor = D2D1::ColorF(0.96f, 0.25f, 0.37f, 0.10f);
            break;
        case THEME_AURORA_EMERALD:
            bgColor = D2D1::ColorF(0.05f, 0.11f, 0.09f, 0.82f * m_opacity);
            borderColor = D2D1::ColorF(0.20f, 0.83f, 0.60f, 0.22f);
            glowColor = D2D1::ColorF(0.20f, 0.83f, 0.60f, 0.08f);
            break;
        case THEME_FROST_GLASS:
        default:
            bgColor = D2D1::ColorF(0.06f, 0.08f, 0.14f, 0.78f * m_opacity);
            borderColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f);
            glowColor = D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.08f);
            break;
    }

    // 1. Soft Ambient Radial Glow
    ID2D1SolidColorBrush* pGlowBrush = nullptr;
    rt->CreateSolidColorBrush(glowColor, &pGlowBrush);
    if (pGlowBrush) {
        float cx = (cardRect.left + cardRect.right) * 0.40f;
        float cy = (cardRect.top + cardRect.bottom) * 0.50f;
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), cardRect.right * 0.45f, cardRect.bottom * 0.45f), pGlowBrush);
        pGlowBrush->Release();
    }

    // 2. Base Translucent Glass Surface
    ID2D1SolidColorBrush* pBgBrush = nullptr;
    rt->CreateSolidColorBrush(bgColor, &pBgBrush);
    if (pBgBrush) {
        rt->FillRoundedRectangle(rounded, pBgBrush);
        pBgBrush->Release();
    }

    // 3. Specular Border Glow
    ID2D1SolidColorBrush* pBorderBrush = nullptr;
    rt->CreateSolidColorBrush(borderColor, &pBorderBrush);
    if (pBorderBrush) {
        rt->DrawRoundedRectangle(rounded, pBorderBrush, 1.0f);
        pBorderBrush->Release();
    }

    // 4. Subtle Top Light Reflection Line
    ID2D1SolidColorBrush* pHighlightBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f), &pHighlightBrush);
    if (pHighlightBrush) {
        rt->DrawLine(
            D2D1::Point2F(cardRect.left + radius, cardRect.top + 1.0f),
            D2D1::Point2F(cardRect.right - radius, cardRect.top + 1.0f),
            pHighlightBrush,
            1.0f
        );
        pHighlightBrush->Release();
    }
}

void WidgetClock::OnRenderContent(ID2D1RenderTarget* rt) {
    D2DContext& ctx = D2DContext::Get();

    float w = (float)m_width;
    float h = (float)m_height;

    // Draw the theme card background
    DrawBackgroundCard(rt, D2D1::RectF(0.0f, 0.0f, w, h));

    // Theme Accent & Typography Colors
    D2D1_COLOR_F accentColor;
    D2D1_COLOR_F whiteColor = D2D1::ColorF(0.98f, 0.98f, 1.0f, 0.98f);
    D2D1_COLOR_F silverColor = D2D1::ColorF(0.72f, 0.77f, 0.88f, 0.85f);
    D2D1_COLOR_F capsuleBgColor;

    switch (m_theme) {
        case THEME_MIDNIGHT_OLED:
            accentColor = D2D1::ColorF(0.95f, 0.96f, 1.0f, 0.95f);
            capsuleBgColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f);
            break;
        case THEME_SUNSET_HORIZON:
            accentColor = D2D1::ColorF(0.98f, 0.44f, 0.52f, 0.95f); // Coral
            capsuleBgColor = D2D1::ColorF(0.98f, 0.57f, 0.24f, 0.14f);
            break;
        case THEME_AURORA_EMERALD:
            accentColor = D2D1::ColorF(0.20f, 0.83f, 0.60f, 0.95f); // Mint
            capsuleBgColor = D2D1::ColorF(0.20f, 0.83f, 0.60f, 0.14f);
            break;
        case THEME_FLOATING_GHOST:
            accentColor = D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.98f);
            capsuleBgColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.30f);
            break;
        case THEME_FROST_GLASS:
        default:
            accentColor = D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.95f); // Sky Cyan
            capsuleBgColor = D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.14f);
            break;
    }

    ID2D1SolidColorBrush* pWhite = nullptr;
    rt->CreateSolidColorBrush(whiteColor, &pWhite);

    ID2D1SolidColorBrush* pAccent = nullptr;
    rt->CreateSolidColorBrush(accentColor, &pAccent);

    ID2D1SolidColorBrush* pSilver = nullptr;
    rt->CreateSolidColorBrush(silverColor, &pSilver);

    ID2D1SolidColorBrush* pCapsuleBg = nullptr;
    rt->CreateSolidColorBrush(capsuleBgColor, &pCapsuleBg);

    // Responsive Mobile Layout Selection
    bool isWide = (w >= 310.0f && (w / h) >= 1.95f);
    bool isSquare = (w >= 180.0f && h >= 170.0f && (w / h) <= 1.35f);
    bool isCompact = (h < 105.0f);

    if (isWide) {
        // ========================================================
        // LAYOUT 1: WIDE BANNER (4x1 style e.g. 380x110)
        // Dual pane: Clock on Left, Greeting & Date on Right
        // ========================================================
        float midX = w * 0.48f;
        float clockSize = std::clamp(h * 0.52f, 28.0f, 64.0f);
        IDWriteTextFormat* clockFmt = GetOrCreateClockFormat(clockSize, DWRITE_FONT_WEIGHT_SEMI_BOLD);

        // Digital Clock
        float clockTop = (h - clockSize) * 0.5f - 2.0f;
        D2D1_RECT_F clockRect = D2D1::RectF(20.0f, clockTop, midX - 6.0f, clockTop + clockSize + 6.0f);
        if (pWhite && clockFmt) {
            rt->DrawText(m_timeString.c_str(), (UINT32)m_timeString.length(), clockFmt, clockRect, pWhite);
        }

        // AM/PM Capsule & Seconds
        float badgesX = 20.0f + (clockSize * 1.58f);
        if (badgesX < midX - 25.0f) {
            if (!m_ampmString.empty()) {
                D2D1_RECT_F ampmPill = D2D1::RectF(badgesX, clockTop + 4.0f, badgesX + 32.0f, clockTop + 22.0f);
                if (pCapsuleBg) {
                    rt->FillRoundedRectangle(D2D1::RoundedRect(ampmPill, 4.0f, 4.0f), pCapsuleBg);
                }
                D2D1_RECT_F ampmText = D2D1::RectF(ampmPill.left + 5.0f, ampmPill.top + 2.0f, ampmPill.right, ampmPill.bottom);
                if (pAccent) {
                    rt->DrawText(m_ampmString.c_str(), (UINT32)m_ampmString.length(), ctx.GetFormatTiny(), ampmText, pAccent);
                }
            }
            if (m_showSeconds) {
                float secTop = m_ampmString.empty() ? (clockTop + 6.0f) : (clockTop + 24.0f);
                D2D1_RECT_F secRect = D2D1::RectF(badgesX, secTop, midX, secTop + 18.0f);
                if (pSilver) {
                    rt->DrawText(m_secString.c_str(), (UINT32)m_secString.length(), ctx.GetFormatSmall(), secRect, pSilver);
                }
            }
        }

        // Elegant Vertical Divider
        ID2D1SolidColorBrush* pDiv = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f), &pDiv);
        if (pDiv) {
            rt->DrawLine(D2D1::Point2F(midX, 16.0f), D2D1::Point2F(midX, h - 16.0f), pDiv, 1.0f);
            pDiv->Release();
        }

        // Right Pane Content
        float rLeft = midX + 18.0f;
        float rWidth = w - rLeft - 20.0f;
        float curY = 16.0f;

        if (m_showGreeting) {
            std::wstring greetText = m_greetingIcon + L" " + m_greetingString;
            D2D1_RECT_F greetRect = D2D1::RectF(rLeft, curY, rLeft + rWidth, curY + 18.0f);
            if (pAccent) {
                rt->DrawText(greetText.c_str(), (UINT32)greetText.length(), ctx.GetFormatSmall(), greetRect, pAccent);
            }
            curY += 20.0f;
        }

        // Bold Day of Week
        D2D1_RECT_F dayRect = D2D1::RectF(rLeft, curY, rLeft + rWidth, curY + 22.0f);
        if (pWhite) {
            rt->DrawText(m_dayString.c_str(), (UINT32)m_dayString.length(), ctx.GetFormatTitle(), dayRect, pWhite);
        }
        curY += 20.0f;

        // Full Date
        D2D1_RECT_F dateRect = D2D1::RectF(rLeft, curY, rLeft + rWidth, curY + 18.0f);
        if (pSilver) {
            rt->DrawText(m_shortDateString.c_str(), (UINT32)m_shortDateString.length(), ctx.GetFormatBody(), dateRect, pSilver);
        }

        // Day Progress Bar
        if (m_showProgress && (h - curY >= 20.0f)) {
            float barY = h - 14.0f;
            D2DContext::DrawProgressBar(rt, D2D1::RectF(rLeft, barY, w - 24.0f, barY + 4.0f),
                m_dayProgressPercent, accentColor, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
        }

    } else if (isSquare) {
        // ========================================================
        // LAYOUT 2: SQUARE HERO (2x2 style e.g. 200x200)
        // Balanced centered mobile card
        // ========================================================
        float curY = 18.0f;

        if (m_showGreeting) {
            std::wstring greetText = m_greetingIcon + L" " + m_greetingString;
            float pillW = (float)greetText.length() * 6.5f + 18.0f;
            D2D1_RECT_F pillRect = D2D1::RectF(18.0f, curY, 18.0f + pillW, curY + 20.0f);
            if (pCapsuleBg) {
                rt->FillRoundedRectangle(D2D1::RoundedRect(pillRect, 10.0f, 10.0f), pCapsuleBg);
            }
            D2D1_RECT_F tRect = D2D1::RectF(pillRect.left + 8.0f, pillRect.top + 2.0f, pillRect.right - 4.0f, pillRect.bottom);
            if (pAccent) {
                rt->DrawText(greetText.c_str(), (UINT32)greetText.length(), ctx.GetFormatTiny(), tRect, pAccent);
            }
            curY += 26.0f;
        }

        // Big Time Typography
        float clockSize = std::clamp(w * 0.26f, 32.0f, 56.0f);
        IDWriteTextFormat* clockFmt = GetOrCreateClockFormat(clockSize, DWRITE_FONT_WEIGHT_BOLD);
        float timeW = clockSize * 1.55f;
        D2D1_RECT_F clockRect = D2D1::RectF(18.0f, curY, 18.0f + timeW, curY + clockSize + 6.0f);
        if (pWhite && clockFmt) {
            rt->DrawText(m_timeString.c_str(), (UINT32)m_timeString.length(), clockFmt, clockRect, pWhite);
        }

        // AM/PM badge
        if (!m_ampmString.empty()) {
            float badgeX = 18.0f + timeW + 4.0f;
            if (badgeX < w - 24.0f) {
                D2D1_RECT_F ampmRect = D2D1::RectF(badgeX, curY + 4.0f, w - 16.0f, curY + 22.0f);
                if (pAccent) {
                    rt->DrawText(m_ampmString.c_str(), (UINT32)m_ampmString.length(), ctx.GetFormatSmall(), ampmRect, pAccent);
                }
            }
        }
        curY += clockSize + 10.0f;

        // Day of Week
        D2D1_RECT_F dayRect = D2D1::RectF(18.0f, curY, w - 18.0f, curY + 24.0f);
        if (pAccent) {
            rt->DrawText(m_dayString.c_str(), (UINT32)m_dayString.length(), ctx.GetFormatTitle(), dayRect, pAccent);
        }
        curY += 22.0f;

        // Date
        D2D1_RECT_F dateRect = D2D1::RectF(18.0f, curY, w - 18.0f, curY + 20.0f);
        if (pSilver) {
            rt->DrawText(m_dateString.c_str(), (UINT32)m_dateString.length(), ctx.GetFormatBody(), dateRect, pSilver);
        }
        curY += 22.0f;

        // Day Progress Bar
        if (m_showProgress && (h - curY >= 14.0f)) {
            float barY = h - 16.0f;
            D2DContext::DrawProgressBar(rt, D2D1::RectF(18.0f, barY, w - 18.0f, barY + 4.0f),
                m_dayProgressPercent, accentColor, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
        }

    } else if (isCompact) {
        // ========================================================
        // LAYOUT 3: COMPACT PILL (2x1 style e.g. 240x90)
        // High density modern minimalist card
        // ========================================================
        float clockSize = std::clamp(h * 0.46f, 24.0f, 44.0f);
        IDWriteTextFormat* clockFmt = GetOrCreateClockFormat(clockSize, DWRITE_FONT_WEIGHT_SEMI_BOLD);

        float clockTop = (h - clockSize) * 0.45f - 4.0f;
        float timeW = clockSize * 1.55f;
        D2D1_RECT_F clockRect = D2D1::RectF(18.0f, clockTop, 18.0f + timeW, clockTop + clockSize + 6.0f);
        if (pWhite && clockFmt) {
            rt->DrawText(m_timeString.c_str(), (UINT32)m_timeString.length(), clockFmt, clockRect, pWhite);
        }

        // AM/PM
        float badgeX = 18.0f + timeW + 4.0f;
        if (!m_ampmString.empty() && badgeX < w - 24.0f) {
            D2D1_RECT_F ampmRect = D2D1::RectF(badgeX, clockTop + 4.0f, w - 12.0f, clockTop + 22.0f);
            if (pAccent) {
                rt->DrawText(m_ampmString.c_str(), (UINT32)m_ampmString.length(), ctx.GetFormatSmall(), ampmRect, pAccent);
            }
        }

        // Single line Date below clock
        float dateY = clockTop + clockSize + 2.0f;
        D2D1_RECT_F dateRect = D2D1::RectF(18.0f, dateY, w - 16.0f, h - 6.0f);
        if (pSilver) {
            std::wstring str = (w >= 250.0f) ? (m_dayString + L" \u2022 " + m_shortDateString) : m_shortDateString;
            rt->DrawText(str.c_str(), (UINT32)str.length(), ctx.GetFormatSmall(), dateRect, pSilver);
        }

    } else {
        // ========================================================
        // LAYOUT 4: STANDARD CARD (Default e.g. 340x130)
        // The quintessential mobile desktop widget
        // ========================================================
        float curY = 16.0f;

        // Top Row: Greeting Capsule + Day Pill
        if (m_showGreeting) {
            std::wstring greetText = m_greetingIcon + L" " + m_greetingString;
            float pillW = (float)greetText.length() * 6.5f + 18.0f;
            D2D1_RECT_F pillRect = D2D1::RectF(18.0f, curY, 18.0f + pillW, curY + 20.0f);
            if (pCapsuleBg) {
                rt->FillRoundedRectangle(D2D1::RoundedRect(pillRect, 10.0f, 10.0f), pCapsuleBg);
            }
            D2D1_RECT_F tRect = D2D1::RectF(pillRect.left + 8.0f, pillRect.top + 2.0f, pillRect.right - 4.0f, pillRect.bottom);
            if (pAccent) {
                rt->DrawText(greetText.c_str(), (UINT32)greetText.length(), ctx.GetFormatTiny(), tRect, pAccent);
            }

            // Quick 12h/24h indicator pill on right
            float chipW = 54.0f;
            D2D1_RECT_F chipRect = D2D1::RectF(w - chipW - 20.0f, curY, w - 20.0f, curY + 18.0f);
            if (pCapsuleBg) {
                rt->FillRoundedRectangle(D2D1::RoundedRect(chipRect, 4.0f, 4.0f), pCapsuleBg);
            }
            D2D1_RECT_F chipText = D2D1::RectF(chipRect.left + 5.0f, chipRect.top + 1.0f, chipRect.right, chipRect.bottom);
            if (pSilver) {
                const wchar_t* mode = m_is24Hour ? L"24H" : L"12H";
                rt->DrawText(mode, (UINT32)wcslen(mode), ctx.GetFormatTiny(), chipText, pSilver);
            }

            curY += 24.0f;
        }

        // Digital Clock Typography
        float clockSize = std::clamp(h * 0.38f, 32.0f, 54.0f);
        IDWriteTextFormat* clockFmt = GetOrCreateClockFormat(clockSize, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        float timeW = clockSize * 1.55f;
        D2D1_RECT_F clockRect = D2D1::RectF(16.0f, curY, 16.0f + timeW, curY + clockSize + 6.0f);
        if (pWhite && clockFmt) {
            rt->DrawText(m_timeString.c_str(), (UINT32)m_timeString.length(), clockFmt, clockRect, pWhite);
        }

        // AM/PM & Seconds stacked or inline
        float badgeX = 16.0f + timeW + 4.0f;
        if (badgeX < w - 28.0f) {
            if (!m_ampmString.empty()) {
                D2D1_RECT_F ampmRect = D2D1::RectF(badgeX, curY + 4.0f, w - 16.0f, curY + 22.0f);
                if (pAccent) {
                    rt->DrawText(m_ampmString.c_str(), (UINT32)m_ampmString.length(), ctx.GetFormatSubtitle(), ampmRect, pAccent);
                }
            }
            if (m_showSeconds) {
                float secTop = m_ampmString.empty() ? (curY + 6.0f) : (curY + 24.0f);
                D2D1_RECT_F secRect = D2D1::RectF(badgeX, secTop, w - 16.0f, secTop + 18.0f);
                if (pSilver) {
                    rt->DrawText(m_secString.c_str(), (UINT32)m_secString.length(), ctx.GetFormatSmall(), secRect, pSilver);
                }
            }
        }
        curY += clockSize + 8.0f;

        // Full Date row
        D2D1_RECT_F dateRect = D2D1::RectF(18.0f, curY, w - 20.0f, curY + 20.0f);
        if (pSilver) {
            std::wstring fullDate = m_dayString + L", " + m_shortDateString;
            rt->DrawText(fullDate.c_str(), (UINT32)fullDate.length(), ctx.GetFormatBody(), dateRect, pSilver);
        }

        // Micro Day Progress Bar at very bottom
        if (m_showProgress && (h - curY >= 18.0f)) {
            float barY = h - 14.0f;
            D2DContext::DrawProgressBar(rt, D2D1::RectF(18.0f, barY, w - 24.0f, barY + 3.5f),
                m_dayProgressPercent, accentColor, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
        }
    }

    if (pWhite) pWhite->Release();
    if (pAccent) pAccent->Release();
    if (pSilver) pSilver->Release();
    if (pCapsuleBg) pCapsuleBg->Release();
}
