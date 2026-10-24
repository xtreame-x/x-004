#pragma once
#include "widget_base.h"

enum ClockTheme {
    THEME_FROST_GLASS = 0, // Windows 11 Acrylic dark frosted glass + cyan accent
    THEME_MIDNIGHT_OLED,   // Ultra-clean pitch black minimalist card + white typography
    THEME_SUNSET_HORIZON,  // Warm coral & amber twilight glow
    THEME_AURORA_EMERALD,  // Mint & deep calm emerald glass
    THEME_FLOATING_GHOST,  // Transparent borderless typography on wallpaper
    THEME_COUNT
};

class WidgetClock : public WidgetBase {
public:
    WidgetClock();
    ~WidgetClock() override;
    void OnUpdate() override;

protected:
    void OnRenderContent(ID2D1RenderTarget* rt) override;
    bool OnMouseDown(int x, int y, bool isRight) override;
    void OnDoubleClick(int x, int y) override;
    void OnBuildContextMenu(HMENU hMenu) override;
    bool OnMenuCommand(int cmd) override;

private:
    IDWriteTextFormat* GetOrCreateClockFormat(float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_SEMI_BOLD);
    void DrawBackgroundCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& cardRect);
    void CycleTheme();

    ClockTheme m_theme = THEME_FROST_GLASS;
    bool m_is24Hour = false;
    bool m_showSeconds = true;
    bool m_showGreeting = true;
    bool m_showProgress = true;

    std::wstring m_timeString;
    std::wstring m_secString;
    std::wstring m_ampmString;
    std::wstring m_dateString;
    std::wstring m_dayString;
    std::wstring m_shortDateString;
    std::wstring m_greetingString;
    std::wstring m_greetingIcon;
    float m_dayProgressPercent = 0.0f;

    IDWriteTextFormat* m_pDynamicClockFormat = nullptr;
    float m_cachedFontSize = 0.0f;
};
