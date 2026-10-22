#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>

class WidgetManager;

class WidgetBase {
public:
    WidgetBase(const std::wstring& id, const std::wstring& title, int defaultWidth, int defaultHeight);
    virtual ~WidgetBase();

    bool Create(HWND hParent = NULL);
    void Destroy();
    void Show(bool show = true);
    bool IsVisible() const { return m_visible; }

    HWND GetHwnd() const { return m_hwnd; }
    const std::wstring& GetId() const { return m_id; }
    const std::wstring& GetTitle() const { return m_title; }

    int GetX() const { return m_x; }
    int GetY() const { return m_y; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    void SetPosition(int x, int y);
    void SetSize(int w, int h);

    bool IsPinned() const { return m_isPinned; }
    void SetPinned(bool pinned);

    bool IsAlwaysOnTop() const { return m_isAlwaysOnTop; }
    void SetAlwaysOnTop(bool onTop);

    bool IsLocked() const { return m_isLocked; }
    void SetLocked(bool locked);

    float GetOpacity() const { return m_opacity; }
    void SetOpacity(float opacity);

    void Redraw();
    virtual void OnUpdate() {}

    void SetManager(WidgetManager* mgr) { m_pManager = mgr; }

protected:
    virtual void OnRenderContent(ID2D1RenderTarget* rt) = 0;
    virtual bool OnMouseDown(int x, int y, bool isRight) { return false; }
    virtual void OnMouseMove(int x, int y) {}
    virtual void OnMouseLeave() {}
    virtual void OnCharInput(wchar_t ch) {}
    virtual void OnKeyDown(WPARAM key) {}

    void Render();
    bool EnsureRenderTarget();

    std::wstring m_id;
    std::wstring m_title;
    int m_x = 100;
    int m_y = 100;
    int m_width = 280;
    int m_height = 180;
    bool m_visible = true;
    bool m_isPinned = false;       // Pinned to desktop (HWND_BOTTOM)
    bool m_isAlwaysOnTop = false;  // Always on top (HWND_TOPMOST)
    bool m_isLocked = false;       // Lock position from drag
    float m_opacity = 0.95f;

    HWND m_hwnd = NULL;
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;
    WidgetManager* m_pManager = nullptr;

    bool m_hoverClose = false;
    bool m_hoverPin = false;

    D2D1_RECT_F GetCloseRect() const;
    D2D1_RECT_F GetPinRect() const;

    // Resize support
    enum ResizeEdge { RESIZE_NONE = 0, RESIZE_LEFT, RESIZE_RIGHT, RESIZE_TOP, RESIZE_BOTTOM,
                      RESIZE_TOPLEFT, RESIZE_TOPRIGHT, RESIZE_BOTTOMLEFT, RESIZE_BOTTOMRIGHT };
    ResizeEdge m_resizeEdge = RESIZE_NONE;
    bool m_isResizing = false;
    POINT m_resizeStartMouse;
    RECT m_resizeStartRect;

    int m_minWidth = 150;
    int m_minHeight = 80;
    int m_maxWidth = 800;
    int m_maxHeight = 600;

    ResizeEdge HitTestResize(int x, int y) const;
    void DrawResizeGrip(ID2D1RenderTarget* rt);
    void UpdateCursorForEdge(ResizeEdge edge);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void ShowContextMenu(int screenX, int screenY);
};
