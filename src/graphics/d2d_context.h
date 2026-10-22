#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>

class D2DContext {
public:
    static D2DContext& Get();

    bool Initialize();
    void Cleanup();

    ID2D1Factory* GetD2DFactory() const { return m_pD2DFactory; }
    IDWriteFactory* GetDWriteFactory() const { return m_pDWriteFactory; }

    IDWriteTextFormat* GetFormatClock() const { return m_pFormatClock; }
    IDWriteTextFormat* GetFormatClockSmall() const { return m_pFormatClockSmall; }
    IDWriteTextFormat* GetFormatTitle() const { return m_pFormatTitle; }
    IDWriteTextFormat* GetFormatSubtitle() const { return m_pFormatSubtitle; }
    IDWriteTextFormat* GetFormatBody() const { return m_pFormatBody; }
    IDWriteTextFormat* GetFormatBodyBold() const { return m_pFormatBodyBold; }
    IDWriteTextFormat* GetFormatSmall() const { return m_pFormatSmall; }
    IDWriteTextFormat* GetFormatTiny() const { return m_pFormatTiny; }

    // Drawing helpers
    static void DrawGlassPanel(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, float radius, float alpha = 0.92f);
    static void DrawInnerCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, float radius, bool hovered = false);
    static void DrawProgressBar(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, float percent, D2D1_COLOR_F barColor, D2D1_COLOR_F bgColor);
    static void DrawCheckbox(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, bool checked, bool hovered);
    static void DrawCloseButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, bool hovered);
    static void DrawPinButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, bool pinned, bool hovered);
    static void DrawNavArrow(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, bool isRight, bool hovered);
    static void DrawWeatherVector(ID2D1RenderTarget* rt, int weatherCode, bool isDay, const D2D1_RECT_F& rect);
    static void DrawBatteryVector(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, int percent, bool isCharging, bool isAC);
    static void DrawResizeGrip(ID2D1RenderTarget* rt, float x, float y, float alpha = 0.35f);

private:
    D2DContext();
    ~D2DContext();

    ID2D1Factory* m_pD2DFactory = nullptr;
    IDWriteFactory* m_pDWriteFactory = nullptr;

    IDWriteTextFormat* m_pFormatClock = nullptr;
    IDWriteTextFormat* m_pFormatClockSmall = nullptr;
    IDWriteTextFormat* m_pFormatTitle = nullptr;
    IDWriteTextFormat* m_pFormatSubtitle = nullptr;
    IDWriteTextFormat* m_pFormatBody = nullptr;
    IDWriteTextFormat* m_pFormatBodyBold = nullptr;
    IDWriteTextFormat* m_pFormatSmall = nullptr;
    IDWriteTextFormat* m_pFormatTiny = nullptr;
};
