#pragma once
#include "widget_base.h"

class WidgetCalendar : public WidgetBase {
public:
    WidgetCalendar();
    void OnUpdate() override;

protected:
    void OnRenderContent(ID2D1RenderTarget* rt) override;
    bool OnMouseDown(int x, int y, bool isRight) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseLeave() override;

private:
    int m_viewYear = 2026;
    int m_viewMonth = 10; // 1-12
    int m_todayYear = 2026;
    int m_todayMonth = 10;
    int m_todayDay = 22;
    int m_selectedDay = 22;

    int m_hoverDay = -1;
    bool m_hoverPrev = false;
    bool m_hoverNext = false;
    bool m_hoverToday = false;

    D2D1_RECT_F GetPrevRect() const;
    D2D1_RECT_F GetNextRect() const;
    D2D1_RECT_F GetTodayRect() const;

    int GetDaysInMonth(int year, int month) const;
    int GetFirstDayOfWeek(int year, int month) const; // 0 = Sunday, 1 = Monday...
};
