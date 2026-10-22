#include "widget_clock.h"
#include "../graphics/d2d_context.h"
#include <ctime>

WidgetClock::WidgetClock()
    : WidgetBase(L"clock", L"TIME & DATE", 280, 135) {
    OnUpdate();
}

void WidgetClock::OnUpdate() {
    time_t now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);

    int hour = localTime.tm_hour;
    int min = localTime.tm_min;
    int sec = localTime.tm_sec;

    // Greeting based on hour
    if (hour >= 5 && hour < 12) {
        m_greetingString = L"Good Morning";
    } else if (hour >= 12 && hour < 17) {
        m_greetingString = L"Good Afternoon";
    } else if (hour >= 17 && hour < 22) {
        m_greetingString = L"Good Evening";
    } else {
        m_greetingString = L"Good Night";
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

    // Date string: "Thursday, October 22, 2026"
    static const wchar_t* days[] = { L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday" };
    static const wchar_t* months[] = { L"January", L"February", L"March", L"April", L"May", L"June",
                                       L"July", L"August", L"September", L"October", L"November", L"December" };

    wchar_t dateBuf[128];
    swprintf_s(dateBuf, L"%s, %s %d, %d",
        days[localTime.tm_wday],
        months[localTime.tm_mon],
        localTime.tm_mday,
        localTime.tm_year + 1900);
    m_dateString = dateBuf;

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

void WidgetClock::OnRenderContent(ID2D1RenderTarget* rt) {
    D2DContext& ctx = D2DContext::Get();

    // 1. Dynamic Greeting Badge (Top-left below header)
    D2D1_RECT_F greetRect = D2D1::RectF(16.0f, 32.0f, (float)m_width - 16.0f, 48.0f);
    ID2D1SolidColorBrush* pGreetBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.35f, 0.75f, 0.98f, 0.90f), &pGreetBrush);
    if (pGreetBrush) {
        rt->DrawText(
            m_greetingString.c_str(),
            (UINT32)m_greetingString.length(),
            ctx.GetFormatSubtitle(),
            greetRect,
            pGreetBrush
        );
        pGreetBrush->Release();
    }

    // 2. Bold Digital Time
    D2D1_RECT_F timeRect = D2D1::RectF(14.0f, 48.0f, 150.0f, 96.0f);
    ID2D1SolidColorBrush* pTimeBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.98f), &pTimeBrush);
    if (pTimeBrush) {
        rt->DrawText(
            m_timeString.c_str(),
            (UINT32)m_timeString.length(),
            ctx.GetFormatClock(),
            timeRect,
            pTimeBrush
        );

        // Seconds & AM/PM
        D2D1_RECT_F secRect = D2D1::RectF(142.0f, 54.0f, 220.0f, 76.0f);
        ID2D1SolidColorBrush* pSecBrush = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(0.40f, 0.80f, 1.0f, 0.85f), &pSecBrush);
        if (pSecBrush) {
            rt->DrawText(
                m_secString.c_str(),
                (UINT32)m_secString.length(),
                ctx.GetFormatClockSmall(),
                secRect,
                pSecBrush
            );
            pSecBrush->Release();
        }

        if (!m_ampmString.empty()) {
            D2D1_RECT_F ampmRect = D2D1::RectF(145.0f, 74.0f, 220.0f, 94.0f);
            ID2D1SolidColorBrush* pAmpmBrush = nullptr;
            rt->CreateSolidColorBrush(D2D1::ColorF(0.65f, 0.70f, 0.85f, 0.75f), &pAmpmBrush);
            if (pAmpmBrush) {
                rt->DrawText(
                    m_ampmString.c_str(),
                    (UINT32)m_ampmString.length(),
                    ctx.GetFormatSmall(),
                    ampmRect,
                    pAmpmBrush
                );
                pAmpmBrush->Release();
            }
        }

        pTimeBrush->Release();
    }

    // 3. Full Date & Day
    D2D1_RECT_F dateRect = D2D1::RectF(16.0f, 102.0f, (float)m_width - 16.0f, 124.0f);
    ID2D1SolidColorBrush* pDateBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.75f, 0.88f, 0.80f), &pDateBrush);
    if (pDateBrush) {
        rt->DrawText(
            m_dateString.c_str(),
            (UINT32)m_dateString.length(),
            ctx.GetFormatBody(),
            dateRect,
            pDateBrush
        );
        pDateBrush->Release();
    }
}
