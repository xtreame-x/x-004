#pragma once
#include "widget_base.h"
#include <vector>

struct HubButton {
    std::wstring id;
    std::wstring label;
    D2D1_COLOR_F activeColor;
};

class WidgetHub : public WidgetBase {
public:
    WidgetHub();

protected:
    void OnRenderContent(ID2D1RenderTarget* rt) override;
    bool OnMouseDown(int x, int y, bool isRight) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseLeave() override;

private:
    std::vector<HubButton> m_buttons;
    int m_hoverButtonIndex = -1;
    D2D1_RECT_F GetButtonRect(size_t index) const;
};
