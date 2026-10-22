#pragma once
#include <windows.h>
#include <vector>
#include <memory>
#include <string>
#include "widgets/widget_base.h"

#define WM_TRAYICON (WM_USER + 200)
#define ID_TRAY_EXIT 3001
#define ID_TRAY_RESET 3002
#define ID_TRAY_PINALL 3003
#define ID_TRAY_LOCKALL 3004
#define ID_TRAY_SHOWALL 3005
#define ID_TRAY_TOGGLE_BASE 3100

class WidgetManager {
public:
    WidgetManager();
    ~WidgetManager();

    bool Initialize(HINSTANCE hInstance);
    void Run();
    void Shutdown();

    void SaveConfig();
    void LoadConfig();
    void ResetLayout();

    void ShowAll();
    void ToggleWidget(const std::wstring& id);
    WidgetBase* GetWidget(const std::wstring& id);

    void PinAll(bool pinned);
    void LockAll(bool locked);
    bool IsAllPinned() const { return m_allPinned; }
    bool IsAllLocked() const { return m_allLocked; }

private:
    void SetupTrayIcon(HINSTANCE hInstance);
    void RemoveTrayIcon();
    void ShowTrayMenu();

    static LRESULT CALLBACK HiddenWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleHiddenMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hHiddenWnd = NULL;
    NOTIFYICONDATAW m_nid;
    HINSTANCE m_hInstance = NULL;

    std::vector<std::unique_ptr<WidgetBase>> m_widgets;
    bool m_allPinned = false;
    bool m_allLocked = false;
};
