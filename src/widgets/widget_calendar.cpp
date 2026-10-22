#include "widget_calendar.h"
#include "../graphics/d2d_context.h"
#include <ctime>

WidgetCalendar::WidgetCalendar()
    : WidgetBase(L"calendar", L"CALENDAR", 290, 260) {
    OnUpdate();
}

void WidgetCalendar::OnUpdate() {
    time_t now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);

    m_todayYear = localTime.tm_year + 1900;
    m_todayMonth = localTime.tm_mon + 1;
    m_todayDay = localTime.tm_mday;

    if (m_viewYear == 0) {
        m_viewYear = m_todayYear;
        m_viewMonth = m_todayMonth;
        m_selectedDay = m_todayDay;
    }
}

int WidgetCalendar::GetDaysInMonth(int year, int month) const {
    if (month == 2) {
        bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return isLeap ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    }
    return 31;
}

int WidgetCalendar::GetFirstDayOfWeek(int year, int month) const {
    tm time_in = { 0 };
    time_in.tm_year = year - 1900;
    time_in.tm_mon = month - 1;
    time_in.tm_mday = 1;
    mktime(&time_in);
    return time_in.tm_wday;
}

D2D1_RECT_F WidgetCalendar::GetPrevRect() const {
    return D2D1::RectF(16.0f, 34.0f, 36.0f, 54.0f);
}

D2D1_RECT_F WidgetCalendar::GetNextRect() const {
    return D2D1::RectF(180.0f, 34.0f, 200.0f, 54.0f);
}

D2D1_RECT_F WidgetCalendar::GetTodayRect() const {
    return D2D1::RectF((float)m_width - 70.0f, 34.0f, (float)m_width - 16.0f, 54.0f);
}

void WidgetCalendar::OnMouseMove(int x, int y) {
    D2D1_RECT_F prevR = GetPrevRect();
    D2D1_RECT_F nextR = GetNextRect();
    D2D1_RECT_F todayR = GetTodayRect();

    bool prevH = (x >= prevR.left && x <= prevR.right && y >= prevR.top && y <= prevR.bottom);
    bool nextH = (x >= nextR.left && x <= nextR.right && y >= nextR.top && y <= nextR.bottom);
    bool todayH = (x >= todayR.left && x <= todayR.right && y >= todayR.top && y <= todayR.bottom);

    if (prevH != m_hoverPrev || nextH != m_hoverNext || todayH != m_hoverToday) {
        m_hoverPrev = prevH;
        m_hoverNext = nextH;
        m_hoverToday = todayH;
        Redraw();
    }
}

void WidgetCalendar::OnMouseLeave() {
    if (m_hoverPrev || m_hoverNext || m_hoverToday) {
        m_hoverPrev = false;
        m_hoverNext = false;
        m_hoverToday = false;
        Redraw();
    }
}

bool WidgetCalendar::OnMouseDown(int x, int y, bool isRight) {
    if (isRight) return false;

    D2D1_RECT_F prevR = GetPrevRect();
    D2D1_RECT_F nextR = GetNextRect();
    D2D1_RECT_F todayR = GetTodayRect();

    // Previous month button
    if (x >= prevR.left && x <= prevR.right && y >= prevR.top && y <= prevR.bottom) {
        m_viewMonth--;
        if (m_viewMonth < 1) {
            m_viewMonth = 12;
            m_viewYear--;
        }
        Redraw();
        return true;
    }

    // Next month button
    if (x >= nextR.left && x <= nextR.right && y >= nextR.top && y <= nextR.bottom) {
        m_viewMonth++;
        if (m_viewMonth > 12) {
            m_viewMonth = 1;
            m_viewYear++;
        }
        Redraw();
        return true;
    }

    // Today button
    if (x >= todayR.left && x <= todayR.right && y >= todayR.top && y <= todayR.bottom) {
        m_viewYear = m_todayYear;
        m_viewMonth = m_todayMonth;
        m_selectedDay = m_todayDay;
        Redraw();
        return true;
    }

    // Grid days click
    float gridTop = 86.0f;
    float cellW = ((float)m_width - 32.0f) / 7.0f;
    float cellH = 26.0f;

    if (y >= gridTop && y <= gridTop + 6 * cellH && x >= 16.0f && x <= (float)m_width - 16.0f) {
        int col = (int)((x - 16.0f) / cellW);
        int row = (int)((y - gridTop) / cellH);
        int slot = row * 7 + col;

        int firstDay = GetFirstDayOfWeek(m_viewYear, m_viewMonth);
        int totalDays = GetDaysInMonth(m_viewYear, m_viewMonth);

        int day = slot - firstDay + 1;
        if (day >= 1 && day <= totalDays) {
            m_selectedDay = day;
            Redraw();
            return true;
        }
    }

    return false;
}

void WidgetCalendar::OnRenderContent(ID2D1RenderTarget* rt) {
    D2DContext& ctx = D2DContext::Get();

    static const wchar_t* months[] = { L"", L"January", L"February", L"March", L"April", L"May", L"June",
                                       L"July", L"August", L"September", L"October", L"November", L"December" };

    // 1. Month Navigation Header
    D2DContext::DrawNavArrow(rt, GetPrevRect(), false, m_hoverPrev);
    D2DContext::DrawNavArrow(rt, GetNextRect(), true, m_hoverNext);

    wchar_t headerBuf[64];
    swprintf_s(headerBuf, L"%s %d", months[m_viewMonth], m_viewYear);

    D2D1_RECT_F monthRect = D2D1::RectF(40.0f, 32.0f, 175.0f, 54.0f);
    ID2D1SolidColorBrush* pMonthBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.98f), &pMonthBrush);
    if (pMonthBrush) {
        rt->DrawText(headerBuf, (UINT32)wcslen(headerBuf), ctx.GetFormatTitle(), monthRect, pMonthBrush);
        pMonthBrush->Release();
    }

    // "Today" button
    D2D1_RECT_F todayR = GetTodayRect();
    D2D1_ROUNDED_RECT todayRound = D2D1::RoundedRect(todayR, 4.0f, 4.0f);
    ID2D1SolidColorBrush* pBtnBg = nullptr;
    rt->CreateSolidColorBrush(m_hoverToday ? D2D1::ColorF(0.35f, 0.40f, 0.55f, 0.35f) : D2D1::ColorF(0.20f, 0.25f, 0.35f, 0.20f), &pBtnBg);
    if (pBtnBg) {
        rt->FillRoundedRectangle(todayRound, pBtnBg);
        pBtnBg->Release();
    }
    ID2D1SolidColorBrush* pBtnText = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.40f, 0.78f, 1.0f, 0.95f), &pBtnText);
    if (pBtnText) {
        D2D1_RECT_F tText = D2D1::RectF(todayR.left + 8.0f, todayR.top + 3.0f, todayR.right - 4.0f, todayR.bottom);
        rt->DrawText(L"Today", 5, ctx.GetFormatSmall(), tText, pBtnText);
        pBtnText->Release();
    }

    // 2. Day-of-Week headers (Su Mo Tu We Th Fr Sa)
    static const wchar_t* dow[] = { L"Su", L"Mo", L"Tu", L"We", L"Th", L"Fr", L"Sa" };
    float gridLeft = 16.0f;
    float cellW = ((float)m_width - 32.0f) / 7.0f;
    float curY = 64.0f;

    ID2D1SolidColorBrush* pDowBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.60f, 0.75f, 0.75f), &pDowBrush);
    if (pDowBrush) {
        for (int i = 0; i < 7; i++) {
            D2D1_RECT_F cellRect = D2D1::RectF(gridLeft + i * cellW + 6.0f, curY, gridLeft + (i + 1) * cellW, curY + 18.0f);
            rt->DrawText(dow[i], 2, ctx.GetFormatTiny(), cellRect, pDowBrush);
        }
        pDowBrush->Release();
    }

    // 3. Grid of Days
    float gridTop = 86.0f;
    float cellH = 26.0f;

    int firstDay = GetFirstDayOfWeek(m_viewYear, m_viewMonth);
    int totalDays = GetDaysInMonth(m_viewYear, m_viewMonth);

    // Prev month days
    int prevMonth = (m_viewMonth == 1) ? 12 : m_viewMonth - 1;
    int prevYear = (m_viewMonth == 1) ? m_viewYear - 1 : m_viewYear;
    int prevTotalDays = GetDaysInMonth(prevYear, prevMonth);

    ID2D1SolidColorBrush* pActiveBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.92f, 0.94f, 0.98f, 0.95f), &pActiveBrush);

    ID2D1SolidColorBrush* pDimBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.40f, 0.45f, 0.55f, 0.40f), &pDimBrush);

    ID2D1SolidColorBrush* pAccentBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.75f, 0.98f, 1.0f), &pAccentBrush);

    ID2D1SolidColorBrush* pAccentBg = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.75f, 0.98f, 0.35f), &pAccentBg);

    ID2D1SolidColorBrush* pSelectedBg = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.66f, 0.33f, 0.97f, 0.40f), &pSelectedBg);

    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 7; col++) {
            int slot = row * 7 + col;
            float cx = gridLeft + col * cellW + cellW / 2.0f;
            float cy = gridTop + row * cellH + cellH / 2.0f;
            D2D1_RECT_F textRect = D2D1::RectF(cx - 10.0f, cy - 8.0f, cx + 14.0f, cy + 12.0f);

            if (slot < firstDay) {
                // Dimmed previous month
                int d = prevTotalDays - (firstDay - 1 - slot);
                wchar_t buf[8];
                swprintf_s(buf, L"%d", d);
                if (pDimBrush) rt->DrawText(buf, (UINT32)wcslen(buf), ctx.GetFormatSmall(), textRect, pDimBrush);
            } else if (slot - firstDay + 1 <= totalDays) {
                // Current month day
                int d = slot - firstDay + 1;
                bool isToday = (m_viewYear == m_todayYear && m_viewMonth == m_todayMonth && d == m_todayDay);
                bool isSelected = (d == m_selectedDay);

                // Highlight background circle
                if (isToday) {
                    if (pAccentBg) rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 3.0f, cy), 12.0f, 12.0f), pAccentBg);
                    if (pAccentBrush) rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 3.0f, cy), 12.0f, 12.0f), pAccentBrush, 1.5f);
                } else if (isSelected) {
                    if (pSelectedBg) rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 3.0f, cy), 12.0f, 12.0f), pSelectedBg);
                }

                wchar_t buf[8];
                swprintf_s(buf, L"%d", d);
                ID2D1SolidColorBrush* useBrush = isToday ? pAccentBrush : pActiveBrush;
                if (useBrush) rt->DrawText(buf, (UINT32)wcslen(buf), ctx.GetFormatSmall(), textRect, useBrush);
            } else {
                // Next month day
                int d = slot - firstDay + 1 - totalDays;
                wchar_t buf[8];
                swprintf_s(buf, L"%d", d);
                if (pDimBrush) rt->DrawText(buf, (UINT32)wcslen(buf), ctx.GetFormatSmall(), textRect, pDimBrush);
            }
        }
    }

    if (pActiveBrush) pActiveBrush->Release();
    if (pDimBrush) pDimBrush->Release();
    if (pAccentBrush) pAccentBrush->Release();
    if (pAccentBg) pAccentBg->Release();
    if (pSelectedBg) pSelectedBg->Release();
}
