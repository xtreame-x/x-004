#include "app.h"
#include "graphics/d2d_context.h"
#include "widgets/widget_clock.h"
#include "widgets/widget_system.h"
#include "widgets/widget_weather.h"
#include "widgets/widget_calendar.h"
#include "widgets/widget_tasks.h"
#include "widgets/widget_news.h"
#include "widgets/widget_hub.h"
#include "utils/json_mini.h"
#include "utils/http_client.h"
#include <fstream>
#include <sstream>

static const wchar_t* HIDDEN_CLASS = L"WinWidgetsHiddenClass";

WidgetManager::WidgetManager() {
    ZeroMemory(&m_nid, sizeof(m_nid));
}

WidgetManager::~WidgetManager() {
    Shutdown();
}

bool WidgetManager::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    // 1. Initialize Direct2D and DirectWrite
    if (!D2DContext::Get().Initialize()) {
        MessageBoxW(NULL, L"Failed to initialize Direct2D / DirectWrite graphics.", L"WinWidgets Error", MB_ICONERROR);
        return false;
    }

    // 2. Register hidden window class for timers and tray icon messages
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WidgetManager::HiddenWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = HIDDEN_CLASS;
    RegisterClassExW(&wc);

    m_hHiddenWnd = CreateWindowExW(
        0, HIDDEN_CLASS, L"WinWidgetsManager", 0,
        0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, this
    );

    // 3. Create all widgets
    m_widgets.push_back(std::make_unique<WidgetClock>());
    m_widgets.push_back(std::make_unique<WidgetSystem>());
    m_widgets.push_back(std::make_unique<WidgetWeather>());
    m_widgets.push_back(std::make_unique<WidgetCalendar>());
    m_widgets.push_back(std::make_unique<WidgetTasks>());
    m_widgets.push_back(std::make_unique<WidgetNews>());
    m_widgets.push_back(std::make_unique<WidgetHub>());

    for (auto& w : m_widgets) {
        w->SetManager(this);
    }

    // 4. Default positions or Load saved positions
    LoadConfig();

    // 5. Initialize Win32 windows for each widget
    for (auto& w : m_widgets) {
        w->Create();
    }

    // 6. Setup system timer (1 second interval for clock, cpu, etc.)
    SetTimer(m_hHiddenWnd, 1001, 1000, NULL);

    // 7. Setup system tray icon
    SetupTrayIcon(hInstance);

    return true;
}

void WidgetManager::Run() {
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void WidgetManager::Shutdown() {
    KillTimer(m_hHiddenWnd, 1001);
    RemoveTrayIcon();

    for (auto& w : m_widgets) {
        w->Destroy();
    }
    m_widgets.clear();

    if (m_hHiddenWnd) {
        DestroyWindow(m_hHiddenWnd);
        m_hHiddenWnd = NULL;
    }

    D2DContext::Get().Cleanup();
}

void WidgetManager::ResetLayout() {
    RECT rcWork;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);

    int screenW = rcWork.right - rcWork.left;
    int screenH = rcWork.bottom - rcWork.top;

    int rightMargin = rcWork.right - 20;
    int topMargin = rcWork.top + 25;

    // Col 1 (Rightmost column): Calendar, Tasks, News
    int col1W = 310;
    int col1X = rightMargin - col1W;

    // Col 2 (Left of Col 1): Clock, Weather, System, Hub
    int col2W = 280;
    int col2X = col1X - col2W - 15;

    // Col 2 stack
    WidgetBase* wClock = GetWidget(L"clock");
    if (wClock) wClock->SetPosition(col2X, topMargin);

    WidgetBase* wWeather = GetWidget(L"weather");
    if (wWeather) wWeather->SetPosition(col2X, topMargin + 145);

    WidgetBase* wSystem = GetWidget(L"system");
    if (wSystem) wSystem->SetPosition(col2X, topMargin + 310);

    // Col 1 stack
    WidgetBase* wCalendar = GetWidget(L"calendar");
    if (wCalendar) wCalendar->SetPosition(col1X, topMargin);

    WidgetBase* wTasks = GetWidget(L"tasks");
    if (wTasks) wTasks->SetPosition(col1X, topMargin + 270);

    WidgetBase* wNews = GetWidget(L"news");
    if (wNews) wNews->SetPosition(col1X - 30, topMargin + 560);

    // Hub bottom center or top
    WidgetBase* wHub = GetWidget(L"hub");
    if (wHub) {
        int hubX = (screenW - wHub->GetWidth()) / 2;
        int hubY = rcWork.bottom - wHub->GetHeight() - 15;
        wHub->SetPosition(hubX, hubY);
    }

    ShowAll();
    SaveConfig();
}

void WidgetManager::LoadConfig() {
    std::ifstream file("widgets_config.json");
    if (!file.is_open()) {
        ResetLayout();
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    mini_json::Value root = mini_json::Value::parse(buffer.str());
    if (!root.is_object()) {
        ResetLayout();
        return;
    }

    m_allLocked = root["all_locked"].as_bool(false);
    m_allPinned = root["all_pinned"].as_bool(false);

    if (root.has("widgets")) {
        const auto& wList = root["widgets"];
        for (auto& w : m_widgets) {
            std::string idUtf8 = HttpClient::WideToUtf8(w->GetId());
            if (wList.has(idUtf8)) {
                const auto& item = wList[idUtf8];
                int x = item["x"].as_int(w->GetX());
                int y = item["y"].as_int(w->GetY());
                int width = item["width"].as_int(w->GetWidth());
                int height = item["height"].as_int(w->GetHeight());
                bool vis = item["visible"].as_bool(true);
                bool pinned = item["pinned"].as_bool(false);
                float op = (float)item["opacity"].as_number((double)w->GetOpacity());

                w->SetPosition(x, y);
                w->SetSize(width, height);
                w->Show(vis);
                w->SetPinned(pinned);
                w->SetOpacity(op);
                w->SetLocked(m_allLocked);
            }
        }
    }
}

void WidgetManager::SaveConfig() {
    mini_json::Value root;
    root["all_locked"] = m_allLocked;
    root["all_pinned"] = m_allPinned;

    mini_json::Value wList;
    for (const auto& w : m_widgets) {
        std::string idUtf8 = HttpClient::WideToUtf8(w->GetId());
        mini_json::Value item;
        item["x"] = w->GetX();
        item["y"] = w->GetY();
        item["width"] = w->GetWidth();
        item["height"] = w->GetHeight();
        item["visible"] = w->IsVisible();
        item["pinned"] = w->IsPinned();
        item["opacity"] = (double)w->GetOpacity();
        wList[idUtf8] = item;
    }
    root["widgets"] = wList;

    std::ofstream file("widgets_config.json");
    if (file.is_open()) {
        file << root.serialize(0);
        file.close();
    }
}

void WidgetManager::ShowAll() {
    for (auto& w : m_widgets) {
        w->Show(true);
    }
    SaveConfig();
}

void WidgetManager::ToggleWidget(const std::wstring& id) {
    WidgetBase* w = GetWidget(id);
    if (w) {
        w->Show(!w->IsVisible());
        SaveConfig();
    }
}

WidgetBase* WidgetManager::GetWidget(const std::wstring& id) {
    for (auto& w : m_widgets) {
        if (w->GetId() == id) {
            return w.get();
        }
    }
    return nullptr;
}

void WidgetManager::PinAll(bool pinned) {
    m_allPinned = pinned;
    for (auto& w : m_widgets) {
        w->SetPinned(pinned);
    }
    SaveConfig();
}

void WidgetManager::LockAll(bool locked) {
    m_allLocked = locked;
    for (auto& w : m_widgets) {
        w->SetLocked(locked);
    }
    SaveConfig();
}

void WidgetManager::SetupTrayIcon(HINSTANCE hInstance) {
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hHiddenWnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(m_nid.szTip, L"WinWidgets - Desktop Gadgets");

    Shell_NotifyIconW(NIM_ADD, &m_nid);
}

void WidgetManager::RemoveTrayIcon() {
    if (m_nid.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
    }
}

void WidgetManager::ShowTrayMenu() {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    for (size_t i = 0; i < m_widgets.size(); i++) {
        UINT flags = MF_BYPOSITION | (m_widgets[i]->IsVisible() ? MF_CHECKED : MF_UNCHECKED);
        InsertMenuW(hMenu, (UINT)i, flags, ID_TRAY_TOGGLE_BASE + (UINT)i, m_widgets[i]->GetTitle().c_str());
    }

    UINT curIdx = (UINT)m_widgets.size();
    InsertMenuW(hMenu, curIdx++, MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, curIdx++, MF_BYPOSITION | (m_allPinned ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_PINALL, L"Pin All to Desktop");
    InsertMenuW(hMenu, curIdx++, MF_BYPOSITION | (m_allLocked ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_LOCKALL, L"Lock All Widget Positions");
    InsertMenuW(hMenu, curIdx++, MF_BYPOSITION, ID_TRAY_SHOWALL, L"Show All Widgets");
    InsertMenuW(hMenu, curIdx++, MF_BYPOSITION, ID_TRAY_RESET, L"Reset Default Layout");
    InsertMenuW(hMenu, curIdx++, MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, curIdx++, MF_BYPOSITION, ID_TRAY_EXIT, L"Exit WinWidgets");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hHiddenWnd);

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hHiddenWnd, NULL);
    DestroyMenu(hMenu);

    if (cmd >= ID_TRAY_TOGGLE_BASE && cmd < (int)(ID_TRAY_TOGGLE_BASE + m_widgets.size())) {
        size_t idx = cmd - ID_TRAY_TOGGLE_BASE;
        m_widgets[idx]->Show(!m_widgets[idx]->IsVisible());
        SaveConfig();
    } else {
        switch (cmd) {
            case ID_TRAY_PINALL:
                PinAll(!m_allPinned);
                break;
            case ID_TRAY_LOCKALL:
                LockAll(!m_allLocked);
                break;
            case ID_TRAY_SHOWALL:
                ShowAll();
                break;
            case ID_TRAY_RESET:
                ResetLayout();
                break;
            case ID_TRAY_EXIT:
                PostQuitMessage(0);
                break;
        }
    }
}

LRESULT CALLBACK WidgetManager::HiddenWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WidgetManager* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<WidgetManager*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<WidgetManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleHiddenMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT WidgetManager::HandleHiddenMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER: {
            for (auto& w : m_widgets) {
                if (w->IsVisible()) {
                    w->OnUpdate();
                }
            }
            return 0;
        }

        case WM_TRAYICON: {
            if (lParam == WM_RBUTTONUP) {
                ShowTrayMenu();
            } else if (lParam == WM_LBUTTONUP) {
                ShowAll();
            }
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
