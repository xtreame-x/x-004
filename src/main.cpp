#include <windows.h>
#include "app.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // 1. Enable Per-Monitor High DPI awareness dynamically
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
        SetProcessDpiAwarenessContextProc setDpi = (SetProcessDpiAwarenessContextProc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (setDpi) {
            setDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }

    // 2. Initialize COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        return 1;
    }

    // 3. Prevent duplicate instances (optional mutex)
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\WinWidgetsAppMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"WinWidgets is already running! Check your system tray near the clock.", L"WinWidgets", MB_OK | MB_ICONINFORMATION);
        CloseHandle(hMutex);
        CoUninitialize();
        return 0;
    }

    // 4. Initialize and Run Widget Manager
    {
        WidgetManager app;
        if (app.Initialize(hInstance)) {
            app.Run();
        }
    }

    // 5. Cleanup
    if (hMutex) {
        CloseHandle(hMutex);
    }
    CoUninitialize();

    return 0;
}

// Fallback entry point for toolchains linking against WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
