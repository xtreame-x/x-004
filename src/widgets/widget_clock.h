#pragma once
#include "widget_base.h"

class WidgetClock : public WidgetBase {
public:
    WidgetClock();
    void OnUpdate() override;

protected:
    void OnRenderContent(ID2D1RenderTarget* rt) override;
    bool OnMouseDown(int x, int y, bool isRight) override;

private:
    bool m_is24Hour = false;
    std::wstring m_timeString;
    std::wstring m_secString;
    std::wstring m_ampmString;
    std::wstring m_dateString;
    std::wstring m_greetingString;
};
