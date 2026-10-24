#include <windows.h>
#include "app.h"
#include <cstdio>

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    HANDLE hFile = CreateFileW(L"crash.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Crash Exception Code: 0x%08lX at Address: %p\n",
            (unsigned long)ep->ExceptionRecord->ExceptionCode,
            ep->ExceptionRecord->ExceptionAddress);
        DWORD written = 0;
        WriteFile(hFile, buf, (DWORD)strlen(buf), &written, NULL);
        CloseHandle(hFile);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    SetUnhandledExceptionFilter(CrashHandler);

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

    // 3. Initialize and Run Widget Manager
    {
        WidgetManager app;
        if (app.Initialize(hInstance)) {
            app.Run();
        }
    }

    // 4. Cleanup
    CoUninitialize();

    return 0;
}

// Fallback entry point for toolchains linking against WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
