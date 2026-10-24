#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>

class WidgetManager;

class WidgetBase {
public:
    enum DisplayMode {
        MODE_DESKTOP_LAYER = 0, // Attached to Desktop right above wallpaper/icons (immune to 3-finger swipe)
        MODE_NORMAL = 1,        // Normal floating window
        MODE_ALWAYS_ON_TOP = 2  // Always on top
    };

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
    void SetMinMaxSize(int minW, int minH, int maxW, int maxH) {
        m_minWidth = minW;
        m_minHeight = minH;
        m_maxWidth = maxW;
        m_maxHeight = maxH;
    }

    DisplayMode GetDisplayMode() const { return m_displayMode; }
    void SetDisplayMode(DisplayMode mode);

    bool IsPinned() const { return m_displayMode == MODE_DESKTOP_LAYER; }
    void SetPinned(bool pinned) { SetDisplayMode(pinned ? MODE_DESKTOP_LAYER : MODE_NORMAL); }

    bool IsAlwaysOnTop() const { return m_displayMode == MODE_ALWAYS_ON_TOP; }
    void SetAlwaysOnTop(bool onTop) { SetDisplayMode(onTop ? MODE_ALWAYS_ON_TOP : MODE_NORMAL); }

    bool IsLocked() const { return m_isLocked; }
    void SetLocked(bool locked);

    float GetOpacity() const { return m_opacity; }
    void SetOpacity(float opacity);

    void Redraw();
    virtual void OnUpdate() {}

    void SetManager(WidgetManager* mgr) { m_pManager = mgr; }

    void SetKeyboardFocusRequired(bool req) { m_requiresKeyboardFocus = req; }

protected:
    virtual void OnRenderContent(ID2D1RenderTarget* rt) = 0;
    virtual bool OnMouseDown(int x, int y, bool isRight) { return false; }
    virtual void OnDoubleClick(int x, int y) {}
    virtual void OnBuildContextMenu(HMENU hMenu) {}
    virtual bool OnMenuCommand(int cmd) { return false; }
    virtual void OnMouseMove(int x, int y) {}
    virtual void OnMouseLeave() {}
    virtual void OnCharInput(wchar_t ch) {}
    virtual void OnKeyDown(WPARAM key) {}

    void Render();
    bool EnsureRenderTarget();
    void ApplyDisplayMode();
    static HWND FindDesktopHostWindow();

    std::wstring m_id;
    std::wstring m_title;
    int m_x = 100;
    int m_y = 100;
    int m_width = 280;
    int m_height = 180;
    bool m_visible = true;

    DisplayMode m_displayMode = MODE_DESKTOP_LAYER;
    bool m_isLocked = false;       // Lock position from drag
    float m_opacity = 0.95f;
    bool m_requiresKeyboardFocus = false;
    bool m_showWindowHeader = true;

    int m_minWidth = 150;
    int m_minHeight = 80;
    int m_maxWidth = 800;
    int m_maxHeight = 600;

    // Smooth non-activating manual dragging
    bool m_isDragging = false;
    POINT m_dragStartCursor = { 0, 0 };
    POINT m_dragStartPos = { 0, 0 };

    HWND m_hwnd = NULL;
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;
    WidgetManager* m_pManager = nullptr;

    bool m_hoverClose = false;
    bool m_hoverPin = false;

    D2D1_RECT_F GetCloseRect() const;
    D2D1_RECT_F GetPinRect() const;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void ShowContextMenu(int screenX, int screenY);
};
