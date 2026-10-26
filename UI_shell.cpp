// ============================================================================
//  ChromeOS  ·  "Neutral Obsidian Monolith"  —  advanced native desktop shell
//  Win32 + Direct2D + DirectWrite. Single translation unit. No third-party
//  libraries, no image files: every surface, gradient, shadow and icon is
//  drawn procedurally (icons are Segoe MDL2 Assets glyphs).
//
//  BUILD (MSVC, "x64 Native Tools" prompt):
//    cl /O2 /EHsc /std:c++17 /utf-8 UI_shell.cpp /link /SUBSYSTEM:WINDOWS ^
//       d2d1.lib dwrite.lib user32.lib gdi32.lib
//
//  BUILD (MinGW-w64):
//    g++ -std=c++17 -O2 -s -mwindows -static UI_shell.cpp -o UI_shell.exe ^
//        -ld2d1 -ldwrite -luser32 -lgdi32
//
//  WHAT IS IN HERE
//    · Animated wallpaper engine with six presets, a picker, cross-fading swaps.
//    · Eight desk widgets that flow into three columns, in three sizes, each
//      individually showable/hideable from Settings > Appearance.
//    · Settings app with System, Appearance and Advanced pages; everything it
//      changes (wallpaper, widget size/visibility, 12/24-hour clock, motion)
//      persists to shell.ini.
//    · Shelf with launcher, pinned apps, running indicators, magnification.
//    · System tray + unified quick-settings / notification bubble with working
//      toggles, volume + brightness sliders, dismissible notifications.
//    · Full-screen launcher: search, paged icon grid, keyboard navigation.
//    · A small but real window manager: drag, resize, focus z-order, minimise,
//      maximise, edge snapping (left/right half, top = full), per-app content
//      (Files, Terminal with a live prompt, Settings, Text Editor, About).
//    · Lock screen, toasts, right-click desktop menu, state persistence.
//
//  KEYS
//    Ctrl+Space  launcher (the Windows key itself is reserved by the OS)
//    F11  fullscreen        Esc  back           Ctrl+Q  quit
//    Enter  launch / unlock / run a terminal command
//    Arrows  launcher navigation    Backspace  edit the search query
//
//  MOUSE
//    Tray pill (bottom right) → system bubble.  Launcher circle (bottom left)
//    → app grid.  Drag volume/brightness/seek.  Drag title bars, edges and
//    corners.  Drag a window to a screen edge to snap it.  Right-click desktop
//    for the context menu.
//
//  LAYOUT MATHS
//    Widget placement and the hit-rectangle transform live in shell_layout.h,
//    which has no Windows dependencies and is covered by tests/layout_test.cpp.
// ============================================================================
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#undef DrawText
#include <d2d1.h>
#include <dwrite.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <map>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <winhttp.h>
#include <wlanapi.h>
#include <bluetoothapis.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <wincodec.h>
#include <commdlg.h>

#include "shell_layout.h"

#ifdef _MSC_VER
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "bthprops.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "comdlg32.lib")
#endif

// ----------------------------------------------------------------------------
//  Design space. Everything below is authored in 1920x1200 units and mapped
//  onto the client rectangle, so the whole shell scales with the window (and
//  therefore with DPI) instead of breaking at other resolutions.
// ----------------------------------------------------------------------------
static const float DW = 1920.f;
static const float DH = 1200.f;
static const wchar_t* VERSION = L"2.1";

static ID2D1Factory*             g_d2d = nullptr;
static IDWriteFactory*           g_dw = nullptr;
static ID2D1HwndRenderTarget*    g_rt = nullptr;
static ID2D1SolidColorBrush*     g_br = nullptr;
static ID2D1LinearGradientBrush* g_wpA = nullptr;      // current wallpaper
static ID2D1LinearGradientBrush* g_wpB = nullptr;      // outgoing wallpaper
static ID2D1RadialGradientBrush* g_glow = nullptr;     // vignette / glow
static HWND                       g_hwnd = nullptr;
static std::map<std::wstring, IDWriteTextFormat*> g_fmt;

static float g_scale = 1.f, g_ox = 0.f, g_oy = 0.f;
static D2D1_MATRIX_3X2_F g_base = D2D1::Matrix3x2F::Identity();
static int   g_clientW = 0, g_clientH = 0;

template <class T> static void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }
static float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float EaseOut(float t) { t = Clamp(t, 0.f, 1.f); float u = 1.f - t; return 1.f - u * u * u; }

// ----------------------------------------------------------------------------
//  Palette — "Neutral Obsidian Monolith" specification
// ----------------------------------------------------------------------------
static const unsigned WHITE = 0xFFFFFF;
static const unsigned BLACK = 0x000000;

static const unsigned COL_LETTER  = 0x07080A;   // letterbox / window bg
static const unsigned COL_SURF    = 0x121316;   // cards, flyouts, shelf
static const unsigned COL_TILE    = 0x171717;   // icon tiles, score boxes
static const unsigned COL_TILE_HI = 0x202024;   // tile hover
static const unsigned COL_SEC     = 0x262626;   // secondary surfaces
static const unsigned COL_ACTIVE  = 0xEDEDED;   // active pill, play button

static const unsigned COL_T100 = 0xFFFFFF;
static const unsigned COL_T90  = 0xF5F5F5;
static const unsigned COL_T80  = 0xE5E5E5;
static const unsigned COL_T70  = 0xD4D4D4;
static const unsigned COL_T50  = 0xA3A3A3;
static const unsigned COL_T35  = 0x737373;

static const unsigned COL_ACC    = 0x34D399;    // confirmed / battery
static const unsigned COL_ACC_BG = 0x10B981;    // 10% accents only

// Wallpaper presets. Each is a 3-stop vertical gradient; preset 0 is the
// original #090A0D → #181B22 → #0D0E12 spec.
struct WpPreset { unsigned a, b, c, glow; const wchar_t* name; const wchar_t* file; };
static const WpPreset WP[] = {
    { 0x090A0D, 0x181B22, 0x0D0E12, 0x2A3340, L"Misty Castle (Glass)", L"wallpaper_glass.jpg" },
    { 0x090A0D, 0x181B22, 0x0D0E12, 0x2A3340, L"Obsidian Void",        nullptr },
    { 0x060F12, 0x0E2429, 0x05090B, 0x123A38, L"Deep Teal",           nullptr },
    { 0x0A0A0C, 0x1C1C20, 0x08080A, 0x33333A, L"Graphite Noir",       nullptr },
    { 0x090A16, 0x191A33, 0x07070E, 0x24265A, L"Indigo Nebula",       nullptr },
    { 0x0B0A08, 0x211B12, 0x0A0806, 0x3A2C16, L"Ember Glow",          nullptr },
    { 0x070C0A, 0x14251E, 0x050B08, 0x1E4034, L"Emerald Forest",      nullptr },
    { 0x080A0D, 0x141820, 0x10141C, 0x344055, L"Custom Picture...",    L"" },
};
static const int WP_COUNT = (int)(sizeof(WP) / sizeof(WP[0]));

// ----------------------------------------------------------------------------
//  Colour + primitive drawing (all coordinates are design-space units)
// ----------------------------------------------------------------------------
static D2D1_COLOR_F C(unsigned rgb, float a = 1.f) { return D2D1::ColorF((UINT32)rgb, a); }
static D2D1_RECT_F R(float x, float y, float w, float h) { return D2D1::RectF(x, y, x + w, y + h); }

static void Fill(float x, float y, float w, float h, float r, D2D1_COLOR_F c) {
    if (w <= 0.f || h <= 0.f) return;
    g_br->SetColor(c);
    g_rt->FillRoundedRectangle(D2D1::RoundedRect(R(x, y, w, h), r, r), g_br);
}
static void Stroke(float x, float y, float w, float h, float r, D2D1_COLOR_F c, float sw = 1.f) {
    if (w <= 0.f || h <= 0.f) return;
    g_br->SetColor(c);
    g_rt->DrawRoundedRectangle(D2D1::RoundedRect(R(x + 0.5f, y + 0.5f, w - 1.f, h - 1.f), r, r), g_br, sw);
}
static void Disc(float cx, float cy, float r, D2D1_COLOR_F c) {
    g_br->SetColor(c);
    g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), g_br);
}
static void Ring(float cx, float cy, float r, D2D1_COLOR_F c, float sw = 1.f) {
    g_br->SetColor(c);
    g_rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), g_br, sw);
}
static void Line(float x0, float y0, float x1, float y1, D2D1_COLOR_F c, float sw = 1.f) {
    g_br->SetColor(c);
    g_rt->DrawLine(D2D1::Point2F(x0, y0), D2D1::Point2F(x1, y1), g_br, sw);
}
// Soft fake drop shadow: Direct2D 1.0 has no blur, so stack translucent
// rounded rectangles that grow and fade.
static void Shadow(float x, float y, float w, float h, float r, float strength = 0.055f, int steps = 7) {
    for (int i = 1; i <= steps; ++i) {
        float k = 1.f - (float)i / (float)(steps + 1);
        Fill(x - i * 2.f, y - i * 2.f + 10.f, w + i * 4.f, h + i * 4.f, r + i * 2.f,
             C(BLACK, strength * k));
    }
}
// Elevated frosted glass card: translucent tinted glass + ambient upper sheen + specular bevel rim + deep shadow.
static void Card(float x, float y, float w, float h, float r, float alpha = 0.52f, bool shadow = true) {
    if (w <= 0.f || h <= 0.f) return;
    if (shadow) Shadow(x, y, w, h, r, 0.12f, 10);
    // 1. Frosted translucent base (allows wallpaper details to show through)
    Fill(x, y, w, h, r, C(0x0E1118, alpha));
    // 2. Subtle glass surface sheen: top ambient light reflection
    float sheenH = std::min(h * 0.45f, 96.f);
    if (sheenH > 4.f) {
        g_rt->PushAxisAlignedClip(R(x, y, w, h), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        Fill(x, y, w, sheenH, r, C(WHITE, 0.038f));
        g_rt->PopAxisAlignedClip();
    }
    // 3. Crisp optical glass rim (light catching the polished beveled edge)
    Stroke(x, y, w, h, r, C(WHITE, 0.13f), 1.0f);
    // 4. Razor-sharp specular highlight hairline along the top edge
    float inset = std::min(r * 0.65f, 18.f);
    Line(x + inset, y + 1.f, x + w - inset, y + 1.f, C(WHITE, 0.22f), 1.0f);
}


// ----------------------------------------------------------------------------
//  Text
// ----------------------------------------------------------------------------
static const wchar_t* F_UI   = L"Segoe UI";
static const wchar_t* F_SEMI = L"Segoe UI Semibold";
static const wchar_t* F_MONO = L"Consolas";
static const wchar_t* F_ICON = L"Segoe MDL2 Assets";

// align: 0 = leading, 1 = center, 2 = trailing
// Interface text size in percent. Only text smaller than 20 design px is scaled
// (labels, rows, captions); big display numerals and icon glyphs are left alone.
// Ctrl + / Ctrl - / Ctrl 0 change it at runtime.
static int   g_textPct = 115;
static const int TEXT_PCT_MIN = 90, TEXT_PCT_MAX = 150, TEXT_PCT_DEFAULT = 115;

static IDWriteTextFormat* Fmt(const wchar_t* font, float size, int weight, int align) {
    std::wstring key = std::wstring(font) + L"|" + std::to_wstring((int)(size * 10.f)) + L"|" +
                       std::to_wstring(weight) + L"|" + std::to_wstring(align);
    auto it = g_fmt.find(key);
    if (it != g_fmt.end()) return it->second;

    IDWriteTextFormat* f = nullptr;
    if (!g_dw) return nullptr;
    if (FAILED(g_dw->CreateTextFormat(font, nullptr, (DWRITE_FONT_WEIGHT)weight, DWRITE_FONT_STYLE_NORMAL,
                                      DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &f)))
        return nullptr;

    f->SetTextAlignment(align == 0 ? DWRITE_TEXT_ALIGNMENT_LEADING
                      : align == 1 ? DWRITE_TEXT_ALIGNMENT_CENTER
                                   : DWRITE_TEXT_ALIGNMENT_TRAILING);
    f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    IDWriteInlineObject* sign = nullptr;
    DWRITE_TRIMMING trim = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    if (SUCCEEDED(g_dw->CreateEllipsisTrimmingSign(f, &sign))) {
        f->SetTrimming(&trim, sign);
        sign->Release();
    }
    g_fmt[key] = f;
    return f;
}

static void Txt(const std::wstring& s, float x, float y, float w, float h,
                const wchar_t* font, float size, int weight, unsigned rgb,
                int align = 0, float alpha = 1.f) {
    if (s.empty()) return;
    if (size < 20.f && std::wcscmp(font, F_ICON) != 0) size *= (float)g_textPct / 100.f;
    IDWriteTextFormat* f = Fmt(font, size, weight, align);
    if (!f) return;
    g_br->SetColor(C(rgb, alpha));
    g_rt->DrawText(s.c_str(), (UINT32)s.size(), f, R(x, y, w, h), g_br,
                   D2D1_DRAW_TEXT_OPTIONS_CLIP, DWRITE_MEASURING_MODE_NATURAL);
}

// One MDL2 glyph centred on (cx, cy).
static void Glyph(wchar_t ch, float cx, float cy, float size, unsigned rgb, float alpha = 1.f) {
    if (!ch) return;
    std::wstring s(1, ch);
    Txt(s, cx - size, cy - size, size * 2.f, size * 2.f, F_ICON, size, 400, rgb, 1, alpha);
}

// ----------------------------------------------------------------------------
//  Animation: exponential approach, frame-rate independent and never
//  overshoots, which is what you want for panel slides and hover states.
// ----------------------------------------------------------------------------
struct Anim {
    float x = 0.f, target = 0.f;
    void Set(float v) { x = target = v; }
    void To(float t) { target = t; }
    bool Settled() const { return std::fabs(target - x) < 0.0008f; }
    float Step(float dt, float rate = 15.f) {
        if (dt <= 0.f) return x;
        x += (target - x) * (1.f - std::exp(-rate * dt));
        if (std::fabs(target - x) < 0.0008f) x = target;
        return x;
    }
};

// ----------------------------------------------------------------------------
//  Critically Damped Harmonic Spring (2nd-Order Physics)
//  Preserves exact pointer release velocity into a seamless glide.
//  Zero sudden stops, zero jerk, frame-rate independent closed-form ODE.
// ----------------------------------------------------------------------------
struct SpringAnim {
    float x = 0.f;
    float target = 0.f;
    float vel = 0.f;
    float omega = 13.5f; // natural angular frequency (silky OS-grade settling)

    void Set(float v) {
        x = target = v;
        vel = 0.f;
    }

    void To(float t) {
        target = t;
    }

    void ToWithVel(float t, float v) {
        target = t;
        vel = v;
    }

    bool Settled() const {
        return std::fabs(target - x) < 0.0004f && std::fabs(vel) < 0.001f;
    }

    float Step(float dt) {
        if (dt <= 0.f) return x;
        if (dt > 0.06f) dt = 0.06f; // clamp hitch
        float e0 = x - target;
        float decay = std::exp(-omega * dt);
        float c2 = vel + omega * e0;
        x = target + decay * (e0 + c2 * dt);
        vel = decay * (vel - omega * c2 * dt);
        if (Settled()) {
            x = target;
            vel = 0.f;
        }
        return x;
    }

    void Follow(float dest, float dt) {
        target = dest;
        float rate = 52.f; // very responsive subpixel follow
        x += (dest - x) * (1.f - std::exp(-rate * dt));
        vel = 0.f;
    }
};

// ----------------------------------------------------------------------------
//  Touch/Pointer Motion Predictor (Weighted Least-Squares Regression)
//  Filters sensor jitter & latency-compensates 1 frame ahead.
// ----------------------------------------------------------------------------
struct TouchPredictor {
    static const int MAX_SAMPLES = 16;
    struct Sample {
        float x;
        double t;
    };
    Sample history[MAX_SAMPLES];
    int count = 0;
    int head = 0;

    void Reset() {
        count = 0;
        head = 0;
    }

    void Add(float x, double t) {
        history[head] = { x, t };
        head = (head + 1) % MAX_SAMPLES;
        if (count < MAX_SAMPLES) count++;
    }

    float GetVelocity(double now, float* outPredictedX = nullptr, float leadSec = 0.018f) const {
        if (count < 2) {
            if (outPredictedX && count > 0) *outPredictedX = history[(head - 1 + MAX_SAMPLES) % MAX_SAMPLES].x;
            return 0.f;
        }

        const double WINDOW = 0.120; // 120ms rolling window
        float sumW = 0.f, sumWT = 0.f, sumWX = 0.f;
        float sumWTT = 0.f, sumWTX = 0.f;

        for (int i = 0; i < count; ++i) {
            int idx = (head - 1 - i + MAX_SAMPLES) % MAX_SAMPLES;
            double dt = now - history[idx].t;
            if (dt > WINDOW) break;

            float age = (float)(dt / WINDOW);
            float w = (1.f - age) * (1.f - age) + 0.04f;
            float tRel = (float)(history[idx].t - now);
            float xVal = history[idx].x;

            sumW += w;
            sumWT += w * tRel;
            sumWX += w * xVal;
            sumWTT += w * tRel * tRel;
            sumWTX += w * tRel * xVal;
        }

        if (sumW < 0.001f) return 0.f;
        float denom = sumW * sumWTT - sumWT * sumWT;
        if (std::fabs(denom) < 1e-6f) return 0.f;

        float vel = (sumW * sumWTX - sumWT * sumWX) / denom;
        int newestIdx = (head - 1 + MAX_SAMPLES) % MAX_SAMPLES;
        float curX = history[newestIdx].x;

        double idle = now - history[newestIdx].t;
        if (idle > 0.035) {
            float fade = (float)std::exp(-(idle - 0.035) * 45.0);
            vel *= fade;
        }

        if (outPredictedX) {
            *outPredictedX = curX + vel * leadSec;
        }
        return vel;
    }
};

static double g_clockT = 0.0;      // seconds since start, for blinking carets
static float  g_dt = 1.f / 60.f;
static double g_lastT = 0.0;
static double NowSec() {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

// ----------------------------------------------------------------------------
//  Application / window model
// ----------------------------------------------------------------------------
enum { APP_FILES = 0, APP_TERM, APP_SETTINGS, APP_EDITOR, APP_ABOUT, APP_COUNT };

struct Win {
    bool  open = false;
    bool  min = false;
    bool  maxed = false;
    float x = 0, y = 0, w = 0, h = 0;
    float rx = 0, ry = 0, rw = 0, rh = 0;       // restore geometry
    float scroll = 0.f;
    float targetScroll = 0.f;
    std::wstring title;
    std::wstring input;                          // terminal / editor line
    std::vector<std::wstring> lines;
};
static Win g_win[APP_COUNT];
static int g_focus = -1;
static std::vector<int> g_z;                     // bottom → top open windows

struct Toast { std::wstring text; wchar_t glyph; float t; };
static std::vector<Toast> g_toasts;

struct Notif { std::wstring app, title, body, when; wchar_t glyph; bool read; };
static std::vector<Notif> g_notifs;

struct Note { std::wstring text; bool done; };
static std::vector<Note> g_notes;

// ----------------------------------------------------------------------------
//  Shell state
// ----------------------------------------------------------------------------
enum { TOG_WIFI, TOG_BT, TOG_DND, TOG_NIGHT, TOG_SHARE, TOG_SAVER, TOG_COUNT };
static bool  g_tog[TOG_COUNT] = { true, false, false, false, true, false };
static const wchar_t* TOG_NAME[TOG_COUNT] = { L"Wi-Fi", L"Bluetooth", L"Do not disturb",
                                              L"Night light", L"Nearby share", L"Battery saver" };
static std::wstring   g_togSub[TOG_COUNT] = { L"Scanning...", L"Off", L"Muted alerts",
                                              L"Warm tint", L"Visible to contacts", L"Until 08:00" };

// Live Wi-Fi & Bluetooth variables
static std::wstring g_wifiSSID = L"Wi-Fi";
static int          g_wifiSignal = 0;

// Live Bitcoin state (real 24-point sparkline from Coinbase)
static std::wstring g_btcPriceStr = L"$80,592";
static std::wstring g_btcChangeStr = L"+0.00%";
static float        g_btcChangeVal = 0.f;
static float        g_btcSpark[24] = {
    80500.f, 80600.f, 80450.f, 80700.f, 80650.f, 80800.f, 80950.f, 80900.f,
    81050.f, 81000.f, 81150.f, 81300.f, 81200.f, 81400.f, 81350.f, 81500.f,
    81600.f, 81550.f, 81700.f, 81650.f, 81800.f, 81900.f, 81850.f, 82000.f
};
static std::mutex   g_btcLock;

// Live Weather state (from wttr.in)
static std::wstring g_weatherTempStr = L"--\u00B0";
static std::wstring g_weatherCond = L"Loading...";
static std::wstring g_weatherHighLow = L"H --\u00B0   L --\u00B0";
static std::wstring g_weatherCity = L"Detecting...";
static wchar_t      g_weatherGlyph = (wchar_t)0xE706;
static std::mutex   g_weatherLock;

// Background worker thread control
static std::atomic<bool> g_netRunning(true);
static std::thread       g_netThread;
static const wchar_t  TOG_GLYPH[TOG_COUNT] = { (wchar_t)0xE701, (wchar_t)0xE702, (wchar_t)0xE708,
                                               (wchar_t)0xE706, (wchar_t)0xE703, (wchar_t)0xE83F };

static float g_vol = 0.72f, g_bri = 0.85f;
static bool  g_playing = true;

// Live machine data, sampled from the OS once a second (see SampleSystem).
static int          g_battPct = 78;
static bool         g_acLine = true;
static bool         g_hasBatt = true;
static float        g_cpu = 0.18f, g_ram = 0.55f, g_disk = 0.46f;
static std::wstring g_uptime = L"up 0h 0m";
static ULONGLONG    g_prevIdle = 0, g_prevKernel = 0, g_prevUser = 0;
static float g_pos = 84.f, g_dur = 228.f;      // media position seconds
static int   g_wp = 0;                          // wallpaper preset index
static std::wstring g_customWpPath = L"wallpaper_glass.jpg";

// Desktop widgets: which ones are shown, and how large. Both live in the
// Appearance page of Settings and are persisted.
static bool  g_widgetOn[shelllayout::W_COUNT] = { true, true, true, true,
                                                  true, true, true, true };
static int   g_sizeIdx = 1;                     // index into shelllayout::Scales()
static float g_widgetScale = 1.f;

// General preferences surfaced in Settings.
static bool  g_clock24 = false;                 // 12-hour clock by default
static bool  g_motion = true;                   // wallpaper drift + waveform
static int   g_setPage = 0;                     // Settings: 0 System, 1 Appearance, 2 Advanced

enum { PANEL_NONE = 0, PANEL_SYS, PANEL_LAUNCH };
static int  g_panel = PANEL_NONE;
static Anim g_sysA, g_launchA, g_lockA, g_ctxA, g_wpFade;

// ---- Home pager (mobile-style swipe between home screens) -------------------
// Page 0 = widgets, page 1 = app grid. g_pageA.x is the live position in pages
// (0.0 .. HOME_PAGES-1); it follows the pointer while dragging and springs to
// g_page on release.
static const int HOME_PAGES = 2;
static int        g_page = 0;                   // page we are settling on
static SpringAnim g_pageA;                      // 2nd-order spring position
static TouchPredictor g_tracker;                // subpixel motion & velocity tracker
static bool       g_pdown = false;              // pointer pressed on the home screen
static bool       g_pdrag = false;              // ... and has become a swipe
static int        g_pdid = 0;                   // hit id under the press
static float      g_pdx0 = 0.f, g_pdy0 = 0.f;   // press point (design space)
static float      g_pstartPos = 0.f;            // g_pageA.x at press
static float      g_pageDX = 0.f;               // x offset of the page being drawn

// Side Screen (All Apps) down-scroll physics state (smooth-scrollbar momentum)
static float g_appScroll = 0.f;
static float g_appTargetScroll = 0.f;
static float g_appMaxScroll = 0.f;
static bool  g_appVDrag = false;
static float g_appDragStartY = 0.f;
static float g_appDragStartScroll = 0.f;

// Two-finger touchpad swipe (WM_MOUSEHWHEEL).
static const float HSWIPE_DIR = 1.f;
static bool       g_hg = false;
static bool       g_hlock = false;
static float      g_hacc = 0.f;
static int        g_hbase = 0;
static double     g_hlastT = 0.0;

static void GoPage(int p, float initialVel = 0.f) {
    g_page = p < 0 ? 0 : (p > HOME_PAGES - 1 ? HOME_PAGES - 1 : p);
    g_pageA.ToWithVel((float)g_page, initialVel);
}
static int  g_launchPage = 0;
static int  g_launchSel = -1;
static bool g_ctxOpen = false;
static float g_ctxX = 0, g_ctxY = 0;
static int  g_ctxHover = -1;
static bool g_locked = false;
static bool g_full = false;
static WINDOWPLACEMENT g_wpPlace = {};
static LONG g_savedStyle = 0;

// Drag bookkeeping
static int   g_dragWin = -1, g_dragPart = 0;
static float g_sw = 0, g_sh = 0;                // geometry at drag start
static int   g_dragSlider = -1;                 // 0 volume, 1 brightness, 2 seek

// ----------------------------------------------------------------------------
//  Hit testing. Rectangles are registered while drawing; mouse routing uses
//  the previous frame's list, which is exact for a UI this static.
// ----------------------------------------------------------------------------
enum {
    H_NONE = 0,
    H_TRAY = 10, H_CLOCK, H_DND, H_SHELF_BG,
    H_TOG0 = 20,                             // + TOG_COUNT
    H_VOL = 40, H_BRI, H_LOCK, H_SETTINGS, H_POWER,
    H_NOTIF = 60,                            // + 100*i body, +50+i dismiss
    H_NOTE = 300,                            // + index
    H_ADDNOTE = 320,
    H_PREV = 340, H_PLAY, H_NEXT, H_SEEK, H_SHUFFLE, H_REPEAT,
    H_LAUNCHER = 360, H_LAUNCH_SEARCH = 361, H_LAUNCH_CLOSE = 362,
    H_APP = 400,                             // + index (launcher grid)
    H_PAGE = 500,                            // + page
    H_SHELF = 600,                           // + index (shelf icons)
    H_WIN = 700,                             // + kind*8 + part
    H_SET_NAV = 760,                         // + page          (Settings tabs)
    H_SET_WP = 770,                          // + preset        (wallpaper swatches)
    H_SET_CUSTOM_WP = 778,                   // choose custom image button
    H_SET_SIZE = 780,                        // + index         (widget size)
    H_SET_WIDGET = 785,                      // + widget        (visibility)
    H_SET_MISC = 796,                        // + 0 clock, +1 motion
    H_SET_RESET = 799,
    H_CTX = 800,                             // + item
    H_UNLOCK = 850, H_FILE = 870,            // + row
    H_DESK = 900,
    H_HAPP = 1000,                           // + index (home-screen app tiles)
    H_HDOT = 1200                            // + page  (home page dots)
};
enum { WP_BODY = 0, WP_TITLE = 1, WP_CLOSE = 2, WP_MIN = 3, WP_MAX = 4,
       WP_RIGHT = 5, WP_BOTTOM = 6, WP_CORNER = 7 };

struct Hit { float x, y, w, h; int id; };
static std::vector<Hit> g_hits;
static int g_hover = H_NONE;

// Widgets are drawn through a Direct2D scale/translate transform when the user
// picks a widget size, so their hit rectangles are registered in local widget
// space and pushed through the matching transform here. Outside a widget the
// transform is the identity, making this a no-op for the rest of the shell.
static shelllayout::Xform g_hitX;

static void AddHit(float x, float y, float w, float h, int id) {
    shelllayout::MapRect(x, y, w, h, g_hitX);
    g_hits.push_back({ x, y, w, h, id });
}
static int HitTest(float mx, float my) {
    for (int i = (int)g_hits.size() - 1; i >= 0; --i) {
        const Hit& h = g_hits[i];
        if (mx >= h.x && mx <= h.x + h.w && my >= h.y && my <= h.y + h.h) return h.id;
    }
    return H_NONE;
}

// ----------------------------------------------------------------------------
//  Data
// ----------------------------------------------------------------------------
struct AppDef {
    const wchar_t* name;
    wchar_t        glyph;
    const wchar_t* mono;
    int            kind;
    const wchar_t* exec;
    const wchar_t* fallback;
};
// glyph == 0 → draw `mono` in Consolas instead of an MDL2 glyph.
static const AppDef APPS[] = {
    // Core Desktop System Windows
    { L"Files",       0xE8B7, nullptr, APP_FILES,    L"explorer.exe",                nullptr },
    { L"Terminal",    0,      L">_",   APP_TERM,     L"wt.exe",                      L"powershell.exe" },
    { L"Settings",    0xE713, nullptr, APP_SETTINGS, L"ms-settings:",               nullptr },
    { L"Text Editor", 0xE70F, nullptr, APP_EDITOR,   L"notepad.exe",                 nullptr },
    { L"About",       0xE946, nullptr, APP_ABOUT,    nullptr,                        nullptr },

    // Primary Productivity & Media
    { L"Chrome",      0xE774, nullptr, -1,           L"chrome.exe",                  L"https://www.google.com" },
    { L"Gmail",       0xE715, nullptr, -1,           L"https://mail.google.com",     L"mailto:" },
    { L"Calendar",    0xE787, nullptr, -1,           L"outlookcal:",                 L"https://calendar.google.com" },
    { L"Photos",      0xE91B, nullptr, -1,           L"ms-photos:",                  L"explorer.exe shell:My Pictures" },
    { L"Play Music",  0xE8D6, nullptr, -1,           L"spotify:",                    L"https://music.youtube.com" },
    { L"Camera",      0xE722, nullptr, -1,           L"microsoft.windows.camera:",   nullptr },
    { L"Maps",        0xE707, nullptr, -1,           L"bingmaps:",                   L"https://maps.google.com" },
    { L"Weather",     0xE706, nullptr, -1,           L"bingweather:",                L"https://weather.com" },
    { L"Clock",       0xE823, nullptr, -1,           L"ms-clock:",                   nullptr },
    { L"Store",       0xE719, nullptr, -1,           L"ms-windows-store:",           nullptr },
    { L"Drive",       0xE753, nullptr, -1,           L"https://drive.google.com",    nullptr },
    { L"Docs",        0xE8A5, nullptr, -1,           L"https://docs.google.com",     nullptr },
    { L"Calculator",  0,      L"1+2",  -1,           L"calc.exe",                    L"calculator:" },
    { L"Notes",       0xE70F, nullptr, -1,           L"onenote:",                    L"notepad.exe" },
    { L"Meet",        0xE77B, nullptr, -1,           L"https://meet.google.com",     nullptr },
    { L"Chat",        0xE8BD, nullptr, -1,           L"https://chat.google.com",     L"ms-teams:" },
    { L"Tasks",       0xE9D5, nullptr, -1,           L"ms-todo:",                    L"https://tasks.google.com" },
    { L"Files Pro",   0xE8B7, nullptr, -1,           L"explorer.exe shell:Personal", L"explorer.exe" },
    { L"Help",        0xE897, nullptr, -1,           L"ms-contact-support:",         L"https://support.microsoft.com/windows" },

    // Expanded Real Windows Tools, Utilities & Workspaces (All Apps with Down-Scroll)
    { L"Task Manager",0xE950, nullptr, -1,           L"taskmgr.exe",                 nullptr },
    { L"Snipping Tool",0xE7C8,nullptr, -1,           L"ms-screenclip:",              L"snippingtool.exe" },
    { L"Paint",       0xE790, nullptr, -1,           L"mspaint.exe",                 nullptr },
    { L"Edge",        0xE777, nullptr, -1,           L"msedge.exe",                  L"https://www.bing.com" },
    { L"VS Code",     0,      L"{ }",  -1,           L"code.cmd",                    L"code.exe" },
    { L"Command Prompt",0,    L"cmd",  -1,           L"cmd.exe",                     L"powershell.exe" },
    { L"Control Panel",0xE771,nullptr, -1,           L"control.exe",                 nullptr },
    { L"Device Manager",0xE7F8,nullptr,-1,           L"devmgmt.msc",                 nullptr },
    { L"Disk Manager",0xEDA2, nullptr, -1,           L"diskmgmt.msc",                nullptr },
    { L"Res Monitor", 0xE9E9, nullptr, -1,           L"resmon.exe",                  nullptr },
    { L"System Info", 0xE7BA, nullptr, -1,           L"msinfo32.exe",                nullptr },
    { L"Registry",    0xE7AC, nullptr, -1,           L"regedit.exe",                 nullptr },
    { L"Disk Cleanup",0xE74D, nullptr, -1,           L"cleanmgr.exe",                nullptr },
    { L"Sound Recorder",0xE720,nullptr,-1,           L"soundrecorder:",              nullptr },
    { L"Media Player",0xE768, nullptr, -1,           L"wmplayer.exe",                L"mswindowsmusic:" },
    { L"Sheets",      0,      L"[=]",  -1,           L"https://sheets.google.com",   nullptr },
    { L"Discord",     0xE714, nullptr, -1,           L"discord:",                    L"https://discord.com/app" },
    { L"GitHub",      0,      L"git",  -1,           L"https://github.com",          nullptr },
    { L"YouTube",     0xE714, nullptr, -1,           L"https://www.youtube.com",     nullptr },
    { L"Reddit",      0xE789, nullptr, -1,           L"https://www.reddit.com",      nullptr },
    { L"Wikipedia",   0,      L"W",    -1,           L"https://www.wikipedia.org",   nullptr },
    { L"Char Map",    0xE890, nullptr, -1,           L"charmap.exe",                 nullptr },
    { L"Magnifier",   0xE721, nullptr, -1,           L"magnify.exe",                 nullptr },
    { L"Power Plan",  0xE7E8, nullptr, -1,           L"powercfg.cpl",                nullptr },
};
static const int APP_N = (int)(sizeof(APPS) / sizeof(APPS[0]));

struct FileRow { const wchar_t* name; const wchar_t* size; const wchar_t* when; wchar_t glyph; bool folder; };
static const FileRow FILES[] = {
    { L"Projects",        L"—",        L"Today, 09:41", 0xE8B7, true  },
    { L"Design",          L"—",        L"Yesterday",    0xE8B7, true  },
    { L"obsidian_shell.cpp", L"128 KB", L"Today, 11:02", 0xE8A5, false },
    { L"palette.png",     L"2.4 MB",   L"Mon 14:20",    0xE91B, false },
    { L"notes.md",        L"6 KB",     L"Sun 18:05",    0xE8A5, false },
    { L"render.mp4",      L"48 MB",    L"Sat 12:33",    0xE8B2, false },
    { L"archive.zip",     L"12 MB",    L"Fri 08:12",    0xE8A5, false },
};
static const int FILE_N = (int)(sizeof(FILES) / sizeof(FILES[0]));

// 24-point BTC sparkline, normalised 0..1 later.
static const float BTC[24] = {
    0.32f, 0.36f, 0.34f, 0.41f, 0.39f, 0.44f, 0.52f, 0.49f, 0.55f, 0.51f, 0.58f, 0.62f,
    0.59f, 0.64f, 0.61f, 0.68f, 0.72f, 0.69f, 0.74f, 0.71f, 0.78f, 0.81f, 0.79f, 0.86f,
};

// ----------------------------------------------------------------------------
//  Settings persistence — %LOCALAPPDATA%\ObsidianShell\shell.ini
//  GetPrivateProfile* keeps this dependency-free and locale-safe.
// ----------------------------------------------------------------------------
static std::wstring SettingsPath() {
    wchar_t buf[MAX_PATH] = { 0 };
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    std::wstring dir = (n > 0 && n < MAX_PATH) ? std::wstring(buf, n) : std::wstring(L".");
    dir += L"\\ObsidianShell";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\shell.ini";
}

static void SaveSettings() {
    std::wstring p = SettingsPath();
    wchar_t num[32];
    swprintf(num, 32, L"%d", (int)std::lround(g_vol * 100.f));
    WritePrivateProfileStringW(L"shell", L"vol", num, p.c_str());
    swprintf(num, 32, L"%d", (int)std::lround(g_bri * 100.f));
    WritePrivateProfileStringW(L"shell", L"bri", num, p.c_str());
    swprintf(num, 32, L"%d", g_wp);
    WritePrivateProfileStringW(L"shell", L"wallpaper", num, p.c_str());
    WritePrivateProfileStringW(L"shell", L"custom_wp", g_customWpPath.c_str(), p.c_str());
    for (int i = 0; i < TOG_COUNT; ++i) {
        wchar_t key[16];
        swprintf(key, 16, L"tog%d", i);
        WritePrivateProfileStringW(L"shell", key, g_tog[i] ? L"1" : L"0", p.c_str());
    }
    swprintf(num, 32, L"%d", g_sizeIdx);
    WritePrivateProfileStringW(L"shell", L"widgetsize", num, p.c_str());
    swprintf(num, 32, L"%d", g_textPct);
    WritePrivateProfileStringW(L"shell", L"textpct", num, p.c_str());
    for (int i = 0; i < shelllayout::W_COUNT; ++i) {
        wchar_t key[16];
        swprintf(key, 16, L"w%d", i);
        WritePrivateProfileStringW(L"shell", key, g_widgetOn[i] ? L"1" : L"0", p.c_str());
    }
    WritePrivateProfileStringW(L"shell", L"clock24", g_clock24 ? L"1" : L"0", p.c_str());
    WritePrivateProfileStringW(L"shell", L"motion", g_motion ? L"1" : L"0", p.c_str());
}

static void LoadSettings() {
    std::wstring p = SettingsPath();
    g_vol = Clamp(GetPrivateProfileIntW(L"shell", L"vol", 72, p.c_str()) / 100.f, 0.f, 1.f);
    g_bri = Clamp(GetPrivateProfileIntW(L"shell", L"bri", 85, p.c_str()) / 100.f, 0.f, 1.f);
    wchar_t cwp[MAX_PATH] = L"";
    GetPrivateProfileStringW(L"shell", L"custom_wp", L"wallpaper_glass.jpg", cwp, MAX_PATH, p.c_str());
    g_customWpPath = cwp;
    g_wp  = GetPrivateProfileIntW(L"shell", L"wallpaper", 0, p.c_str()) % WP_COUNT;
    if (g_wp < 0) g_wp = 0;
    for (int i = 0; i < TOG_COUNT; ++i) {
        wchar_t key[16];
        swprintf(key, 16, L"tog%d", i);
        g_tog[i] = GetPrivateProfileIntW(L"shell", key, g_tog[i] ? 1 : 0, p.c_str()) != 0;
    }

    int ns = 0;
    const float* scales = shelllayout::Scales(ns);
    g_sizeIdx = GetPrivateProfileIntW(L"shell", L"widgetsize", 1, p.c_str());
    if (g_sizeIdx < 0 || g_sizeIdx >= ns) g_sizeIdx = 1;
    g_widgetScale = scales[g_sizeIdx];
    g_textPct = GetPrivateProfileIntW(L"shell", L"textpct", TEXT_PCT_DEFAULT, p.c_str());
    if (g_textPct < TEXT_PCT_MIN || g_textPct > TEXT_PCT_MAX) g_textPct = TEXT_PCT_DEFAULT;

    for (int i = 0; i < shelllayout::W_COUNT; ++i) {
        wchar_t key[16];
        swprintf(key, 16, L"w%d", i);
        g_widgetOn[i] = GetPrivateProfileIntW(L"shell", key, 1, p.c_str()) != 0;
    }
    g_clock24 = GetPrivateProfileIntW(L"shell", L"clock24", 0, p.c_str()) != 0;
    g_motion  = GetPrivateProfileIntW(L"shell", L"motion", 1, p.c_str()) != 0;
}

// ----------------------------------------------------------------------------
//  Small formatting helpers
// ----------------------------------------------------------------------------
static std::wstring Pad2(int v) { return (v < 10 ? L"0" : L"") + std::to_wstring(v); }
static std::wstring TimeHM() {
    SYSTEMTIME st; GetLocalTime(&st);
    if (g_clock24) return Pad2(st.wHour) + L":" + Pad2(st.wMinute);
    int h = st.wHour % 12; if (h == 0) h = 12;
    return std::to_wstring(h) + L":" + Pad2(st.wMinute);
}
static std::wstring TimeHMS() {
    SYSTEMTIME st; GetLocalTime(&st);
    if (g_clock24) return Pad2(st.wHour) + L":" + Pad2(st.wMinute) + L":" + Pad2(st.wSecond);
    int h = st.wHour % 12; if (h == 0) h = 12;
    return std::to_wstring(h) + L":" + Pad2(st.wMinute) + L":" + Pad2(st.wSecond);
}
static std::wstring DateLong() {
    static const wchar_t* WD[7] = { L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday" };
    static const wchar_t* MO[12] = { L"January", L"February", L"March", L"April", L"May", L"June",
                                     L"July", L"August", L"September", L"October", L"November", L"December" };
    SYSTEMTIME st; GetLocalTime(&st);
    int wd = st.wDayOfWeek % 7, mo = (st.wMonth >= 1 && st.wMonth <= 12) ? st.wMonth - 1 : 0;
    return std::wstring(WD[wd]) + L", " + std::to_wstring(st.wDay) + L" " + MO[mo];
}
static std::wstring DateShort() {
    static const wchar_t* WD[7] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
    static const wchar_t* MO[12] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
                                     L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };
    SYSTEMTIME st; GetLocalTime(&st);
    int wd = st.wDayOfWeek % 7, mo = (st.wMonth >= 1 && st.wMonth <= 12) ? st.wMonth - 1 : 0;
    return std::wstring(WD[wd]) + L", " + std::to_wstring(st.wDay) + L" " + MO[mo];
}
static std::wstring MediaTime(float sec) {
    int s = (int)sec; if (s < 0) s = 0;
    return std::to_wstring(s / 60) + L":" + Pad2(s % 60);
}

// ----------------------------------------------------------------------------
//  Forward declarations for things defined in later sections
// ----------------------------------------------------------------------------
static void UpdateAnimations(float dt);
static void Render();
static void OpenApp(int kind);
static void LaunchRealApp(const AppDef& app);
static void ShowToast(const std::wstring& text, wchar_t glyph);
static void ClosePanels();
static void SetWallpaper(int idx);
static void BuildWallpaperBrushes();
static void ToggleFullscreen(HWND h);
static void FocusWindow(int kind);
static void ResetWindow(int kind);
// ============================================================================
//  Section 2 — reusable surfaces, the wallpaper engine and the desk widgets
// ============================================================================

static ID2D1LinearGradientBrush* g_scrim = nullptr;   // bottom scrim for legibility

static IWICImagingFactory* g_wicFactory = nullptr;
static ID2D1Bitmap*        g_wpBitmap = nullptr;
static ID2D1Bitmap*        g_wpOldBitmap = nullptr;

static ID2D1Bitmap* LoadBitmapFromFile(const wchar_t* path) {
    if (!g_rt || !path || !path[0]) return nullptr;
    if (!g_wicFactory) {
        CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_wicFactory));
    }
    if (!g_wicFactory) return nullptr;

    std::wstring fullPath = path;
    if (GetFileAttributesW(fullPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        wchar_t exePath[MAX_PATH] = L"";
        if (GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
            wchar_t* lastSlash = wcsrchr(exePath, L'\\');
            if (lastSlash) {
                *(lastSlash + 1) = L'\0';
                std::wstring candidate = std::wstring(exePath) + path;
                if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    fullPath = candidate;
                }
            }
        }
    }

    IWICBitmapDecoder* pDecoder = nullptr;
    HRESULT hr = g_wicFactory->CreateDecoderFromFilename(fullPath.c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr) || !pDecoder) return nullptr;

    IWICBitmapFrameDecode* pSource = nullptr;
    hr = pDecoder->GetFrame(0, &pSource);
    if (FAILED(hr) || !pSource) { pDecoder->Release(); return nullptr; }

    IWICFormatConverter* pConverter = nullptr;
    hr = g_wicFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr) || !pConverter) { pSource->Release(); pDecoder->Release(); return nullptr; }

    hr = pConverter->Initialize(pSource, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr)) { pConverter->Release(); pSource->Release(); pDecoder->Release(); return nullptr; }

    ID2D1Bitmap* pBitmap = nullptr;
    hr = g_rt->CreateBitmapFromWicBitmap(pConverter, NULL, &pBitmap);

    pConverter->Release();
    pSource->Release();
    pDecoder->Release();
    return pBitmap;
}

static void PromptCustomWallpaper(HWND hwnd);
static void SetCustomWallpaper(const std::wstring& path);


// ---- gradient brushes -------------------------------------------------------
static ID2D1LinearGradientBrush* MakeLinear(const D2D1_GRADIENT_STOP* stops, UINT32 n,
                                           D2D1_POINT_2F p0, D2D1_POINT_2F p1) {
    if (!g_rt) return nullptr;
    ID2D1GradientStopCollection* col = nullptr;
    if (FAILED(g_rt->CreateGradientStopCollection(stops, n, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &col)) || !col)
        return nullptr;
    ID2D1LinearGradientBrush* b = nullptr;
    g_rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(p0, p1), col, &b);
    col->Release();
    return b;
}

static void BuildWallpaperBrushes() {
    const WpPreset& p = WP[g_wp % WP_COUNT];

    // Transition bitmap
    SafeRelease(g_wpOldBitmap);
    g_wpOldBitmap = g_wpBitmap;
    g_wpBitmap = nullptr;

    const wchar_t* imgPath = nullptr;
    if (p.file && p.file[0]) {
        imgPath = p.file;
    } else if (g_wp == WP_COUNT - 1 && !g_customWpPath.empty()) {
        imgPath = g_customWpPath.c_str();
    }

    if (imgPath) {
        g_wpBitmap = LoadBitmapFromFile(imgPath);
    }

    D2D1_GRADIENT_STOP st[3] = {
        { 0.00f, C(p.a) },
        { 0.46f, C(p.b) },
        { 1.00f, C(p.c) },
    };
    SafeRelease(g_wpA);
    g_wpA = MakeLinear(st, 3, D2D1::Point2F(0.f, 0.f), D2D1::Point2F(DW * 0.30f, DH));

    SafeRelease(g_glow);
    if (g_rt) {
        D2D1_GRADIENT_STOP gs[2] = { { 0.f, C(p.glow, 0.38f) }, { 1.f, C(p.glow, 0.f) } };
        ID2D1GradientStopCollection* col = nullptr;
        if (SUCCEEDED(g_rt->CreateGradientStopCollection(gs, 2, D2D1_GAMMA_2_2,
                                                        D2D1_EXTEND_MODE_CLAMP, &col)) && col) {
            g_rt->CreateRadialGradientBrush(
                D2D1::RadialGradientBrushProperties(D2D1::Point2F(DW * 0.66f, DH * 0.16f),
                                                    D2D1::Point2F(0.f, 0.f), 980.f, 720.f),
                col, &g_glow);
            col->Release();
        }
    }

    if (!g_scrim) {
        D2D1_GRADIENT_STOP ss[2] = { { 0.f, C(BLACK, 0.f) }, { 1.f, C(BLACK, 0.42f) } };
        g_scrim = MakeLinear(ss, 2, D2D1::Point2F(0.f, DH - 320.f), D2D1::Point2F(0.f, DH));
    }
}

static void SetWallpaper(int idx) {
    idx %= WP_COUNT;
    if (idx < 0) idx += WP_COUNT;
    if (idx == WP_COUNT - 1 && g_customWpPath.empty()) {
        PromptCustomWallpaper(g_hwnd);
        return;
    }
    if (idx == g_wp && g_wpBitmap) return;
    g_wp = idx;
    SaveSettings();

    SafeRelease(g_wpB);
    g_wpB = g_wpA;
    g_wpA = nullptr;
    BuildWallpaperBrushes();

    g_wpFade.Set(0.f);
    g_wpFade.To(1.f);
    ShowToast(std::wstring(L"Wallpaper: ") + WP[g_wp].name, (wchar_t)0xE790);
}

static void SetCustomWallpaper(const std::wstring& path) {
    if (path.empty()) return;
    g_customWpPath = path;
    g_wp = WP_COUNT - 1;
    SaveSettings();

    SafeRelease(g_wpB);
    g_wpB = g_wpA;
    g_wpA = nullptr;
    BuildWallpaperBrushes();

    g_wpFade.Set(0.f);
    g_wpFade.To(1.f);
    ShowToast(L"Loaded Custom Wallpaper", (wchar_t)0xE790);
}

static void PromptCustomWallpaper(HWND hwnd) {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Image Files (*.jpg;*.jpeg;*.png;*.bmp;*.webp)\0*.jpg;*.jpeg;*.png;*.bmp;*.webp\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Select Custom Desktop Wallpaper";
    if (GetOpenFileNameW(&ofn)) {
        SetCustomWallpaper(filename);
    }
}

static void DrawWallpaper() {
    // Base deep dark canvas
    g_br->SetColor(C(0x06080A));
    g_rt->FillRectangle(R(0.f, 0.f, DW, DH), g_br);

    if (g_wpBitmap) {
        D2D1_SIZE_F sz = g_wpBitmap->GetSize();
        if (sz.width > 0.f && sz.height > 0.f) {
            float scale = std::max(DW / sz.width, DH / sz.height);
            float drawW = sz.width * scale;
            float drawH = sz.height * scale;
            float drawX = (DW - drawW) * 0.5f;
            float drawY = (DH - drawH) * 0.5f;

            float opacity = g_wpFade.x;
            if (g_wpOldBitmap && opacity < 0.999f) {
                D2D1_SIZE_F osz = g_wpOldBitmap->GetSize();
                float oScale = std::max(DW / osz.width, DH / osz.height);
                float oW = osz.width * oScale, oH = osz.height * oScale;
                g_rt->DrawBitmap(g_wpOldBitmap, R((DW - oW)*0.5f, (DH - oH)*0.5f, oW, oH), 1.0f - opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            }
            g_rt->DrawBitmap(g_wpBitmap, R(drawX, drawY, drawW, drawH), opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

            // Dark ambient atmospheric scrim for high contrast and text readability
            g_br->SetColor(C(0x06080C, 0.32f));
            g_rt->FillRectangle(R(0.f, 0.f, DW, DH), g_br);
        }
    } else {
        if (g_wpB && g_wpFade.x < 0.999f) {
            g_wpB->SetOpacity(1.f - g_wpFade.x);
            g_rt->FillRectangle(R(0.f, 0.f, DW, DH), g_wpB);
        }
        if (g_wpA) {
            g_wpA->SetOpacity(g_wpFade.x);
            g_rt->FillRectangle(R(0.f, 0.f, DW, DH), g_wpA);
        }
    }

    if (g_glow) {
        float t = g_motion ? (float)g_clockT : 0.f;
        g_glow->SetCenter(D2D1::Point2F(DW * 0.66f - g_pageA.x * 160.f + std::sin(t * 0.06f) * 90.f,
                                        DH * 0.16f + std::cos(t * 0.045f) * 46.f));
        g_glow->SetOpacity(g_wpBitmap ? 0.32f : 0.55f);
        g_rt->FillRectangle(R(0.f, 0.f, DW, DH), g_glow);
        g_glow->SetOpacity(1.f);
    }

    // Faint horizontal hairlines give the wallpaper glass texture
    for (int i = 1; i < 9; ++i) {
        float y = (DH / 9.f) * (float)i;
        Line(0.f, y, DW, y, C(WHITE, 0.012f));
    }
}

// ----------------------------------------------------------------------------
//  Reusable primitives
// ----------------------------------------------------------------------------
static void Label(const std::wstring& s, float x, float y, float w, unsigned rgb = COL_T35, int align = 0) {
    Txt(s, x, y, w, 18.f, F_UI, 11.f, 600, rgb, align, 0.95f);
}

static void Meter(float x, float y, float w, float v, unsigned rgb) {
    Fill(x, y, w, 6.f, 3.f, C(WHITE, 0.10f));
    Fill(x, y, w * Clamp(v, 0.f, 1.f), 6.f, 3.f, C(rgb, 0.95f));
}

static void Sparkline(float x, float y, float w, float h, const float* v, int n, unsigned rgb) {
    if (n < 2) return;
    float lo = v[0], hi = v[0];
    for (int i = 1; i < n; ++i) { lo = std::min(lo, v[i]); hi = std::max(hi, v[i]); }
    float span = (hi - lo) < 0.001f ? 1.f : (hi - lo);
    float px = 0.f, py = 0.f;
    for (int i = 0; i < n; ++i) {
        float cx = x + w * (float)i / (float)(n - 1);
        float cy = y + h - h * (v[i] - lo) / span;
        if (i) {
            Line(px, py, cx, cy, C(rgb, 0.85f), 2.f);
            // soft glow under the stroke
            Line(px, py + 1.5f, cx, cy + 1.5f, C(rgb, 0.16f), 3.f);
        }
        px = cx; py = cy;
    }
    Disc(px, py, 3.2f, C(rgb, 0.95f));
    Disc(px, py, 7.f, C(rgb, 0.14f));
}

static void Checkbox(float x, float y, float s, bool on, bool hov) {
    Fill(x, y, s, s, 5.f, on ? C(COL_ACC, 0.95f) : C(WHITE, hov ? 0.10f : 0.05f));
    if (!on) Stroke(x, y, s, s, 5.f, C(WHITE, hov ? 0.34f : 0.20f));
    else Glyph((wchar_t)0xE73E, x + s * 0.5f, y + s * 0.5f, s * 0.58f, 0x0B0B0D);
}

// Quick-settings toggle tile
static void ToggleTile(float x, float y, float w, float h, int i) {
    bool on = g_tog[i];
    bool hov = (g_hover == H_TOG0 + i);
    if (on) {
        Fill(x, y, w, h, 16.f, C(COL_T100, hov ? 0.98f : 0.93f));
    } else {
        Fill(x, y, w, h, 16.f, hov ? C(COL_TILE_HI, 0.98f) : C(COL_TILE, 0.90f));
        Stroke(x, y, w, h, 16.f, C(WHITE, hov ? 0.11f : 0.05f));
    }
    unsigned fg = on ? 0x101012 : COL_T90;
    float cy = y + 26.f;
    Disc(x + 32.f, cy, 15.f, on ? C(BLACK, 0.07f) : C(WHITE, 0.06f));
    Glyph(TOG_GLYPH[i], x + 32.f, cy, 15.f, fg, 0.95f);
    Txt(TOG_NAME[i], x + 56.f, y + 12.f, w - 66.f, 28.f, F_SEMI, 12.5f, 600, on ? 0x101012 : COL_T90, 0, 1.f);
    Txt(g_togSub[i].c_str(), x + 56.f, y + 34.f, w - 66.f, 22.f, F_UI, 10.5f, 400, on ? 0x4A4A50 : COL_T35, 0, 1.f);
    AddHit(x, y, w, h, H_TOG0 + i);
}

// Slider with knob; registers its own hit region.
static void SliderDraw(float x, float y, float w, float v, int id, float knob = 8.f) {
    const float cy = y + 16.f;
    Fill(x, cy - 2.5f, w, 5.f, 2.5f, C(WHITE, 0.13f));
    Fill(x, cy - 2.5f, w * Clamp(v, 0.f, 1.f), 5.f, 2.5f, C(COL_T80, 0.92f));
    float kx = x + w * Clamp(v, 0.f, 1.f);
    if (g_hover == id) Disc(kx, cy, knob + 5.f, C(WHITE, 0.10f));
    Disc(kx, cy, knob, C(COL_T100, 0.97f));
    AddHit(x - 8.f, y, w + 16.f, 32.f, id);
}

static void RingButton(float cx, float cy, float r, int id, wchar_t glyph, float size,
                       bool primary = false) {
    bool hov = (g_hover == id);
    if (primary) Disc(cx, cy, r, C(COL_ACTIVE, hov ? 1.f : 0.93f));
    else {
        if (hov) Disc(cx, cy, r, C(WHITE, 0.08f));
        Ring(cx, cy, r, C(WHITE, hov ? 0.16f : 0.08f), 1.f);
    }
    Glyph(glyph, cx, cy, size, primary ? 0x0B0B0D : COL_T90, 0.95f);
    AddHit(cx - r, cy - r, r * 2.f, r * 2.f, id);
}

// ----------------------------------------------------------------------------
//  Desk widget: clock
// ----------------------------------------------------------------------------
static void DrawClockCard(float x, float y, float w, float h) {
    Card(x, y, w, h, 24.f, 0.78f);
    Label(L"LOCAL TIME", x + 28.f, y + 22.f, w - 56.f);
    Txt(TimeHM(), x + 26.f, y + 44.f, w - 140.f, 88.f, F_SEMI, 62.f, 600, COL_T100, 0, 1.f);
    Txt(DateLong(), x + 28.f, y + 132.f, w - 56.f, 28.f, F_UI, 14.f, 400, COL_T50, 0, 1.f);

    // seconds ring
    SYSTEMTIME st; GetLocalTime(&st);
    float cx = x + w - 66.f, cy = y + h * 0.5f;
    Ring(cx, cy, 27.f, C(WHITE, 0.07f), 3.f);
    float a0 = -1.5707963f;
    float a1 = a0 + 6.2831853f * ((float)st.wSecond / 60.f);
    const int seg = 34;
    for (int i = 0; i < seg; ++i) {
        float t0 = a0 + (a1 - a0) * (float)i / seg;
        float t1 = a0 + (a1 - a0) * (float)(i + 1) / seg;
        Line(cx + std::cos(t0) * 27.f, cy + std::sin(t0) * 27.f,
             cx + std::cos(t1) * 27.f, cy + std::sin(t1) * 27.f, C(COL_T100, 0.85f), 3.f);
    }
    Disc(cx, cy, 3.f, C(COL_T100, 0.9f));
    Txt(Pad2(st.wSecond), cx - 20.f, cy + 32.f, 40.f, 18.f, F_MONO, 11.f, 400, COL_T50, 1, 0.9f);
}

// ----------------------------------------------------------------------------
//  Desk widget: weather
// ----------------------------------------------------------------------------
static void DrawWeather(float x, float y, float w, float h) {
    Card(x, y, w, h, 24.f, 0.78f);
    Disc(x + 44.f, y + 46.f, 19.f, C(WHITE, 0.06f));
    Glyph(g_weatherGlyph, x + 44.f, y + 46.f, 18.f, COL_T90);
    Txt(g_weatherTempStr.c_str(), x + 24.f, y + 78.f, 140.f, 66.f, F_SEMI, 46.f, 600, COL_T100, 0, 1.f);
    Txt(g_weatherCond.c_str(), x + 26.f, y + 142.f, w - 50.f, 24.f, F_UI, 13.f, 400, COL_T50, 0, 1.f);
    Txt(g_weatherHighLow.c_str(), x + 26.f, y + 164.f, w - 50.f, 22.f, F_UI, 11.5f, 400, COL_T35, 0, 1.f);
    Txt(g_weatherCity.c_str(), x + w - 160.f, y + 24.f, 138.f, 20.f, F_UI, 11.f, 500, COL_T35, 2, 1.f);
}

// ----------------------------------------------------------------------------
//  Desk widget: finance
// ----------------------------------------------------------------------------
static void DrawFinance(float x, float y, float w, float h) {
    Card(x, y, w, h, 24.f, 0.78f);
    Txt(L"BTC / USD", x + 26.f, y + 20.f, 140.f, 20.f, F_UI, 11.5f, 600, COL_T35, 0, 1.f);
    Txt(g_btcPriceStr.c_str(), x + 24.f, y + 42.f, 240.f, 44.f, F_SEMI, 30.f, 600, COL_T100, 0, 1.f);
    unsigned changeCol = g_btcChangeVal >= 0.f ? COL_ACC : 0xF87171;
    Txt(g_btcChangeStr.c_str(), x + 24.f, y + 84.f, 100.f, 22.f, F_UI, 12.f, 600, changeCol, 0, 1.f);
    Txt(L"24h", x + 24.f, y + 104.f, 100.f, 18.f, F_UI, 10.5f, 400, COL_T35, 0, 1.f);
    Sparkline(x + 26.f, y + 126.f, w - 52.f, 52.f, g_btcSpark, 24, changeCol);
}

// ----------------------------------------------------------------------------
//  Desk widget: notes
// ----------------------------------------------------------------------------
static void DrawNotesCard(float x, float y, float w, float h) {
    Card(x, y, w, h, 24.f, 0.80f);
    Txt(L"Notes", x + 26.f, y + 18.f, 200.f, 30.f, F_SEMI, 15.f, 600, COL_T90, 0, 1.f);
    Txt(std::to_wstring(g_notes.size()) + L" items", x + w - 130.f, y + 24.f, 104.f, 20.f,
        F_UI, 11.f, 400, COL_T35, 2, 1.f);

    float ry = y + 56.f;
    for (int i = 0; i < (int)g_notes.size() && i < 5; ++i) {
        bool hov = g_hover == H_NOTE + i;
        if (hov) Fill(x + 16.f, ry - 10.f, w - 32.f, 42.f, 10.f, C(WHITE, 0.04f));
        Checkbox(x + 26.f, ry - 2.f, 18.f, g_notes[i].done, hov);
        Txt(g_notes[i].text, x + 56.f, ry - 8.f, w - 84.f, 30.f, F_UI, 13.f, 400,
            g_notes[i].done ? COL_T35 : COL_T80, 0, 1.f);
        if (g_notes[i].done) {
            // strike-through
            float tw = 0.f;
            for (size_t k = 0; k < g_notes[i].text.size(); ++k) tw += (g_notes[i].text[k] == L' ') ? 4.f : 7.2f;
            Line(x + 56.f, ry + 8.f, x + 56.f + std::min(tw, w - 90.f), ry + 8.f, C(COL_T35, 0.8f), 1.f);
        }
        AddHit(x + 16.f, ry - 12.f, w - 32.f, 44.f, H_NOTE + i);
        ry += 44.f;
    }
    // add-row
    bool hov = g_hover == H_ADDNOTE;
    float ay = y + h - 52.f;
    Fill(x + 16.f, ay, w - 32.f, 38.f, 12.f, C(WHITE, hov ? 0.06f : 0.03f));
    Stroke(x + 16.f, ay, w - 32.f, 38.f, 12.f, C(WHITE, hov ? 0.12f : 0.06f));
    Glyph((wchar_t)0xE710, x + 40.f, ay + 19.f, 11.f, COL_T50);
    Txt(L"Add a note", x + 58.f, ay + 4.f, 200.f, 30.f, F_UI, 12.f, 500, COL_T50, 0, 1.f);
    AddHit(x + 16.f, ay, w - 32.f, 38.f, H_ADDNOTE);
}

// ----------------------------------------------------------------------------
//  Desk widget: media player
// ----------------------------------------------------------------------------
static void DrawMediaCard(float x, float y, float w, float h) {
    Card(x, y, w, h, 24.f, 0.82f);
    // Album art: procedural duotone gradient + concentric rings.
    float ax = x + 22.f, ay = y + 22.f, as = 92.f;
    g_br->SetColor(C(0x1B1E24));
    g_rt->FillRoundedRectangle(D2D1::RoundedRect(R(ax, ay, as, as), 16.f, 16.f), g_br);
    Disc(ax + as * 0.5f, ay + as * 0.5f, as * 0.40f, C(0x2E3440, 0.9f));
    Ring(ax + as * 0.5f, ay + as * 0.5f, as * 0.28f, C(0x3D4552, 0.9f), 1.2f);
    Ring(ax + as * 0.5f, ay + as * 0.5f, as * 0.17f, C(0x4A5260, 0.9f), 1.2f);
    Disc(ax + as * 0.5f, ay + as * 0.5f, as * 0.06f, C(0x0A0A0C, 0.9f));
    Stroke(ax, ay, as, as, 16.f, C(WHITE, 0.07f));

    Txt(L"Neutral Obsidian", x + 130.f, y + 26.f, w - 156.f, 28.f, F_SEMI, 14.5f, 600, COL_T90, 0, 1.f);
    Txt(L"Buffy · Ambient Works", x + 130.f, y + 52.f, w - 156.f, 22.f, F_UI, 11.5f, 400, COL_T35, 0, 1.f);

    // Waveform flourish (frozen when reduce-motion is on)
    const float wavT = g_motion ? (float)g_clockT : 0.f;
    for (int i = 0; i < 22; ++i) {
        float bh = 4.f + 12.f * std::fabs(std::sin((float)i * 0.9f + wavT * 1.6f));
        Fill(x + 130.f + i * 6.f, y + 78.f - bh * 0.5f, 3.f, bh, 1.5f, C(WHITE, 0.14f));
    }

    // Progress + controls
    float tw = w - 44.f;
    float pv = Clamp(g_pos / (g_dur > 1.f ? g_dur : 1.f), 0.f, 1.f);
    SliderDraw(x + 22.f, y + h - 92.f, tw, pv, H_SEEK, 6.f);
    Txt(MediaTime(g_pos), x + 22.f, y + h - 70.f, 60.f, 18.f, F_MONO, 10.f, 400, COL_T35, 0, 1.f);
    Txt(MediaTime(g_dur), x + w - 82.f, y + h - 70.f, 60.f, 18.f, F_MONO, 10.f, 400, COL_T35, 2, 1.f);

    float cy = y + h - 32.f;
    bool hovS = g_hover == H_SHUFFLE, hovR = g_hover == H_REPEAT;
    Glyph((wchar_t)0xE8B1, x + 34.f, cy, 12.f, hovS ? COL_T90 : COL_T35);
    Glyph((wchar_t)0xE892, x + w * 0.5f - 44.f, cy, 13.f, g_hover == H_PREV ? COL_T90 : COL_T50);
    float pr = 17.f;
    Disc(x + w * 0.5f, cy, pr, C(COL_ACTIVE, g_hover == H_PLAY ? 1.f : 0.94f));
    Glyph(g_playing ? (wchar_t)0xE769 : (wchar_t)0xE768, x + w * 0.5f, cy, 11.f, 0x0B0B0D);
    Glyph((wchar_t)0xE893, x + w * 0.5f + 44.f, cy, 13.f, g_hover == H_NEXT ? COL_T90 : COL_T50);
    Glyph((wchar_t)0xE8EE, x + w - 34.f, cy, 12.f, hovR ? COL_T90 : COL_T35);

    AddHit(x + 20.f, cy - 18.f, 30.f, 36.f, H_SHUFFLE);
    AddHit(x + w * 0.5f - 62.f, cy - 18.f, 36.f, 36.f, H_PREV);
    AddHit(x + w * 0.5f - 18.f, cy - 18.f, 36.f, 36.f, H_PLAY);
    AddHit(x + w * 0.5f + 26.f, cy - 18.f, 36.f, 36.f, H_NEXT);
    AddHit(x + w - 52.f, cy - 18.f, 30.f, 36.f, H_REPEAT);
}

// ----------------------------------------------------------------------------
//  Desk widget: system meters
// ----------------------------------------------------------------------------
static void DrawSystemCard(float x, float y, float w, float h) {
    Card(x, y, w, h, 24.f, 0.76f);
    Txt(L"System", x + 26.f, y + 18.f, 160.f, 28.f, F_SEMI, 15.f, 600, COL_T90, 0, 1.f);
    Disc(x + w - 100.f, y + 32.f, 9.f, C(COL_ACC_BG, 0.18f));
    Disc(x + w - 100.f, y + 32.f, 3.f, C(COL_ACC, 0.95f));
    Txt(L"live", x + w - 92.f, y + 22.f, 66.f, 20.f, F_UI, 11.f, 500, COL_ACC, 2, 1.f);
    const wchar_t* names[3] = { L"CPU", L"Memory", L"Disk C:" };
    float vals[3] = { g_cpu, g_ram, g_disk };
    float ry = y + 56.f;
    for (int i = 0; i < 3; ++i) {
        Txt(names[i], x + 26.f, ry, 90.f, 18.f, F_UI, 11.f, 400, COL_T50, 0, 1.f);
        Txt(std::to_wstring((int)(vals[i] * 100)) + L"%", x + w - 76.f, ry, 50.f, 18.f,
            F_MONO, 10.5f, 400, COL_T35, 2, 1.f);
        Meter(x + 26.f, ry + 22.f, w - 52.f, vals[i], COL_T90);
        ry += 42.f;
    }
    Txt(g_uptime, x + 26.f, y + h - 26.f, w - 52.f, 18.f, F_UI, 10.f, 400, COL_T35, 0, 1.f);
}

// ----------------------------------------------------------------------------
//  Desk widget: shortcuts cheat-sheet
// ----------------------------------------------------------------------------
static void DrawShortcutsCard(float x, float y, float w, float h) {
    Card(x, y, w, h, 24.f, 0.72f);
    Txt(L"Shortcuts", x + 26.f, y + 18.f, 200.f, 28.f, F_SEMI, 15.f, 600, COL_T90, 0, 1.f);
    struct K { const wchar_t* key; const wchar_t* what; };
    static const K keys[] = {
        { L"Ctrl+Spc",  L"Launcher" },
        { L"F11",       L"Fullscreen" },
        { L"Esc",       L"Back" },
        { L"R-click",   L"Desktop menu" },
    };
    float ry = y + 58.f;
    for (int i = 0; i < 4; ++i) {
        float kw = 0.f;
        size_t n = wcslen(keys[i].key);
        kw = 26.f + (float)n * 6.4f;
        Fill(x + 26.f, ry, kw, 26.f, 7.f, C(WHITE, 0.05f));
        Stroke(x + 26.f, ry, kw, 26.f, 7.f, C(WHITE, 0.10f));
        Txt(keys[i].key, x + 26.f, ry + 3.f, kw, 20.f, F_MONO, 10.5f, 400, COL_T70, 1, 1.f);
        Txt(keys[i].what, x + 34.f + kw, ry + 3.f, w - 70.f - kw, 20.f, F_UI, 11.5f, 400, COL_T50, 0, 1.f);
        ry += 40.f;
    }
}

// ----------------------------------------------------------------------------
//  Desk widget: recent files
// ----------------------------------------------------------------------------
static void DrawRecentCard(float x, float y, float w, float h) {
    Card(x, y, w, h, 24.f, 0.74f);
    Txt(L"Recent", x + 26.f, y + 18.f, 160.f, 28.f, F_SEMI, 15.f, 600, COL_T90, 0, 1.f);
    Txt(L"Open Files", x + w - 120.f, y + 22.f, 94.f, 20.f, F_UI, 11.f, 500, COL_T50, 2, 1.f);
    float ry = y + 58.f;
    for (int i = 0; i < 3; ++i) {
        float cx = x + 44.f;
        Disc(cx, ry + 14.f, 16.f, C(WHITE, 0.05f));
        Glyph(FILES[i + 2].glyph, cx, ry + 14.f, 11.f, COL_T70);
        Txt(FILES[i + 2].name, x + 70.f, ry + 2.f, w - 96.f, 20.f, F_UI, 11.5f, 500, COL_T80, 0, 1.f);
        Txt(FILES[i + 2].when, x + 70.f, ry + 20.f, w - 96.f, 18.f, F_UI, 10.f, 400, COL_T35, 0, 1.f);
        ry += 46.f;
    }
}

// Human names, used by the Appearance page's visibility list. Kept in widget
// id order so the list and shelllayout::WidgetId cannot drift apart.
static const wchar_t* WIDGET_NAME[shelllayout::W_COUNT] = {
    L"Clock", L"Weather", L"Finance", L"Notes", L"Media", L"System", L"Recent files", L"Shortcuts",
};

// Draws one widget in its own local coordinate space, scaled about its own top
// left corner. The hit transform mirrors this exactly (see AddHit), so a widget
// at any size still reports clicks where the pixels are.
template <class F>
static void InWidget(float x, float y, float s, F fn) {
    g_hitX.s = s;
    g_hitX.tx = x + g_pageDX;
    g_hitX.ty = y;
    g_rt->SetTransform(D2D1::Matrix3x2F::Scale(s, s) *
                       D2D1::Matrix3x2F::Translation(x + g_pageDX, y) * g_base);
    fn();
    g_hitX = shelllayout::Xform();
    g_hitX.tx = g_pageDX;
    g_rt->SetTransform(D2D1::Matrix3x2F::Translation(g_pageDX, 0.f) * g_base);
}


static void DrawDesktopWidgets() {
    shelllayout::Rect placed[shelllayout::W_COUNT];
    // The shelf starts at 1096; widgets are clipped out above 1088 so they can
    // never slide underneath it at the largest size.
    const int n = shelllayout::Compute(g_widgetOn, g_widgetScale, 96.f, 88.f, 18.f, 18.f, 1088.f, placed);
    for (int i = 0; i < n; ++i) {
        const int id = placed[i].id;
        InWidget(placed[i].x, placed[i].y, g_widgetScale, [id] {
            switch (id) {
            case shelllayout::W_CLOCK:     DrawClockCard(0.f, 0.f, 420.f, 200.f); break;
            case shelllayout::W_WEATHER:   DrawWeather(0.f, 0.f, 300.f, 200.f); break;
            case shelllayout::W_FINANCE:   DrawFinance(0.f, 0.f, 300.f, 200.f); break;
            case shelllayout::W_NOTES:     DrawNotesCard(0.f, 0.f, 420.f, 352.f); break;
            case shelllayout::W_MEDIA:     DrawMediaCard(0.f, 0.f, 420.f, 240.f); break;
            case shelllayout::W_SYSTEM:    DrawSystemCard(0.f, 0.f, 300.f, 206.f); break;
            case shelllayout::W_RECENT:    DrawRecentCard(0.f, 0.f, 300.f, 216.f); break;
            case shelllayout::W_SHORTCUTS: DrawShortcutsCard(0.f, 0.f, 300.f, 232.f); break;
            default: break;
            }
        });
    }
}
// ============================================================================
//  Section 3 — shelf, system tray and the unified system bubble
// ============================================================================

static const float SHELF_Y = 1096.f;
static const float SHELF_H = 80.f;

// Pinned shelf entries (indices into APPS).
static const int PINNED[] = { APP_FILES, APP_TERM, 5, 6, 8, 9, APP_SETTINGS };
static const int PINNED_N = (int)(sizeof(PINNED) / sizeof(PINNED[0]));

static float ShelfWidth() { return 28.f + 56.f * (float)(PINNED_N + 1) + 8.f * (float)PINNED_N; }

static void BatteryIcon(float x, float y, float w, float h, float v) {
    Stroke(x, y, w, h, 4.f, C(WHITE, 0.35f));
    Fill(x + w + 1.f, y + h * 0.32f, 3.f, h * 0.36f, 1.5f, C(WHITE, 0.35f));
    float iw = (w - 6.f) * Clamp(v, 0.f, 1.f);
    if (iw > 0.5f) Fill(x + 3.f, y + 3.f, iw, h - 6.f, 2.f, C(COL_T90, 0.92f));
}

// Draw a single app icon tile (used by the shelf and the launcher grid).
static void AppIcon(const AppDef& app, float cx, float cy, float tile, float glyphSize, bool hovered) {
    if (hovered) Fill(cx - tile * 0.5f, cy - tile * 0.5f, tile, tile, tile * 0.30f, C(WHITE, 0.10f));
    else Fill(cx - tile * 0.5f, cy - tile * 0.5f, tile, tile, tile * 0.30f, C(WHITE, 0.045f));
    if (app.glyph) Glyph(app.glyph, cx, cy, glyphSize, COL_T90, 0.95f);
    else if (app.mono) Txt(app.mono, cx - glyphSize, cy - glyphSize, glyphSize * 2.f, glyphSize * 2.f,
                           F_MONO, glyphSize * 1.15f, 600, COL_T90, 1, 0.95f);
    else Disc(cx, cy, glyphSize * 0.4f, C(COL_T90, 0.9f));
}

static void DrawShelf() {
    float w = ShelfWidth();
    float x = (DW - w) * 0.5f, y = SHELF_Y, h = SHELF_H;
    Card(x, y, w, h, 26.f, 0.86f);

    float step = 56.f + 8.f;
    float ix = x + 14.f + 28.f;       // centre of launcher button
    // Launcher button
    bool lh = (g_hover == H_LAUNCHER) || (g_panel == PANEL_LAUNCH);
    Disc(ix, y + h * 0.5f, 22.f, lh ? C(COL_T100, 0.96f) : C(WHITE, 0.08f));
    Glyph((wchar_t)0xE700, ix, y + h * 0.5f, 14.f, lh ? 0x0B0B0D : COL_T90);
    AddHit(ix - 26.f, y + 8.f, 52.f, h - 16.f, H_LAUNCHER);

    for (int i = 0; i < PINNED_N; ++i) {
        const AppDef& app = APPS[PINNED[i]];
        float cx = x + 14.f + 56.f + step * (float)i + 28.f;
        bool hov = (g_hover == H_SHELF + i);
        float lift = hov ? 3.f : 0.f;
        AppIcon(app, cx, y + h * 0.5f - lift, 46.f, 19.f, hov);
        if (app.kind >= 0 && g_win[app.kind].open) {
            Disc(cx, y + h - 11.f, 2.2f, C(COL_T90, 0.9f));
        }
        AddHit(cx - 26.f, y + 6.f, 52.f, h - 12.f, H_SHELF + i);
    }
}

// ----------------------------------------------------------------------------
//  System tray pill
// ----------------------------------------------------------------------------
static const float TRAY_W = 232.f, TRAY_H = 64.f;

static void DrawTray() {
    float x = DW - 32.f - TRAY_W, y = SHELF_Y + (SHELF_H - TRAY_H) * 0.5f;
    bool hov = (g_hover == H_TRAY);
    Fill(x, y, TRAY_W, TRAY_H, 22.f, C(hov ? COL_SURF : COL_SURF, hov ? 0.98f : 0.82f));
    Stroke(x, y, TRAY_W, TRAY_H, 22.f, C(WHITE, hov ? 0.12f : 0.055f));

    if (g_hasBatt) {
        BatteryIcon(x + 18.f, y + 20.f, 26.f, 13.f, (float)g_battPct / 100.f);
        Txt(std::to_wstring(g_battPct) + L"%", x + 50.f, y + 14.f, 44.f, 24.f, F_UI, 11.5f, 600,
            g_acLine ? COL_T100 : COL_T70, 0, 1.f);
    } else {
        Glyph((wchar_t)0xE7E8, x + 28.f, y + 26.f, 11.f, COL_T70);
        Txt(L"AC", x + 50.f, y + 14.f, 44.f, 24.f, F_UI, 11.5f, 600, COL_T70, 0, 1.f);
    }
    Glyph((wchar_t)0xE701, x + 98.f, y + 26.f, 11.f, g_tog[TOG_WIFI] ? COL_T90 : COL_T35, 0.9f);
    if (g_tog[TOG_BT]) Glyph((wchar_t)0xE702, x + 122.f, y + 26.f, 10.f, COL_T70, 0.85f);

    Txt(TimeHM(), x + TRAY_W - 92.f, y + 10.f, 76.f, 26.f, F_SEMI, 15.f, 600, COL_T90, 2, 1.f);
    Txt(DateShort(), x + TRAY_W - 112.f, y + 34.f, 96.f, 18.f, F_UI, 10.f, 400, COL_T35, 2, 1.f);

    AddHit(x, y, TRAY_W, TRAY_H, H_TRAY);
}

// ----------------------------------------------------------------------------
//  Unified system bubble: header, toggles, sliders, notifications, footer
// ----------------------------------------------------------------------------
static void DrawNotifRow(float x, float y, float w, int idx, float a) {
    const Notif& n = g_notifs[idx];
    bool hov = (g_hover == H_NOTIF + idx);
    Fill(x, y, w, 74.f, 14.f, C(WHITE, hov ? 0.055f : 0.028f));
    Disc(x + 26.f, y + 26.f, 14.f, C(WHITE, 0.06f));
    Glyph(n.glyph, x + 26.f, y + 26.f, 11.f, n.read ? COL_T35 : COL_T80);
    Txt(n.app, x + 48.f, y + 10.f, w - 96.f, 18.f, F_UI, 10.f, 600, COL_T35, 0, 1.f);
    Txt(n.when, x + w - 56.f, y + 10.f, 44.f, 18.f, F_UI, 10.f, 400, COL_T35, 2, 1.f);
    Txt(n.title, x + 48.f, y + 26.f, w - 96.f, 22.f, F_UI, 12.f, 600, n.read ? COL_T50 : COL_T90, 0, 1.f);
    Txt(n.body, x + 48.f, y + 46.f, w - 96.f, 20.f, F_UI, 10.5f, 400, COL_T35, 0, 1.f);
    if (hov) Glyph((wchar_t)0xE711, x + w - 20.f, y + 48.f, 8.f, COL_T50, a);
    AddHit(x, y, w, 74.f, H_NOTIF + idx);
    AddHit(x + w - 32.f, y + 34.f, 26.f, 28.f, H_NOTIF + 50 + idx);
}

static void DrawSystemBubble(float a) {
    const float w = 448.f, h = 720.f;
    const float eased = EaseOut(a);
    float x = DW - 32.f - w;
    float y = SHELF_Y - 12.f - h + (1.f - eased) * 20.f;

    if (!g_rt) return;
    ID2D1Layer* layer = nullptr;
    g_rt->CreateLayer(&layer);
    if (layer) {
        D2D1_LAYER_PARAMETERS lp = D2D1::LayerParameters(
            D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
            D2D1::IdentityMatrix(), Clamp(a, 0.f, 1.f));
        g_rt->PushLayer(lp, layer);
    }

    Shadow(x, y, w, h, 28.f, 0.07f, 9);
    Fill(x, y, w, h, 28.f, C(COL_SURF, 0.965f));
    Stroke(x, y, w, h, 28.f, C(WHITE, 0.07f));

    // ---- header
    Txt(DateLong(), x + 26.f, y + 18.f, w - 120.f, 22.f, F_UI, 12.f, 500, COL_T50, 0, 1.f);
    Txt(TimeHM(), x + 24.f, y + 38.f, 240.f, 54.f, F_SEMI, 38.f, 600, COL_T100, 0, 1.f);
    Disc(x + w - 62.f, y + 40.f, 17.f, C(WHITE, 0.05f));
    Glyph(g_weatherGlyph, x + w - 62.f, y + 40.f, 15.f, COL_T90);
    Txt(g_weatherTempStr.c_str(), x + w - 132.f, y + 26.f, 44.f, 26.f, F_SEMI, 14.f, 600, COL_T90, 2, 1.f);
    Txt(g_weatherCity.c_str(), x + w - 170.f, y + 48.f, 104.f, 18.f, F_UI, 10.f, 400, COL_T35, 2, 1.f);
    Line(x + 24.f, y + 100.f, x + w - 24.f, y + 100.f, C(WHITE, 0.06f));

    // ---- toggles
    const float pad = 24.f, gap = 14.f;
    const float tw = (w - pad * 2.f - gap) * 0.5f, th = 66.f;
    for (int i = 0; i < TOG_COUNT; ++i) {
        float tx = x + pad + (float)(i % 2) * (tw + gap);
        float ty = y + 118.f + (float)(i / 2) * (th + gap);
        ToggleTile(tx, ty, tw, th, i);
    }

    // ---- sliders
    float sy = y + 118.f + 3.f * (th + gap) + 6.f;
    Glyph((wchar_t)(g_vol <= 0.001f ? 0xE74F : 0xE767), x + pad + 12.f, sy + 16.f, 12.f, COL_T50);
    SliderDraw(x + pad + 32.f, sy, w - pad * 2.f - 32.f, g_vol, H_VOL);
    float by = sy + 40.f;
    Glyph((wchar_t)0xE706, x + pad + 12.f, by + 16.f, 12.f, COL_T50);
    SliderDraw(x + pad + 32.f, by, w - pad * 2.f - 32.f, g_bri, H_BRI);

    // ---- notifications
    float ny = by + 40.f;
    int unread = 0;
    for (size_t i = 0; i < g_notifs.size(); ++i) if (!g_notifs[i].read) ++unread;
    std::wstring nlabel = L"Notifications";
    if (unread > 0) nlabel += L"  \u00B7  " + std::to_wstring(unread) + L" new";
    Txt(nlabel, x + pad, ny, 260.f, 22.f, F_SEMI, 12.f, 600, COL_T90, 0, 1.f);
    Txt(L"Clear all", x + w - pad - 90.f, ny + 2.f, 90.f, 20.f, F_UI, 10.5f, 500, COL_T35, 2, 1.f);
    AddHit(x + w - pad - 96.f, ny - 4.f, 96.f, 26.f, H_NOTIF + 99);
    ny += 26.f;

    int shown = 0;
    for (int i = 0; i < (int)g_notifs.size() && shown < 2; ++i, ++shown) {
        DrawNotifRow(x + pad, ny + shown * 82.f, w - pad * 2.f, i, a);
    }
    if (shown == 0) {
        Txt(L"You're all caught up", x + pad, ny + 16.f, w - pad * 2.f, 30.f, F_UI, 12.f, 400,
            COL_T35, 1, 1.f);
    }

    // ---- footer
    float fy = y + h - 44.f;
    Line(x + 24.f, fy - 18.f, x + w - 24.f, fy - 18.f, C(WHITE, 0.06f));
    RingButton(x + pad + 14.f, fy, 17.f, H_LOCK, (wchar_t)0xE72E, 12.f);
    RingButton(x + pad + 62.f, fy, 17.f, H_SETTINGS, (wchar_t)0xE713, 12.f);
    RingButton(x + w - pad - 14.f, fy, 17.f, H_POWER, (wchar_t)0xE7E8, 12.f);
    Txt(g_tog[TOG_DND] ? L"Do not disturb on" : L"Do not disturb off",
        x + pad + 96.f, fy - 11.f, w - pad * 2.f - 160.f, 22.f, F_UI, 10.5f, 400, COL_T35, 0, 1.f);

    if (layer) { g_rt->PopLayer(); layer->Release(); }
}
// ============================================================================
//  Section 4 — launcher, desktop context menu, toasts, lock screen
// ============================================================================

static const float LX = 200.f, LY = 140.f, LW = 1520.f, LH = 720.f;
static const int   LCOLS = 7, LROWS = 3, LPER = LCOLS * LROWS;

static std::wstring g_query;
static std::vector<int> g_view;

static wchar_t LowerW(wchar_t c) { return (c >= L'A' && c <= L'Z') ? (wchar_t)(c + 32) : c; }
static bool ContainsI(const std::wstring& hay, const std::wstring& needle) {
    if (needle.empty()) return true;
    if (hay.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
        size_t j = 0;
        while (j < needle.size() && LowerW(hay[i + j]) == LowerW(needle[j])) ++j;
        if (j == needle.size()) return true;
    }
    return false;
}

static void RebuildView() {
    g_view.clear();
    for (int i = 0; i < APP_N; ++i)
        if (ContainsI(APPS[i].name, g_query)) g_view.push_back(i);
}

static int PageCount() {
    int n = (int)g_view.size();
    return n == 0 ? 1 : (n + LPER - 1) / LPER;
}

static void DrawLauncher(float a) {
    const float eased = EaseOut(a);
    float y = LY + (1.f - eased) * 36.f;
    float x = LX;

    if (!g_rt) return;
    ID2D1Layer* layer = nullptr;
    g_rt->CreateLayer(&layer);
    if (layer) {
        D2D1_LAYER_PARAMETERS lp = D2D1::LayerParameters(
            D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
            D2D1::IdentityMatrix(), Clamp(a, 0.f, 1.f));
        g_rt->PushLayer(lp, layer);
    }

    Shadow(x, y, LW, LH, 32.f, 0.075f, 10);
    Fill(x, y, LW, LH, 32.f, C(COL_SURF, 0.975f));
    Stroke(x, y, LW, LH, 32.f, C(WHITE, 0.07f));

    // ---- search field
    float sx = x + 56.f, sy = y + 44.f, sw = 620.f, sh = 56.f;
    Fill(sx, sy, sw, sh, 18.f, C(COL_SEC, 0.55f));
    Stroke(sx, sy, sw, sh, 18.f, C(WHITE, 0.09f));
    Glyph((wchar_t)0xE721, sx + 26.f, sy + sh * 0.5f, 12.f, COL_T50);
    if (g_query.empty()) {
        Txt(L"Search apps and settings", sx + 50.f, sy + 6.f, sw - 80.f, sh - 12.f, F_UI, 14.f, 400,
            COL_T35, 0, 1.f);
    } else {
        Txt(g_query, sx + 50.f, sy + 6.f, sw - 110.f, sh - 12.f, F_UI, 14.f, 500, COL_T90, 0, 1.f);
        if (std::fmod(g_clockT, 1.1) < 0.55)
            Line(sx + 52.f + std::min(560.f, (float)g_query.size() * 8.2f), sy + 18.f,
                 sx + 52.f + std::min(560.f, (float)g_query.size() * 8.2f), sy + 40.f,
                 C(COL_T90, 0.7f), 1.5f);
    }
    AddHit(sx, sy, sw, sh, H_LAUNCH_SEARCH);

    Txt(L"All apps", x + LW - 260.f, y + 58.f, 130.f, 26.f, F_UI, 12.f, 500, COL_T35, 2, 1.f);
    Txt(std::to_wstring((int)g_view.size()), x + LW - 110.f, y + 58.f, 40.f, 26.f, F_MONO, 11.f,
        400, COL_T35, 2, 1.f);
    RingButton(x + LW - 44.f, y + 50.f, 16.f, H_LAUNCH_CLOSE, (wchar_t)0xE711, 10.f);

    // ---- grid
    const float gx = x + 56.f, gy = y + 136.f, tileW = 189.f, tileH = 150.f, gap = 14.f;
    int start = g_launchPage * LPER;
    for (int k = 0; k < LPER; ++k) {
        int vi = start + k;
        if (vi >= (int)g_view.size()) break;
        const AppDef& app = APPS[g_view[vi]];
        float tx = gx + (float)(k % LCOLS) * (tileW + gap);
        float ty = gy + (float)(k / LCOLS) * (tileH + gap);
        bool hov = (g_hover == H_APP + vi) || (g_launchSel == vi);
        if (hov) Fill(tx, ty, tileW, tileH, 18.f, C(WHITE, 0.07f));
        AppIcon(app, tx + tileW * 0.5f, ty + 56.f, 76.f, 30.f, false);
        Txt(app.name, tx + 6.f, ty + 104.f, tileW - 12.f, 34.f, F_UI, 12.f, 500,
            hov ? COL_T100 : COL_T70, 1, 1.f);
        AddHit(tx, ty, tileW, tileH, H_APP + vi);
    }

    if (g_view.empty()) {
        Glyph((wchar_t)0xE721, x + LW * 0.5f, gy + 120.f, 34.f, COL_T35, 0.8f);
        Txt(L"No apps match \u201C" + g_query + L"\u201D", x + 100.f, gy + 190.f, LW - 200.f, 34.f,
            F_UI, 15.f, 400, COL_T35, 1, 1.f);
    }

    // ---- page dots
    int pages = PageCount();
    float dotY = y + LH - 48.f;
    float total = (float)pages * 18.f;
    for (int p = 0; p < pages; ++p) {
        float cx = x + LW * 0.5f - total * 0.5f + 9.f + (float)p * 18.f;
        bool on = (p == g_launchPage);
        Disc(cx, dotY, on ? 4.5f : 3.2f, on ? C(COL_T90, 0.95f) : C(WHITE, 0.20f));
        AddHit(cx - 9.f, dotY - 10.f, 18.f, 20.f, H_PAGE + p);
    }
    if (pages > 1) {
        Txt(L"Page " + std::to_wstring(g_launchPage + 1) + L" of " + std::to_wstring(pages),
            x + LW - 260.f, dotY - 12.f, 200.f, 24.f, F_UI, 10.5f, 400, COL_T35, 2, 1.f);
    }

    if (layer) { g_rt->PopLayer(); layer->Release(); }
}

// ----------------------------------------------------------------------------
//  Desktop context menu
// ----------------------------------------------------------------------------
struct CtxItem { const wchar_t* label; const wchar_t* key; wchar_t glyph; int cmd; };
enum { CTX_WALL = 0, CTX_CUSTOM_WALL, CTX_PERS, CTX_LAUNCH, CTX_SYS, CTX_FULL, CTX_LOCK, CTX_QUIT, CTX_N };
static const CtxItem CTX[CTX_N] = {
    { L"Choose wallpaper\u2026", L"",        (wchar_t)0xE8B9, CTX_CUSTOM_WALL },
    { L"Next wallpaper",         L"",        (wchar_t)0xE790, CTX_WALL        },
    { L"Personalise\u2026",       L"",        (wchar_t)0xE713, CTX_PERS        },
    { L"Open launcher",          L"Super",   (wchar_t)0xE700, CTX_LAUNCH      },
    { L"System menu",            L"",        (wchar_t)0xE713, CTX_SYS         },
    { L"Toggle fullscreen",      L"F11",     (wchar_t)0xE740, CTX_FULL        },
    { L"Lock screen",            L"",        (wchar_t)0xE72E, CTX_LOCK        },
    { L"Quit",                   L"Ctrl+Q",  (wchar_t)0xE7E8, CTX_QUIT        },
};

static void DrawContextMenu(float a) {
    const float w = 264.f, rowH = 36.f, pad = 8.f;
    float h = pad * 2.f + rowH * CTX_N + 8.f;
    float x = Clamp(g_ctxX, 8.f, DW - w - 8.f);
    float y = Clamp(g_ctxY, 8.f, DH - h - 8.f);

    float e = EaseOut(a);
    ID2D1Layer* layer = nullptr;
    g_rt->CreateLayer(&layer);
    if (layer) {
        D2D1_LAYER_PARAMETERS lp = D2D1::LayerParameters(
            D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
            D2D1::IdentityMatrix(), Clamp(e, 0.f, 1.f));
        g_rt->PushLayer(lp, layer);
    }

    Shadow(x, y, w, h, 14.f, 0.08f, 6);
    Fill(x, y, w, h, 14.f, C(0x16171A, 0.985f));
    Stroke(x, y, w, h, 14.f, C(WHITE, 0.09f));

    for (int i = 0; i < CTX_N; ++i) {
        float ry = y + pad + rowH * (float)i + (i >= 4 ? 8.f : 0.f);
        bool hov = (g_ctxHover == i);
        if (hov) Fill(x + 6.f, ry, w - 12.f, rowH, 9.f, C(WHITE, 0.07f));
        Glyph(CTX[i].glyph, x + 26.f, ry + rowH * 0.5f, 10.f, hov ? COL_T90 : COL_T50);
        Txt(CTX[i].label, x + 46.f, ry + 4.f, w - 130.f, rowH - 8.f, F_UI, 12.f, 500,
            hov ? COL_T100 : COL_T80, 0, 1.f);
        if (CTX[i].key[0]) Txt(CTX[i].key, x + w - 92.f, ry + 4.f, 76.f, rowH - 8.f, F_MONO, 9.5f,
                               400, COL_T35, 2, 1.f);
        AddHit(x + 6.f, ry, w - 12.f, rowH, H_CTX + i);
        if (i == 4) Line(x + 14.f, ry + rowH + 4.f, x + w - 14.f, ry + rowH + 4.f, C(WHITE, 0.06f));
    }

    if (layer) { g_rt->PopLayer(); layer->Release(); }
}

// ----------------------------------------------------------------------------
//  Toasts
// ----------------------------------------------------------------------------
static void DrawToasts() {
    const float w = 404.f, h = 66.f;
    float x = DW - 32.f - w;
    float baseY = SHELF_Y - 24.f;
    int shown = 0;
    for (int i = (int)g_toasts.size() - 1; i >= 0 && shown < 3; --i, ++shown) {
        const Toast& t = g_toasts[i];
        float in = Clamp(t.t / 0.20f, 0.f, 1.f);
        float out = Clamp((4.2f - t.t) / 0.35f, 0.f, 1.f);
        float a = std::min(in, out);
        if (a <= 0.01f) continue;
        float y = baseY - h - (float)shown * (h + 10.f);
        float slide = (1.f - EaseOut(in)) * 24.f;
        Shadow(x, y + slide, w, h, 16.f, 0.06f * a, 5);
        Fill(x, y + slide, w, h, 16.f, C(0x16171A, 0.97f * a));
        Stroke(x, y + slide, w, h, 16.f, C(WHITE, 0.08f * a));
        Disc(x + 34.f, y + slide + h * 0.5f, 16.f, C(WHITE, 0.06f * a));
        Glyph(t.glyph, x + 34.f, y + slide + h * 0.5f, 12.f, COL_T90, a);
        Txt(t.text, x + 60.f, y + slide + 10.f, w - 84.f, h - 20.f, F_UI, 12.f, 500, COL_T90, 0, a);
    }
}

// ----------------------------------------------------------------------------
//  Lock screen
// ----------------------------------------------------------------------------
static void DrawLockScreen(float a) {
    float e = EaseOut(a);
    Fill(0.f, 0.f, DW, DH, 0.f, C(0x05060A, 0.86f * e));
    if (g_glow) {
        g_glow->SetOpacity(0.35f * e);
        g_rt->FillRectangle(R(0.f, 0.f, DW, DH), g_glow);
        g_glow->SetOpacity(1.f);
    }

    float cy = DH * 0.28f;
    Txt(TimeHM(), 0.f, cy, DW, 130.f, F_SEMI, 108.f, 600, COL_T100, 1, 0.98f * e);
    Txt(DateLong(), 0.f, cy + 128.f, DW, 36.f, F_UI, 18.f, 400, COL_T70, 1, e);

    Disc(DW * 0.5f, cy + 246.f, 44.f, C(WHITE, 0.07f * e));
    Ring(DW * 0.5f, cy + 246.f, 44.f, C(WHITE, 0.14f * e), 1.f);
    Glyph((wchar_t)0xE77B, DW * 0.5f, cy + 246.f, 30.f, COL_T80, e);
    Txt(L"Buffy", DW * 0.5f - 100.f, cy + 300.f, 200.f, 28.f, F_UI, 13.f, 600, COL_T90, 1, e);

    float py = cy + 366.f;
    Fill(DW * 0.5f - 110.f, py, 220.f, 46.f, 23.f, C(WHITE, 0.07f * e));
    Stroke(DW * 0.5f - 110.f, py, 220.f, 46.f, 23.f, C(WHITE, 0.13f * e));
    Txt(L"Press Enter to unlock", DW * 0.5f - 105.f, py + 6.f, 210.f, 34.f, F_UI, 12.f, 500,
        COL_T70, 1, e);

    for (int i = 0; i < 4; ++i)
        Disc(DW * 0.5f - 27.f + i * 18.f, py + 72.f, 3.f, C(WHITE, 0.18f * e));

    AddHit(0.f, 0.f, DW, DH, H_UNLOCK);
}
// ============================================================================
//  Section 5 — window manager and per-app window content
// ============================================================================

static const float TITLE_H = 46.f;
static const wchar_t* WIN_TITLE[APP_COUNT] = { L"Files", L"Terminal", L"Settings", L"Text Editor", L"About" };
static const wchar_t  WIN_GLYPH[APP_COUNT] = { (wchar_t)0xE8B7, (wchar_t)0xE756, (wchar_t)0xE713,
                                               (wchar_t)0xE70F, (wchar_t)0xE946 };

static void Switch(float x, float y, float w, float h, bool on) {
    Fill(x, y, w, h, h * 0.5f, on ? C(COL_ACC, 0.92f) : C(WHITE, 0.16f));
    float kx = on ? (x + w - h * 0.5f) : (x + h * 0.5f);
    Disc(kx, y + h * 0.5f, h * 0.5f - 3.f, on ? C(0x0B0B0D, 0.95f) : C(COL_T90, 0.95f));
}

// ---- Files -----------------------------------------------------------------
static int g_fileHover = -1;

static void FilesContent(float x, float y, float w, float h, Win& win) {
    // breadcrumb
    Txt(L"Home", x + 20.f, y + 12.f, 60.f, 22.f, F_UI, 11.5f, 500, COL_T35, 0, 1.f);
    Txt(L"/", x + 66.f, y + 12.f, 16.f, 22.f, F_UI, 11.5f, 400, COL_T35, 0, 1.f);
    Txt(L"Projects", x + 82.f, y + 12.f, 200.f, 22.f, F_UI, 11.5f, 500, COL_T70, 0, 1.f);
    Line(x + 16.f, y + 40.f, x + w - 16.f, y + 40.f, C(WHITE, 0.055f));

    g_rt->PushAxisAlignedClip(R(x, y + 42.f, w, h - 42.f), D2D1_ANTIALIAS_MODE_ALIASED);
    float ry = y + 48.f - win.scroll;
    for (int i = 0; i < FILE_N; ++i) {
        if (ry > y + h + 20.f) break;
        bool hov = (g_fileHover == i);
        if (ry + 44.f > y + 40.f) {
            if (hov) Fill(x + 12.f, ry, w - 24.f, 44.f, 10.f, C(WHITE, 0.05f));
            Disc(x + 38.f, ry + 22.f, 15.f, C(WHITE, 0.05f));
            Glyph(FILES[i].glyph, x + 38.f, ry + 22.f, 11.f, FILES[i].folder ? COL_T90 : COL_T50);
            Txt(FILES[i].name, x + 62.f, ry + 6.f, w - 260.f, 22.f, F_UI, 12.f, 500, COL_T80, 0, 1.f);
            Txt(FILES[i].when, x + w - 210.f, ry + 6.f, 130.f, 22.f, F_UI, 11.f, 400, COL_T35, 2, 1.f);
            Txt(FILES[i].size, x + w - 76.f, ry + 6.f, 56.f, 22.f, F_MONO, 10.5f, 400, COL_T35, 2, 1.f);
            if (ry > y + 40.f && ry + 44.f < y + h)
                AddHit(x + 12.f, ry, w - 24.f, 44.f, H_FILE + i);
        }
        ry += 44.f;
    }
    g_rt->PopAxisAlignedClip();
}

// ---- Terminal --------------------------------------------------------------
static void TermPush(const std::wstring& s) { g_win[APP_TERM].lines.push_back(s); }

static void TermRun(const std::wstring& cmd) {
    Win& w = g_win[APP_TERM];
    TermPush(L"buffy@obsidian:~$ " + cmd);
    if (cmd.empty()) return;
    if (cmd == L"help") {
        TermPush(L"  help          show this list");
        TermPush(L"  ls            list the current folder");
        TermPush(L"  date          print the current date and time");
        TermPush(L"  whoami        print the current user");
        TermPush(L"  ver           shell version");
        TermPush(L"  echo <text>   print text back");
        TermPush(L"  clear         clear the screen");
    } else if (cmd == L"ls") {
        TermPush(L"  Projects/  Design/  obsidian_shell.cpp  palette.png  notes.md");
    } else if (cmd == L"date") {
        TermPush(L"  " + DateLong() + L"  " + TimeHMS());
    } else if (cmd == L"whoami") {
        TermPush(L"  buffy");
    } else if (cmd == L"ver") {
        TermPush(L"  Obsidian Shell " + std::wstring(VERSION) + L" (Direct2D)");
    } else if (cmd == L"clear") {
        w.lines.clear();
    } else if (cmd.size() >= 5 && cmd.substr(0, 5) == L"echo ") {
        TermPush(L"  " + cmd.substr(5));
    } else {
        TermPush(L"  command not found: " + cmd + L"  (try 'help')");
    }
    TermPush(L"");
}

static void TermContent(float x, float y, float w, float h, Win& win) {
    const float lineH = 21.f;
    std::vector<std::wstring>& L = win.lines;
    int promptLines = 1;
    int visible = (int)((h - 16.f) / lineH);
    if (visible < 1) visible = 1;
    int total = (int)L.size() + promptLines;
    int first = total > visible ? total - visible : 0;

    g_rt->PushAxisAlignedClip(R(x, y, w, h), D2D1_ANTIALIAS_MODE_ALIASED);
    float ry = y + 8.f;
    for (int i = first; i < (int)L.size(); ++i) {
        Txt(L[i], x + 18.f, ry, w - 36.f, lineH, F_MONO, 12.f, 400,
            L[i].compare(0, 2, L"  ") == 0 ? COL_T70 : COL_T90, 0, 0.95f);
        ry += lineH;
    }
    // live prompt
    std::wstring prompt = L"buffy@obsidian:~$ ";
    Txt(prompt, x + 18.f, ry, w - 36.f, lineH, F_MONO, 12.f, 400, COL_ACC, 0, 0.95f);
    float pw = 18.f + (float)prompt.size() * 7.35f;
    Txt(win.input, x + pw, ry, w - pw - 40.f, lineH, F_MONO, 12.f, 400, COL_T100, 0, 1.f);
    if (std::fmod(g_clockT, 1.06) < 0.55) {
        float cx = x + pw + (float)win.input.size() * 7.35f;
        Fill(cx, ry + 3.f, 8.f, lineH - 5.f, 1.f, C(COL_T100, 0.75f));
    }
    g_rt->PopAxisAlignedClip();
}

// ---- Settings --------------------------------------------------------------
// A small navigation rail plus three pages: System, Appearance, Advanced.
// Appearance is where the shell's look is actually editable — wallpaper, the
// size of every desktop widget, which widgets exist, clock format and motion.
static const wchar_t* SET_PAGES[3] = { L"System", L"Appearance", L"Advanced" };

static const wchar_t* SET_SIZE_LABEL[3] = { L"Compact", L"Comfortable", L"Large" };

static void WallpaperSwatch(float x, float y, float w, float h, int idx, bool sel, bool hov) {
    const WpPreset& p = WP[idx];
    Fill(x, y, w, h, 12.f, C(p.b, 1.f));
    // Two bands approximate the three-stop gradient without building a brush
    // per swatch per frame.
    Fill(x + 1.f, y + 1.f, w - 2.f, h * 0.44f, 11.f, C(p.a, 0.92f));
    Fill(x + 1.f, y + h * 0.60f, w - 2.f, h * 0.39f, 11.f, C(p.c, 0.92f));
    Disc(x + w - 30.f, y + 22.f, 30.f, C(p.glow, 0.30f));
    Stroke(x, y, w, h, 12.f, C(WHITE, sel ? 0.62f : (hov ? 0.18f : 0.08f)), sel ? 1.6f : 1.f);
    Txt(p.name, x + 10.f, y + h - 26.f, w - 20.f, 18.f, F_UI, 10.5f, 500,
        sel ? COL_T100 : COL_T70, 0, 1.f);
    if (sel) {
        Disc(x + 17.f, y + 17.f, 8.f, C(COL_ACC, 0.95f));
        Glyph((wchar_t)0xE73E, x + 17.f, y + 17.f, 6.f, 0x0B0B0D);
    }
    AddHit(x, y, w, h, H_SET_WP + idx);
}

static void SegButton(float x, float y, float w, float h, const wchar_t* label, bool on, bool hov, int id) {
    Fill(x, y, w, h, 10.f, on ? C(COL_T100, 0.95f) : C(WHITE, hov ? 0.08f : 0.04f));
    if (!on) Stroke(x, y, w, h, 10.f, C(WHITE, 0.09f));
    Txt(label, x, y + (h - 16.f) * 0.5f, w, 16.f, F_UI, 11.f, 600,
        on ? 0x101012 : COL_T80, 1, 1.f);
    AddHit(x, y, w, h, id);
}

static void SettingsRow(float x, float y, float w, const wchar_t* label, bool on, int id, bool hov) {
    if (hov) Fill(x, y, w, 34.f, 9.f, C(WHITE, 0.045f));
    Txt(label, x + 10.f, y + 7.f, w - 74.f, 20.f, F_UI, 11.5f, 500,
        on ? COL_T90 : COL_T50, 0, 1.f);
    Switch(x + w - 52.f, y + 7.f, 40.f, 20.f, on);
    AddHit(x, y, w, 34.f, id);
}

static void ResetAppearance() {
    int ns = 0;
    const float* scales = shelllayout::Scales(ns);
    g_sizeIdx = 1;
    g_widgetScale = scales[1];
    g_textPct = TEXT_PCT_DEFAULT;
    for (int i = 0; i < shelllayout::W_COUNT; ++i) g_widgetOn[i] = true;
    g_clock24 = false;
    g_motion = true;
    if (g_wp == 0) { SafeRelease(g_wpA); BuildWallpaperBrushes(); }
    else SetWallpaper(0);
    SaveSettings();
    ShowToast(L"Appearance reset to defaults", (wchar_t)0xE72C);
}

static void SettingsContent(float x, float y, float w, float h) {
    const float navW = 176.f;
    Line(x + navW, y + 16.f, x + navW, y + h - 16.f, C(WHITE, 0.055f));
    Txt(L"Settings", x + 24.f, y + 20.f, navW - 44.f, 30.f, F_SEMI, 16.f, 600, COL_T100, 0, 1.f);

    for (int i = 0; i < 3; ++i) {
        float ry = y + 66.f + (float)i * 42.f;
        bool on = (g_setPage == i);
        bool hov = (g_hover == H_SET_NAV + i);
        if (on || hov) Fill(x + 12.f, ry, navW - 24.f, 34.f, 9.f, C(WHITE, on ? 0.09f : 0.04f));
        if (on) Fill(x + 12.f, ry, 3.f, 34.f, 1.5f, C(COL_ACTIVE, 0.9f));
        Txt(SET_PAGES[i], x + 26.f, ry + 7.f, navW - 60.f, 20.f, F_UI, 11.5f,
            on ? 600 : 500, on ? COL_T100 : COL_T70, 0, 1.f);
        AddHit(x + 12.f, ry, navW - 24.f, 34.f, H_SET_NAV + i);
    }
    Txt(L"Obsidian Shell " + std::wstring(VERSION), x + 24.f, y + h - 32.f, navW - 44.f, 20.f,
        F_UI, 10.f, 400, COL_T35, 0, 1.f);

    const float cx = x + navW + 26.f;
    const float cw = w - navW - 52.f;
    const float ry = y + 20.f;

    if (g_setPage == 0) {
        // ---- System -------------------------------------------------------
        Txt(L"System", cx, ry, cw, 30.f, F_SEMI, 17.f, 600, COL_T100, 0, 1.f);
        Txt(L"Connectivity, notifications and output levels.", cx, ry + 28.f, cw, 20.f,
            F_UI, 11.f, 400, COL_T35, 0, 1.f);

        static const int rows[] = { TOG_WIFI, TOG_BT, TOG_DND, TOG_NIGHT, TOG_SAVER };
        float ty = ry + 60.f;
        for (int i = 0; i < 5; ++i) {
            int t = rows[i];
            bool on = g_tog[t];
            bool hov = (g_hover == H_TOG0 + t);
            if (hov) Fill(cx - 8.f, ty, cw + 16.f, 52.f, 10.f, C(WHITE, 0.04f));
            Disc(cx + 18.f, ty + 26.f, 15.f, C(WHITE, 0.055f));
            Glyph(TOG_GLYPH[t], cx + 18.f, ty + 26.f, 11.f, on ? COL_T90 : COL_T50);
            Txt(TOG_NAME[t], cx + 44.f, ty + 8.f, cw - 120.f, 22.f, F_UI, 12.f, 500, COL_T80, 0, 1.f);
            Txt(g_togSub[t].c_str(), cx + 44.f, ty + 27.f, cw - 120.f, 20.f, F_UI, 10.5f, 400, COL_T35, 0, 1.f);
            Switch(cx + cw - 44.f, ty + 16.f, 44.f, 21.f, on);
            AddHit(cx - 8.f, ty, cw + 16.f, 52.f, H_TOG0 + t);
            ty += 52.f;
        }
        Line(cx - 4.f, ty + 8.f, cx + cw + 4.f, ty + 8.f, C(WHITE, 0.055f));
        Txt(L"Volume", cx, ty + 22.f, 90.f, 22.f, F_UI, 11.5f, 500, COL_T50, 0, 1.f);
        SliderDraw(cx + 80.f, ty + 16.f, cw - 96.f, g_vol, H_VOL, 7.f);
        Txt(L"Brightness", cx, ty + 58.f, 90.f, 22.f, F_UI, 11.5f, 500, COL_T50, 0, 1.f);
        SliderDraw(cx + 80.f, ty + 52.f, cw - 96.f, g_bri, H_BRI, 7.f);
        Txt(L"These toggles are shell state, not device control.", cx, ty + 92.f, cw, 20.f,
            F_UI, 10.f, 400, COL_T35, 0, 1.f);
    } else if (g_setPage == 1) {
        // ---- Appearance ---------------------------------------------------
        const float half = (cw - 28.f) * 0.5f;
        const float lx = cx, rx = cx + half + 28.f;

        Txt(L"WALLPAPER", lx, ry, half, 18.f, F_UI, 10.f, 600, COL_T35, 0, 1.f);
        {
            const float sw = (half - 10.f) * 0.5f, sh = 52.f;
            for (int i = 0; i < WP_COUNT; ++i) {
                float px = lx + (float)(i % 2) * (sw + 10.f);
                float py = ry + 24.f + (float)(i / 2) * (sh + 8.f);
                WallpaperSwatch(px, py, sw, sh, i, i == g_wp, g_hover == H_SET_WP + i);
            }
        }
        float customBtnY = ry + 24.f + 4.f * 60.f + 6.f;
        bool hovCustom = (g_hover == H_SET_CUSTOM_WP);
        Fill(lx, customBtnY, half, 32.f, 9.f, C(WHITE, hovCustom ? 0.10f : 0.05f));
        Stroke(lx, customBtnY, half, 32.f, 9.f, C(WHITE, hovCustom ? 0.22f : 0.12f));
        Glyph((wchar_t)0xE8B9, lx + 20.f, customBtnY + 16.f, 12.f, COL_T90);
        Txt(L"Browse Custom Image...", lx + 38.f, customBtnY + 7.f, half - 48.f, 18.f, F_UI, 11.f, 500, COL_T90, 0, 1.f);
        AddHit(lx, customBtnY, half, 32.f, H_SET_CUSTOM_WP);

        float ly = customBtnY + 44.f;
        Txt(L"WIDGET SIZE", lx, ly, half, 18.f, F_UI, 10.f, 600, COL_T35, 0, 1.f);
        {
            const float bw = (half - 16.f) / 3.f;
            for (int i = 0; i < 3; ++i) {
                SegButton(lx + (float)i * (bw + 8.f), ly + 22.f, bw, 30.f, SET_SIZE_LABEL[i],
                          g_sizeIdx == i, g_hover == H_SET_SIZE + i, H_SET_SIZE + i);
            }
        }
        ly += 64.f;
        SettingsRow(lx, ly, half, L"24-hour clock", g_clock24, H_SET_MISC + 0,
                    g_hover == H_SET_MISC + 0);
        SettingsRow(lx, ly + 36.f, half, L"Reduce motion", !g_motion, H_SET_MISC + 1,
                    g_hover == H_SET_MISC + 1);

        Txt(L"DESKTOP WIDGETS", rx, ry, half, 18.f, F_UI, 10.f, 600, COL_T35, 0, 1.f);
        Txt(L"Shown", rx + half - 60.f, ry, 60.f, 18.f, F_UI, 10.f, 400, COL_T35, 2, 1.f);
        for (int i = 0; i < shelllayout::W_COUNT; ++i) {
            SettingsRow(rx, ry + 24.f + (float)i * 36.f, half, WIDGET_NAME[i], g_widgetOn[i],
                        H_SET_WIDGET + i, g_hover == H_SET_WIDGET + i);
        }
        Txt(L"Widgets flow into three columns; hidden ones close the gap.", rx, ry + 24.f + 8.f * 36.f,
            half, 34.f, F_UI, 10.f, 400, COL_T35, 0, 0.9f);
    } else {
        // ---- Advanced -----------------------------------------------------
        Txt(L"Advanced", cx, ry, cw, 30.f, F_SEMI, 17.f, 600, COL_T100, 0, 1.f);
        Txt(L"Where things live, and how to put them back.", cx, ry + 28.f, cw, 20.f,
            F_UI, 11.f, 400, COL_T35, 0, 1.f);

        Txt(L"SETTINGS FILE", cx, ry + 66.f, cw, 18.f, F_UI, 10.f, 600, COL_T35, 0, 1.f);
        Txt(SettingsPath(), cx, ry + 86.f, cw, 22.f, F_MONO, 10.5f, 400, COL_T70, 0, 1.f);
        Txt(L"Plain INI: delete it to return to first-run defaults.", cx, ry + 108.f, cw, 20.f,
            F_UI, 10.5f, 400, COL_T35, 0, 1.f);

        Txt(L"SHELL", cx, ry + 144.f, cw, 18.f, F_UI, 10.f, 600, COL_T35, 0, 1.f);
        Txt(L"Obsidian Shell " + std::wstring(VERSION) + L"  ·  Direct2D / DirectWrite  ·  x64",
            cx, ry + 164.f, cw, 20.f, F_UI, 11.f, 400, COL_T70, 0, 1.f);
        Txt(L"Ctrl+Space launcher  ·  F11 fullscreen  ·  Ctrl+Q quit", cx, ry + 186.f, cw, 20.f,
            F_UI, 11.f, 400, COL_T50, 0, 1.f);

        bool hov = (g_hover == H_SET_RESET);
        Fill(cx, ry + 232.f, 268.f, 38.f, 11.f, C(WHITE, hov ? 0.10f : 0.05f));
        Stroke(cx, ry + 232.f, 268.f, 38.f, 11.f, C(WHITE, hov ? 0.20f : 0.10f));
        Glyph((wchar_t)0xE72C, cx + 24.f, ry + 251.f, 11.f, COL_T80);
        Txt(L"Reset appearance to defaults", cx + 44.f, ry + 242.f, 210.f, 20.f, F_UI, 11.5f, 500,
            COL_T80, 0, 1.f);
        AddHit(cx, ry + 232.f, 268.f, 38.f, H_SET_RESET);
        Txt(L"Wallpaper, widget size, widget visibility, clock and motion.", cx, ry + 280.f, cw, 20.f,
            F_UI, 10.f, 400, COL_T35, 0, 1.f);
    }
}

// ---- Text editor -----------------------------------------------------------
static void EditorContent(float x, float y, float w, float h, Win& win) {
    const float lineH = 22.f;
    g_rt->PushAxisAlignedClip(R(x, y, w, h), D2D1_ANTIALIAS_MODE_ALIASED);
    float ry = y + 10.f;
    for (int i = 0; i < (int)win.lines.size(); ++i) {
        Txt(Pad2(i + 1), x + 14.f, ry, 28.f, lineH, F_MONO, 10.f, 400, COL_T35, 2, 0.9f);
        Txt(win.lines[i], x + 52.f, ry, w - 70.f, lineH, F_MONO, 12.f, 400, COL_T80, 0, 0.95f);
        ry += lineH;
    }
    Txt(win.input, x + 52.f, ry, w - 70.f, lineH, F_MONO, 12.f, 400, COL_T90, 0, 1.f);
    if (std::fmod(g_clockT, 1.06) < 0.55) {
        float cx = x + 52.f + (float)win.input.size() * 7.35f;
        Fill(cx, ry + 3.f, 8.f, lineH - 5.f, 1.f, C(COL_ACC, 0.8f));
    }
    g_rt->PopAxisAlignedClip();
}

// ---- About -----------------------------------------------------------------
static void AboutContent(float x, float y, float w, float h) {
    g_rt->PushAxisAlignedClip(R(x, y, w, h), D2D1_ANTIALIAS_MODE_ALIASED);
    Disc(x + 74.f, y + 78.f, 36.f, C(WHITE, 0.05f));
    Ring(x + 74.f, y + 78.f, 36.f, C(WHITE, 0.10f));
    Glyph((wchar_t)0xE790, x + 74.f, y + 78.f, 26.f, COL_T90);

    Txt(L"Obsidian Shell", x + 130.f, y + 50.f, w - 170.f, 40.f, F_SEMI, 24.f, 600, COL_T100, 0, 1.f);
    Txt(L"Version " + std::wstring(VERSION) + L"  ·  Direct2D / DirectWrite",
        x + 130.f, y + 88.f, w - 170.f, 24.f, F_UI, 11.5f, 400, COL_T50, 0, 1.f);
    Txt(L"One translation unit, no external libraries, no image files.",
        x + 130.f, y + 108.f, w - 170.f, 24.f, F_UI, 11.f, 400, COL_T35, 0, 1.f);

    float ry = y + 156.f;
    static const wchar_t* bullets[] = {
        L"Six wallpapers, picker in Settings \u203A Appearance",
        L"Eight desktop widgets, three sizes, per-widget visibility",
        L"Shelf, launcher, unified system bubble, window manager",
        L"12/24-hour clock, reduce-motion, toasts, lock screen",
        L"Everything persisted to shell.ini in your profile",
    };
    for (int i = 0; i < 5; ++i) {
        Disc(x + 32.f, ry + 10.f, 2.f, C(COL_ACC, 0.9f));
        Txt(bullets[i], x + 46.f, ry, w - 90.f, 22.f, F_UI, 11.5f, 400, COL_T70, 0, 0.95f);
        ry += 28.f;
    }

    // palette chip strip
    unsigned chips[6] = { COL_ACC, COL_ACTIVE, COL_SEC, COL_TILE, COL_SURF, 0x090A0D };
    Txt(L"Palette", x + 30.f, ry + 12.f, 100.f, 20.f, F_UI, 10.f, 600, COL_T35, 0, 1.f);
    for (int i = 0; i < 6; ++i) {
        float cx = x + 100.f + (float)i * 30.f;
        Fill(cx, ry + 10.f, 22.f, 22.f, 6.f, C(chips[i], 0.98f));
        Stroke(cx, ry + 10.f, 22.f, 22.f, 6.f, C(WHITE, 0.10f));
    }
    Txt(L"Built for Windows 10 / 11  ·  x64", x + 28.f, y + h - 40.f, w - 56.f, 22.f,
        F_UI, 10.5f, 400, COL_T35, 0, 0.9f);
    g_rt->PopAxisAlignedClip();
}

// ----------------------------------------------------------------------------
//  Window chrome
// ----------------------------------------------------------------------------
static void DrawWindowChrome(int kind, float x, float y, float w, float h, bool focused) {
    Shadow(x, y, w, h, 20.f, focused ? 0.075f : 0.05f, 8);
    Fill(x, y, w, h, 20.f, C(COL_SURF, focused ? 0.985f : 0.93f));
    Stroke(x, y, w, h, 20.f, C(WHITE, focused ? 0.11f : 0.05f));

    // Registered first so that every later region takes priority over the body.
    AddHit(x, y, w, h, H_WIN + kind * 8 + WP_BODY);
    AddHit(x, y, w, TITLE_H, H_WIN + kind * 8 + WP_TITLE);

    // title bar
    Fill(x + 1.f, y + 1.f, w - 2.f, TITLE_H, 19.f, C(WHITE, focused ? 0.035f : 0.02f));
    Line(x + 1.f, y + TITLE_H + 1.f, x + w - 1.f, y + TITLE_H + 1.f, C(WHITE, 0.07f));
    Glyph(WIN_GLYPH[kind], x + 24.f, y + TITLE_H * 0.5f + 1.f, 11.f, focused ? COL_T90 : COL_T50);
    Txt(WIN_TITLE[kind], x + 42.f, y + 8.f, w - 170.f, TITLE_H - 14.f, F_UI, 12.f, 600,
        focused ? COL_T90 : COL_T50, 0, 1.f);

    // window buttons
    float bx = x + w - 22.f;
    struct B { int part; wchar_t glyph; };
    B btns[3] = { { WP_CLOSE, (wchar_t)0xE711 }, { WP_MAX, (wchar_t)(g_win[kind].maxed ? 0xE923 : 0xE922) }, { WP_MIN, (wchar_t)0xE921 } };
    for (int i = 0; i < 3; ++i) {
        int id = H_WIN + kind * 8 + btns[i].part;
        bool hov = (g_hover == id);
        if (hov) Disc(bx, y + TITLE_H * 0.5f + 1.f, 13.f, C(WHITE, 0.10f));
        Glyph(btns[i].glyph, bx, y + TITLE_H * 0.5f + 1.f, 8.5f, hov ? COL_T100 : COL_T70);
        AddHit(bx - 15.f, y + 8.f, 30.f, 30.f, id);
        bx -= 34.f;
    }

    // resize edges (registered last so they win over everything else)
    AddHit(x + w - 8.f, y + 8.f, 8.f, h - 16.f, H_WIN + kind * 8 + WP_RIGHT);
    AddHit(x + 8.f, y + h - 8.f, w - 16.f, 8.f, H_WIN + kind * 8 + WP_BOTTOM);
    AddHit(x + w - 14.f, y + h - 14.f, 14.f, 14.f, H_WIN + kind * 8 + WP_CORNER);
}

static void DrawWindow(int kind) {
    Win& win = g_win[kind];
    if (!win.open || win.min) return;

    bool focused = (g_focus == kind);
    float x = win.x, y = win.y, w = win.w, h = win.h;
    DrawWindowChrome(kind, x, y, w, h, focused);

    float cx = x + 1.f, cy = y + TITLE_H + 2.f, cw = w - 2.f, ch = h - TITLE_H - 3.f;
    g_rt->PushAxisAlignedClip(R(cx, cy, cw, ch), D2D1_ANTIALIAS_MODE_ALIASED);
    switch (kind) {
    case APP_FILES:     FilesContent(cx, cy, cw, ch, win); break;
    case APP_TERM:      TermContent(cx, cy, cw, ch, win); break;
    case APP_SETTINGS:  SettingsContent(cx, cy, cw, ch); break;
    case APP_EDITOR:    EditorContent(cx, cy, cw, ch, win); break;
    case APP_ABOUT:     AboutContent(cx, cy, cw, ch); break;
    default: break;
    }
    g_rt->PopAxisAlignedClip();

    if (win.maxed) {
        Line(x + 20.f, y + h - 1.5f, x + w - 20.f, y + h - 1.5f, C(WHITE, 0.10f));
    }
}

// Snap preview while a window is being dragged to an edge.
static void DrawSnapHint() {
    if (g_dragWin < 0 || g_dragPart != WP_TITLE) return;
    Win& win = g_win[g_dragWin];
    float x = win.x, y = win.y, w = win.w;
    D2D1_RECT_F hint = R(0, 0, 0, 0);
    bool show = false;
    if (x < 12.f) { hint = R(20.f, 20.f, DW * 0.5f - 30.f, DH - 130.f); show = true; }
    else if (x + w > DW - 12.f) { hint = R(DW * 0.5f + 10.f, 20.f, DW * 0.5f - 30.f, DH - 130.f); show = true; }
    else if (y < 12.f) { hint = R(20.f, 20.f, DW - 40.f, DH - 130.f); show = true; }
    if (!show) return;
    Fill(hint.left, hint.top, hint.right - hint.left, hint.bottom - hint.top, 20.f, C(WHITE, 0.045f));
    Stroke(hint.left, hint.top, hint.right - hint.left, hint.bottom - hint.top, 20.f, C(WHITE, 0.14f));
}
// ============================================================================
//  Section 6 — shell actions, render loop, input routing, WinMain
// ============================================================================

static void ShowToast(const std::wstring& text, wchar_t glyph) {
    Toast t;
    t.text = text;
    t.glyph = glyph;
    t.t = 0.f;
    g_toasts.push_back(t);
    if (g_toasts.size() > 4) g_toasts.erase(g_toasts.begin());
}

static void ClosePanels() {
    g_panel = PANEL_NONE;
    g_ctxOpen = false;
}

static void FocusWindow(int kind) {
    g_focus = kind;
    auto it = std::find(g_z.begin(), g_z.end(), kind);
    if (it != g_z.end()) g_z.erase(it);
    g_z.push_back(kind);
}

static void ResetWindow(int kind) {
    Win& w = g_win[kind];
    w.w = 900.f;
    w.h = 600.f;
    w.x = 470.f + (float)kind * 36.f;
    w.y = 190.f + (float)kind * 34.f;
    w.maxed = false;
    float maxX = DW - 40.f - w.w, maxY = SHELF_Y - 24.f - w.h;
    if (w.x > maxX) w.x = maxX < 40.f ? 40.f : maxX;
    if (w.y > maxY) w.y = maxY < 60.f ? 60.f : maxY;
    if (w.x < 20.f) w.x = 20.f;
    if (w.y < 20.f) w.y = 20.f;
}

static void MaximizeWindow(int kind) {
    Win& w = g_win[kind];
    if (!w.maxed) {
        w.rx = w.x; w.ry = w.y; w.rw = w.w; w.rh = w.h;
        w.x = 20.f; w.y = 20.f; w.w = DW - 40.f; w.h = SHELF_Y - 44.f;
        w.maxed = true;
    } else {
        w.x = w.rx; w.y = w.ry; w.w = w.rw; w.h = w.rh;
        w.maxed = false;
    }
}

static void OpenApp(int kind) {
    if (kind < 0 || kind >= APP_COUNT) return;
    Win& w = g_win[kind];
    if (!w.open) {
        w.open = true;
        w.min = false;
        ResetWindow(kind);
        g_z.push_back(kind);
    }
    w.min = false;
    FocusWindow(kind);
    ClosePanels();
    ShowToast(std::wstring(L"Opened ") + WIN_TITLE[kind], WIN_GLYPH[kind]);
}

static bool TryShellExec(const wchar_t* target, const wchar_t* params = nullptr) {
    if (!target || !target[0]) return false;
    HINSTANCE hInst = ShellExecuteW(nullptr, L"open", target, params, nullptr, SW_SHOWNORMAL);
    return ((INT_PTR)hInst > 32);
}

static void LaunchRealApp(const AppDef& app) {
    bool ok = false;
    if (app.exec && app.exec[0]) {
        ok = TryShellExec(app.exec);
        if (!ok && app.fallback && app.fallback[0]) {
            ok = TryShellExec(app.fallback);
        }
    }
    if (ok) {
        ShowToast(std::wstring(L"Launched ") + app.name, app.glyph ? app.glyph : (wchar_t)0xE756);
    } else if (app.kind >= 0) {
        OpenApp(app.kind);
    } else {
        ShowToast(std::wstring(L"Unable to launch ") + app.name, (wchar_t)0xE783);
    }
}

static void LaunchViewIndex(int vi) {
    if (vi < 0 || vi >= (int)g_view.size()) return;
    LaunchRealApp(APPS[g_view[vi]]);
    ClosePanels();
}

static void RunContextCommand(int cmd) {
    g_ctxOpen = false;
    switch (cmd) {
    case CTX_CUSTOM_WALL: PromptCustomWallpaper(g_hwnd); break;
    case CTX_WALL:        SetWallpaper(g_wp + 1); break;
    case CTX_PERS:   g_setPage = 1; OpenApp(APP_SETTINGS); break;
    case CTX_LAUNCH: g_panel = PANEL_LAUNCH; break;
    case CTX_SYS:    g_panel = PANEL_SYS; break;
    case CTX_FULL:   ToggleFullscreen(g_hwnd); break;
    case CTX_LOCK:   g_locked = true; g_panel = PANEL_NONE; break;
    case CTX_QUIT:   PostMessageW(g_hwnd, WM_CLOSE, 0, 0); break;
    default: break;
    }
}

// ----------------------------------------------------------------------------
//  Real Wi-Fi & Bluetooth state (Pure Windows Native Wifi & Bluetooth APIs)
// ----------------------------------------------------------------------------
static void SampleWifi() {
    HANDLE hClient = NULL;
    DWORD dwCurVersion = 0;
    DWORD dwResult = WlanOpenHandle(2, NULL, &dwCurVersion, &hClient);
    if (dwResult == ERROR_SUCCESS) {
        PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
        bool connected = false;
        if (WlanEnumInterfaces(hClient, NULL, &pIfList) == ERROR_SUCCESS) {
            for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
                PWLAN_INTERFACE_INFO pIfInfo = (PWLAN_INTERFACE_INFO)&pIfList->InterfaceInfo[i];
                PWLAN_CONNECTION_ATTRIBUTES pConn = NULL;
                DWORD dwSize = 0;
                WLAN_OPCODE_VALUE_TYPE opType;
                if (WlanQueryInterface(hClient, &pIfInfo->InterfaceGuid, wlan_intf_opcode_current_connection, NULL, &dwSize, (PVOID*)&pConn, &opType) == ERROR_SUCCESS) {
                    if (pConn->isState == wlan_interface_state_connected) {
                        DOT11_SSID ssid = pConn->wlanAssociationAttributes.dot11Ssid;
                        char s[33] = {0};
                        memcpy(s, ssid.ucSSID, std::min<size_t>(ssid.uSSIDLength, 32));
                        std::string sStr = s;
                        g_wifiSSID = std::wstring(sStr.begin(), sStr.end());
                        g_wifiSignal = (int)pConn->wlanAssociationAttributes.wlanSignalQuality;
                        g_togSub[TOG_WIFI] = g_wifiSSID + L" \u2022 " + std::to_wstring(g_wifiSignal) + L"%";
                        g_tog[TOG_WIFI] = true;
                        connected = true;
                    }
                    WlanFreeMemory(pConn);
                }
                if (connected) break;
            }
            WlanFreeMemory(pIfList);
        }
        if (!connected) {
            g_tog[TOG_WIFI] = false;
            g_togSub[TOG_WIFI] = L"Disconnected";
        }
        WlanCloseHandle(hClient, NULL);
    } else {
        g_tog[TOG_WIFI] = false;
        g_togSub[TOG_WIFI] = L"Off";
    }
}

static void SampleBluetooth() {
    BLUETOOTH_FIND_RADIO_PARAMS params = { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) };
    HANDLE hRadio = NULL;
    HBLUETOOTH_RADIO_FIND hFind = BluetoothFindFirstRadio(&params, &hRadio);
    if (hFind != NULL) {
        g_tog[TOG_BT] = true;
        BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = { sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS) };
        searchParams.hRadio = hRadio;
        searchParams.fReturnAuthenticated = TRUE;
        searchParams.fReturnConnected = TRUE;
        searchParams.fReturnRemembered = TRUE;
        searchParams.fReturnUnknown = FALSE;
        searchParams.fIssueInquiry = FALSE;
        searchParams.cTimeoutMultiplier = 1;
        BLUETOOTH_DEVICE_INFO devInfo = { sizeof(BLUETOOTH_DEVICE_INFO) };
        HBLUETOOTH_DEVICE_FIND hDevFind = BluetoothFindFirstDevice(&searchParams, &devInfo);
        int devCount = 0;
        int connectedCount = 0;
        std::wstring firstConnected = L"";
        if (hDevFind != NULL) {
            do {
                devCount++;
                if (devInfo.fConnected) {
                    connectedCount++;
                    if (firstConnected.empty()) firstConnected = devInfo.szName;
                }
            } while (BluetoothFindNextDevice(hDevFind, &devInfo));
            BluetoothFindDeviceClose(hDevFind);
        }
        if (connectedCount > 0) {
            g_togSub[TOG_BT] = firstConnected;
        } else if (devCount > 0) {
            g_togSub[TOG_BT] = std::to_wstring(devCount) + L" paired";
        } else {
            g_togSub[TOG_BT] = L"On \u2022 No devices";
        }
        CloseHandle(hRadio);
        BluetoothFindRadioClose(hFind);
    } else {
        g_tog[TOG_BT] = false;
        g_togSub[TOG_BT] = L"Off";
    }
}

// ----------------------------------------------------------------------------
//  HTTP Client (Pure WinHttp, non-blocking asynchronous fetcher)
// ----------------------------------------------------------------------------
static std::string HttpGet(const wchar_t* host, const wchar_t* path, bool https = true) {
    std::string res;
    HINTERNET hSession = WinHttpOpen(L"ObsidianShell/2.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return res;
    HINTERNET hConnect = WinHttpConnect(hSession, host, https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return res; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return res; }

    DWORD timeout = 4000;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        if (WinHttpReceiveResponse(hRequest, NULL)) {
            DWORD dwSize = 0;
            do {
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) break;
                char* buf = new char[dwSize + 1];
                DWORD dwDownloaded = 0;
                if (WinHttpReadData(hRequest, (LPVOID)buf, dwSize, &dwDownloaded)) {
                    buf[dwDownloaded] = '\0';
                    res.append(buf, dwDownloaded);
                }
                delete[] buf;
            } while (dwSize > 0);
        }
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return res;
}

// ----------------------------------------------------------------------------
//  Background thread for real-time Bitcoin & Weather updates
// ----------------------------------------------------------------------------
static void NetWorkerThread() {
    int btcTimer = 0;
    int weatherTimer = 0;
    while (g_netRunning) {
        if (btcTimer <= 0) {
            btcTimer = 30;
            std::string btcJson = HttpGet(L"api.coinbase.com", L"/v2/prices/BTC-USD/historic?period=day");
            if (!btcJson.empty()) {
                std::vector<float> allPrices;
                size_t pos = 0;
                while ((pos = btcJson.find("\"price\":\"", pos)) != std::string::npos) {
                    pos += 9;
                    size_t end = btcJson.find('"', pos);
                    if (end == std::string::npos) break;
                    std::string s = btcJson.substr(pos, end - pos);
                    float p = (float)atof(s.c_str());
                    if (p > 0.f) allPrices.push_back(p);
                    pos = end + 1;
                }
                if (!allPrices.empty()) {
                    float cur = allPrices.front();
                    float oldest = allPrices.back();
                    float chg = ((cur - oldest) / (oldest > 0.01f ? oldest : 1.f)) * 100.f;

                    long long intPrice = (long long)cur;
                    std::string sPrice = std::to_string(intPrice);
                    int n = (int)sPrice.length();
                    std::string formatted = "$";
                    for (int i = 0; i < n; ++i) {
                        formatted += sPrice[i];
                        if ((n - i - 1) % 3 == 0 && i < n - 1) formatted += ",";
                    }

                    char chgBuf[32];
                    snprintf(chgBuf, 32, "%s%.2f%%", chg >= 0.f ? "+" : "", chg);

                    {
                        std::lock_guard<std::mutex> lock(g_btcLock);
                        g_btcPriceStr = std::wstring(formatted.begin(), formatted.end());
                        g_btcChangeStr = std::wstring(chgBuf, chgBuf + strlen(chgBuf));
                        g_btcChangeVal = chg;

                        for (int i = 0; i < 24; ++i) {
                            int idx = (int)((allPrices.size() - 1) * (1.0f - (float)i / 23.0f));
                            if (idx < 0) idx = 0;
                            if (idx >= (int)allPrices.size()) idx = (int)allPrices.size() - 1;
                            g_btcSpark[i] = allPrices[idx];
                        }
                    }
                    if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE);
                }
            }
        }

        if (weatherTimer <= 0) {
            weatherTimer = 900;
            std::string wJson = HttpGet(L"wttr.in", L"/?format=j1");
            if (!wJson.empty()) {
                auto findVal = [&](const std::string& key) -> std::string {
                    size_t p = wJson.find("\"" + key + "\":\"");
                    if (p == std::string::npos) p = wJson.find("\"" + key + "\": \"");
                    if (p == std::string::npos) return "";
                    p = wJson.find('"', p + key.length() + 2);
                    if (p == std::string::npos) return "";
                    p++;
                    size_t e = wJson.find('"', p);
                    if (e == std::string::npos) return "";
                    return wJson.substr(p, e - p);
                };

                std::string sTempC = findVal("temp_C");
                std::string sHighC = findVal("maxtempC");
                std::string sLowC  = findVal("mintempC");
                std::string sCond = "";
                size_t descPos = wJson.find("\"weatherDesc\"");
                if (descPos != std::string::npos) {
                    size_t valPos = wJson.find("\"value\":\"", descPos);
                    if (valPos != std::string::npos) {
                        valPos += 9;
                        size_t end = wJson.find('"', valPos);
                        if (end != std::string::npos) sCond = wJson.substr(valPos, end - valPos);
                    }
                }
                std::string sCity = "";
                size_t areaPos = wJson.find("\"areaName\"");
                if (areaPos != std::string::npos) {
                    size_t valPos = wJson.find("\"value\":\"", areaPos);
                    if (valPos != std::string::npos) {
                        valPos += 9;
                        size_t end = wJson.find('"', valPos);
                        if (end != std::string::npos) sCity = wJson.substr(valPos, end - valPos);
                    }
                }

                if (!sTempC.empty()) {
                    std::lock_guard<std::mutex> lock(g_weatherLock);
                    g_weatherTempStr = std::wstring(sTempC.begin(), sTempC.end()) + L"\u00B0";
                    if (!sCond.empty()) g_weatherCond = std::wstring(sCond.begin(), sCond.end());
                    if (!sHighC.empty() && !sLowC.empty()) {
                        g_weatherHighLow = L"H " + std::wstring(sHighC.begin(), sHighC.end()) + L"\u00B0   L " +
                                           std::wstring(sLowC.begin(), sLowC.end()) + L"\u00B0";
                    }
                    if (!sCity.empty()) g_weatherCity = std::wstring(sCity.begin(), sCity.end());

                    std::string lowerCond = sCond;
                    for (char& ch : lowerCond) ch = (char)tolower(ch);
                    if (lowerCond.find("rain") != std::string::npos || lowerCond.find("drizzle") != std::string::npos || lowerCond.find("shower") != std::string::npos)
                        g_weatherGlyph = (wchar_t)0xE77B;
                    else if (lowerCond.find("thunder") != std::string::npos || lowerCond.find("storm") != std::string::npos)
                        g_weatherGlyph = (wchar_t)0xE7C4;
                    else if (lowerCond.find("snow") != std::string::npos || lowerCond.find("sleet") != std::string::npos || lowerCond.find("ice") != std::string::npos)
                        g_weatherGlyph = (wchar_t)0xE7C5;
                    else if (lowerCond.find("cloud") != std::string::npos || lowerCond.find("overcast") != std::string::npos)
                        g_weatherGlyph = (wchar_t)0xE753;
                    else if (lowerCond.find("fog") != std::string::npos || lowerCond.find("mist") != std::string::npos || lowerCond.find("haze") != std::string::npos)
                        g_weatherGlyph = (wchar_t)0xE728;
                    else
                        g_weatherGlyph = (wchar_t)0xE706;

                    if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE);
                }
            }
        }

        for (int i = 0; i < 10 && g_netRunning; ++i) {
            Sleep(100);
        }
        btcTimer--;
        weatherTimer--;
    }
}

// ----------------------------------------------------------------------------
//  Real machine data: battery, CPU load, memory, disk, uptime, Wi-Fi, Bluetooth. Pure
//  Win32 (no WMI, no extra libraries); CPU load is a delta of GetSystemTimes.
// ----------------------------------------------------------------------------
static void SampleSystem() {
    static int netTicks = 0;
    if (netTicks++ % 3 == 0) {
        SampleWifi();
        SampleBluetooth();
    }
    SYSTEM_POWER_STATUS ps;
    if (GetSystemPowerStatus(&ps)) {
        if (ps.BatteryLifePercent != 255 && ps.BatteryLifePercent <= 100) {
            g_battPct = (int)ps.BatteryLifePercent;
            g_hasBatt = true;
        } else if ((ps.BatteryFlag & 128) != 0) {
            g_hasBatt = false;          // BatteryFlag 128 = no system battery
        }
        g_acLine = (ps.ACLineStatus == 1);
    }

    MEMORYSTATUSEX ms;
    ZeroMemory(&ms, sizeof(ms));
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) g_ram = (float)ms.dwMemoryLoad / 100.f;

    ULARGE_INTEGER freeBytes, totalBytes, totalFree;
    if (GetDiskFreeSpaceExW(L"C:\\", &freeBytes, &totalBytes, &totalFree) && totalBytes.QuadPart > 0) {
        double used = (double)(totalBytes.QuadPart - totalFree.QuadPart) / (double)totalBytes.QuadPart;
        g_disk = Clamp((float)used, 0.f, 1.f);
    }

    FILETIME fi, fk, fu;
    if (GetSystemTimes(&fi, &fk, &fu)) {
        ULARGE_INTEGER i, k, u;
        i.LowPart = fi.dwLowDateTime; i.HighPart = fi.dwHighDateTime;
        k.LowPart = fk.dwLowDateTime; k.HighPart = fk.dwHighDateTime;
        u.LowPart = fu.dwLowDateTime; u.HighPart = fu.dwHighDateTime;
        if (g_prevKernel != 0) {
            ULONGLONG dIdle = i.QuadPart - g_prevIdle;
            ULONGLONG dTotal = (k.QuadPart - g_prevKernel) + (u.QuadPart - g_prevUser);
            if (dTotal > 0)
                g_cpu = Clamp(1.f - (float)((double)dIdle / (double)dTotal), 0.f, 1.f);
        }
        g_prevIdle = i.QuadPart;
        g_prevKernel = k.QuadPart;
        g_prevUser = u.QuadPart;
    }

    unsigned long long mins = GetTickCount64() / 60000ull;
    g_uptime = L"up " + std::to_wstring(mins / 60ull) + L"h " + std::to_wstring(mins % 60ull) + L"m";
}

// ----------------------------------------------------------------------------
//  Animation tick
// ----------------------------------------------------------------------------
static void UpdateAnimations(float dt) {
    g_sysA.To(g_panel == PANEL_SYS ? 1.f : 0.f);
    g_launchA.To(g_panel == PANEL_LAUNCH ? 1.f : 0.f);
    g_ctxA.To(g_ctxOpen ? 1.f : 0.f);
    g_lockA.To(g_locked ? 1.f : 0.f);
    if (g_hg && NowSec() - g_hlastT > 0.15) {    // touchpad gesture finished
        g_hg = false;
        g_hlock = false;
        g_hacc = 0.f;
        GoPage(g_page);                          // spring back if it never reached the threshold
    }
    if (!g_pdrag && !(g_hg && !g_hlock)) {       // while swiping the input owns the position
        g_pageA.To((float)g_page);
        if (!g_motion) g_pageA.Set(g_pageA.target);
        else g_pageA.Step(dt);
    }

    // smooth-scrollbar momentum damping for window scroll
    for (int i = 0; i < APP_COUNT; ++i) {
        Win& w = g_win[i];
        if (!w.open) continue;
        float diff = w.targetScroll - w.scroll;
        if (std::fabs(diff) > 0.05f) {
            float dampingRate = 22.0f;
            w.scroll += diff * (1.f - std::exp(-dampingRate * dt));
        } else {
            w.scroll = w.targetScroll;
        }
    }

    // smooth-scrollbar momentum damping for side screen (all apps) down scroll
    float appDiff = g_appTargetScroll - g_appScroll;
    if (std::fabs(appDiff) > 0.05f) {
        float dampingRate = 20.0f;
        g_appScroll += appDiff * (1.f - std::exp(-dampingRate * dt));
    } else {
        g_appScroll = g_appTargetScroll;
    }

    // Reduce motion: panels arrive fully open instead of sliding, and the
    // wallpaper cross-fade collapses to a cut.
    if (!g_motion) {
        g_sysA.Set(g_sysA.target);
        g_launchA.Set(g_launchA.target);
        g_ctxA.Set(g_ctxA.target);
        g_lockA.Set(g_lockA.target);
        g_wpFade.Set(g_wpFade.target);
    } else {
        g_sysA.Step(dt, 19.f);
        g_launchA.Step(dt, 17.f);
        g_ctxA.Step(dt, 26.f);
        g_lockA.Step(dt, 12.f);
        g_wpFade.Step(dt, 6.f);
    }

    if (g_wpB && g_wpFade.x > 0.999f) SafeRelease(g_wpB);

    for (size_t i = 0; i < g_toasts.size(); ++i) g_toasts[i].t += dt;
    g_toasts.erase(std::remove_if(g_toasts.begin(), g_toasts.end(),
                                  [](const Toast& t) { return t.t > 4.3f; }),
                   g_toasts.end());

    if (g_playing) {
        g_pos += dt;
        if (g_pos >= g_dur) g_pos = 0.f;
    }
}

// ----------------------------------------------------------------------------
//  Render target management
// ----------------------------------------------------------------------------
static void ReleaseBrushes() {
    SafeRelease(g_wpBitmap);
    SafeRelease(g_wpOldBitmap);
    SafeRelease(g_br);
    SafeRelease(g_wpA);
    SafeRelease(g_wpB);
    SafeRelease(g_glow);
    SafeRelease(g_scrim);
}

static void EnsureTarget() {
    if (g_rt) return;
    if (!g_d2d || !g_hwnd) return;
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU((UINT32)std::max<LONG>(1, rc.right - rc.left),
                                   (UINT32)std::max<LONG>(1, rc.bottom - rc.top));
    // Force 96 DPI: the whole UI is authored in a 1920x1200 design space that
    // Render() scales to the window itself. With the default (system DPI, e.g.
    // 144 at 150% scaling) Direct2D would scale everything a second time, which
    // blows the layout up ~1.5x and offsets mouse hit-testing.
    D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), 96.f, 96.f);
    if (FAILED(g_d2d->CreateHwndRenderTarget(rtp,
                                             D2D1::HwndRenderTargetProperties(g_hwnd, size),
                                             &g_rt)))
        return;
    g_rt->SetDpi(96.f, 96.f);
    g_rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    g_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    g_rt->CreateSolidColorBrush(C(WHITE), &g_br);
    BuildWallpaperBrushes();
    g_wpFade.Set(1.f);
}

// ----------------------------------------------------------------------------
//  Home pager: page 1 (apps), page dots, and the page strip itself
// ----------------------------------------------------------------------------
// Everything that may start a swipe: the bare desktop, app tiles and the
// widgets' buttons / note rows (a press that doesn't move is still a tap).
// Sliders are excluded - horizontal drags belong to them.
static bool PagerHit(int id) {
    return id == H_DESK || (id >= H_HAPP && id < H_HAPP + APP_N) ||
           (id >= H_NOTE && id < H_NOTE + 20) || id == H_ADDNOTE ||
           (id >= H_PREV && id <= H_REPEAT && id != H_SEEK);
}
static bool PagerAreaHit(int id) {
    return PagerHit(id) || (id >= H_HDOT && id < H_HDOT + HOME_PAGES);
}
// Smooth hyperbolic rubber-band resistance curve (C1 continuous at boundaries)
static float RubberPos(float p) {
    const float hi = (float)(HOME_PAGES - 1);
    if (p < 0.f) {
        float over = -p;
        return -(over * 0.45f) / (1.f + over * 1.25f);
    }
    if (p > hi) {
        float over = p - hi;
        return hi + (over * 0.45f) / (1.f + over * 1.25f);
    }
    return p;
}
static void HomeAppTap(int i) {
    if (i < 0 || i >= APP_N) return;
    LaunchRealApp(APPS[i]);
}

static void DrawHomeApps() {
    const int cols = 6;
    const float cellW = 240.f, cellH = 176.f;
    const float x0 = (DW - (float)cols * cellW) * 0.5f, y0 = 146.f;
    const float clipTop = 136.f, clipH = 924.f;

    int rows = (APP_N + cols - 1) / cols;
    float totalH = (float)rows * cellH + 30.f;
    g_appMaxScroll = std::max(0.f, totalH - clipH);

    // Fixed Top Header above the scrolling viewport
    Txt(L"All Applications", x0, 72.f, 340.f, 32.f, F_SEMI, 20.f, 600, COL_T100, 0, 1.f);
    Txt(std::to_wstring(APP_N) + L" installed \u2022 Scroll down for full library",
        x0, 104.f, 440.f, 22.f, F_UI, 12.f, 400, COL_T50, 0, 1.f);

    // Viewport Clipping for Smooth Down-Scroll
    g_rt->PushAxisAlignedClip(R(x0 - 24.f, clipTop, (float)cols * cellW + 48.f, clipH), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    for (int i = 0; i < APP_N; ++i) {
        float cx = x0 + (float)(i % cols) * cellW + cellW * 0.5f;
        float cy = y0 + (float)(i / cols) * cellH - g_appScroll + 70.f;
        if (cy + 96.f < clipTop || cy - 96.f > clipTop + clipH) continue;
        bool hov = (g_hover == H_HAPP + i) && !g_pdrag && !g_appVDrag;
        if (hov) Fill(cx - 96.f, cy - 64.f, 192.f, 156.f, 20.f, C(WHITE, 0.055f));
        AppIcon(APPS[i], cx, cy - 14.f, 84.f, 32.f, hov);
        Txt(APPS[i].name, cx - 96.f, cy + 42.f, 192.f, 24.f, F_UI, 12.5f, 500,
            hov ? COL_T100 : COL_T80, 1, 1.f);
        AddHit(cx - 96.f, cy - 64.f, 192.f, 156.f, H_HAPP + i);
    }

    g_rt->PopAxisAlignedClip();

    // Sleek frosted glass vertical scrollbar indicator
    if (g_appMaxScroll > 4.f) {
        float trackX = x0 + (float)cols * cellW + 24.f;
        float trackY = 146.f, trackH = 900.f;
        float thumbH = std::max(56.f, trackH * (clipH / (clipH + g_appMaxScroll)));
        float thumbY = trackY + (trackH - thumbH) * (g_appScroll / g_appMaxScroll);
        Fill(trackX, trackY, 5.f, trackH, 2.5f, C(WHITE, 0.05f));
        Fill(trackX, thumbY, 5.f, thumbH, 2.5f, C(WHITE, 0.32f));
    }
}

static void DrawPageDots() {
    if (HOME_PAGES < 2) return;
    const float cy = 1066.f, step = 22.f;
    const float total = step * (float)(HOME_PAGES - 1);
    for (int p = 0; p < HOME_PAGES; ++p) {
        float cx = DW * 0.5f - total * 0.5f + (float)p * step;
        float prox = 1.f - std::min(1.f, std::fabs((float)p - g_pageA.x));
        float a = Clamp(0.22f + 0.73f * prox + ((g_hover == H_HDOT + p) ? 0.2f : 0.f), 0.f, 1.f);
        Disc(cx, cy, 3.2f + 1.8f * prox, C(prox > 0.5f ? COL_T90 : WHITE, a));
        AddHit(cx - 12.f, cy - 12.f, 24.f, 24.f, H_HDOT + p);
    }
}

// Draws every visible page side by side, shifted by the live page position.
static void DrawHomePages() {
    g_rt->SetTransform(g_base);
    g_rt->PushAxisAlignedClip(R(0.f, 0.f, DW, DH), D2D1_ANTIALIAS_MODE_ALIASED);
    for (int p = 0; p < HOME_PAGES; ++p) {
        float dx = ((float)p - g_pageA.x) * DW;
        if (dx <= -DW || dx >= DW) continue;     // fully off-screen: skip
        g_pageDX = dx;
        g_hitX = shelllayout::Xform();
        g_hitX.tx = dx;
        g_rt->SetTransform(D2D1::Matrix3x2F::Translation(dx, 0.f) * g_base);
        if (p == 0) DrawDesktopWidgets();
        else DrawHomeApps();
    }
    g_pageDX = 0.f;
    g_hitX = shelllayout::Xform();
    g_rt->SetTransform(g_base);
    g_rt->PopAxisAlignedClip();
    DrawPageDots();
}

static void Render() {
    EnsureTarget();
    if (!g_rt || !g_br) return;
    if (g_clientW < 32 || g_clientH < 32) return;   // minimised: nothing to draw

    g_hits.clear();
    g_rt->BeginDraw();
    g_rt->SetTransform(D2D1::Matrix3x2F::Identity());
    g_rt->Clear(C(COL_LETTER));
    g_rt->SetTransform(g_base);

    DrawWallpaper();
    AddHit(0.f, 0.f, DW, DH, H_DESK);
    DrawHomePages();
    for (size_t i = 0; i < g_z.size(); ++i) DrawWindow(g_z[i]);
    DrawSnapHint();
    DrawShelf();
    DrawTray();
    if (g_sysA.x > 0.002f) DrawSystemBubble(g_sysA.x);
    if (g_launchA.x > 0.002f) DrawLauncher(g_launchA.x);
    DrawToasts();
    if (g_ctxA.x > 0.002f) DrawContextMenu(g_ctxA.x);
    if (g_lockA.x > 0.002f) DrawLockScreen(g_lockA.x);

    HRESULT hr = g_rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseBrushes();
        SafeRelease(g_rt);
    }
}

// ----------------------------------------------------------------------------
//  Coordinates
// ----------------------------------------------------------------------------
static void ToDesignXY(int px, int py, float& x, float& y) {
    x = ((float)px - g_ox) / (g_scale > 0.0001f ? g_scale : 1.f);
    y = ((float)py - g_oy) / (g_scale > 0.0001f ? g_scale : 1.f);
}
static void ToDesign(LPARAM lp, float& x, float& y) {
    ToDesignXY((int)(short)LOWORD(lp), (int)(short)HIWORD(lp), x, y);
}

static float g_mx0 = 0.f, g_my0 = 0.f;

// ----------------------------------------------------------------------------
//  Window geometry helpers
// ----------------------------------------------------------------------------
static void ApplySnap(int kind) {
    Win& w = g_win[kind];
    if (w.maxed) return;
    const float margin = 20.f, gap = 20.f;
    const float fullH = SHELF_Y - 44.f;
    if (w.y < 12.f) {
        MaximizeWindow(kind);
        return;
    }
    if (w.x < 12.f) {
        w.rx = w.x; w.ry = w.y; w.rw = w.w; w.rh = w.h;
        w.x = margin;
        w.y = margin;
        w.w = (DW - margin * 2.f - gap) * 0.5f;
        w.h = fullH;
    } else if (w.x + w.w > DW - 12.f) {
        w.rx = w.x; w.ry = w.y; w.rw = w.w; w.rh = w.h;
        w.x = margin + (DW - margin * 2.f - gap) * 0.5f + gap;
        w.y = margin;
        w.w = (DW - margin * 2.f - gap) * 0.5f;
        w.h = fullH;
    }
}

static void ScrollWindowAt(float mx, float my, float delta) {
    for (int i = (int)g_z.size() - 1; i >= 0; --i) {
        int kind = g_z[i];
        Win& w = g_win[kind];
        if (!w.open || w.min) continue;
        if (mx >= w.x && mx <= w.x + w.w && my >= w.y && my <= w.y + w.h) {
            w.targetScroll = Clamp(w.targetScroll - delta, 0.f, 800.f);
            return;
        }
    }
}

// ----------------------------------------------------------------------------
//  Click routing
// ----------------------------------------------------------------------------
static float g_slideX = 0.f, g_slideW = 1.f;

static void ApplySliderValue(int which, float mx) {
    float v = Clamp((mx - g_slideX) / (g_slideW > 1.f ? g_slideW : 1.f), 0.f, 1.f);
    if (which == 0) g_vol = v;
    else if (which == 1) g_bri = v;
    else { g_pos = v * g_dur; g_playing = true; }
}

// The same slider id exists in the bubble, in Settings and in the media card,
// so lock onto the geometry of the track that was actually pressed. Captured
// once at drag start, which also keeps dragging sane past the track ends.
static void BeginSlider(int which, int id, float mx, float my) {
    // Hit rectangles are padded by 8px on each side for easier grabbing; the
    // value must map onto the visible track, so undo that padding.
    const float pad = 8.f;
    bool found = false;
    for (int i = (int)g_hits.size() - 1; i >= 0; --i) {
        const Hit& h = g_hits[i];
        if (h.id != id) continue;
        if (mx < h.x || mx > h.x + h.w || my < h.y || my > h.y + h.h) continue;
        g_slideX = h.x + pad;
        g_slideW = std::max(1.f, h.w - pad * 2.f);
        found = true;
        break;
    }
    if (!found) {
        for (size_t i = 0; i < g_hits.size(); ++i) {
            if (g_hits[i].id != id) continue;
            g_slideX = g_hits[i].x + pad;
            g_slideW = std::max(1.f, g_hits[i].w - pad * 2.f);
            break;
        }
    }
    ApplySliderValue(which, mx);
}

static void HandleClick(int id, float mx, float my) {
    // --- system bubble -------------------------------------------------------
    if (id >= H_TOG0 && id < H_TOG0 + TOG_COUNT) {
        int t = id - H_TOG0;
        g_tog[t] = !g_tog[t];
        ShowToast(std::wstring(TOG_NAME[t]) + (g_tog[t] ? L" on" : L" off"), TOG_GLYPH[t]);
        SaveSettings();
        return;
    }

    // --- Settings: wallpaper, widget size, widget visibility, preferences ---
    if (id >= H_SET_NAV && id < H_SET_NAV + 3) {
        g_setPage = id - H_SET_NAV;
        return;
    }
    if (id == H_SET_CUSTOM_WP) {
        PromptCustomWallpaper(g_hwnd);
        return;
    }
    if (id >= H_SET_WP && id < H_SET_WP + WP_COUNT) {
        SetWallpaper(id - H_SET_WP);
        return;
    }
    if (id >= H_SET_SIZE && id < H_SET_SIZE + 3) {
        int ns = 0;
        const float* scales = shelllayout::Scales(ns);
        g_sizeIdx = id - H_SET_SIZE;
        g_widgetScale = scales[g_sizeIdx];
        SaveSettings();
        ShowToast(std::wstring(L"Widget size: ") + SET_SIZE_LABEL[g_sizeIdx], (wchar_t)0xE740);
        return;
    }
    if (id >= H_SET_WIDGET && id < H_SET_WIDGET + shelllayout::W_COUNT) {
        int i = id - H_SET_WIDGET;
        g_widgetOn[i] = !g_widgetOn[i];
        SaveSettings();
        ShowToast(std::wstring(WIDGET_NAME[i]) +
                  (g_widgetOn[i] ? L" widget shown" : L" widget hidden"), (wchar_t)0xE713);
        return;
    }
    if (id == H_SET_MISC + 0) {
        g_clock24 = !g_clock24;
        SaveSettings();
        ShowToast(g_clock24 ? L"24-hour clock" : L"12-hour clock", (wchar_t)0xE823);
        return;
    }
    if (id == H_SET_MISC + 1) {
        g_motion = !g_motion;
        SaveSettings();
        ShowToast(g_motion ? L"Animations on" : L"Animations reduced", (wchar_t)0xE769);
        return;
    }
    if (id == H_SET_RESET) {
        ResetAppearance();
        return;
    }
    // Order matters here: "clear all" and "add note" live *inside* the
    // numeric ranges of their neighbouring lists, so they are tested first and
    // the generic ranges are kept narrow.
    if (id == H_NOTIF + 99) {
        g_notifs.clear();
        ShowToast(L"Notifications cleared", (wchar_t)0xE74D);
        return;
    }
    if (id >= H_NOTIF + 50 && id < H_NOTIF + 50 + 30) {
        int idx = id - (H_NOTIF + 50);
        if (idx >= 0 && idx < (int)g_notifs.size()) g_notifs.erase(g_notifs.begin() + idx);
        return;
    }
    if (id >= H_NOTIF && id < H_NOTIF + 30) {
        int idx = id - H_NOTIF;
        if (idx >= 0 && idx < (int)g_notifs.size()) g_notifs[idx].read = true;
        return;
    }
    if (id == H_ADDNOTE) {
        if (g_notes.size() < 5) {
            g_notes.push_back({ L"New note", false });
            ShowToast(L"Note added", (wchar_t)0xE710);
        } else {
            ShowToast(L"Note list is full (5)", (wchar_t)0xE7BA);
        }
        return;
    }
    if (id >= H_NOTE && id < H_NOTE + 40) {
        int idx = id - H_NOTE;
        if (idx >= 0 && idx < (int)g_notes.size()) g_notes[idx].done = !g_notes[idx].done;
        return;
    }
    if (id >= H_APP && id < H_APP + 100) { LaunchViewIndex(id - H_APP); return; }
    if (id >= H_PAGE && id < H_PAGE + 20) {
        g_launchPage = id - H_PAGE;
        g_launchSel = -1;
        return;
    }
    if (id >= H_SHELF && id < H_SHELF + 40) {
        int i = id - H_SHELF;
        if (i >= 0 && i < PINNED_N) {
            LaunchRealApp(APPS[PINNED[i]]);
        }
        return;
    }
    if (id >= H_CTX && id < H_CTX + CTX_N) { RunContextCommand(id - H_CTX); return; }
    if (id >= H_WIN && id < H_WIN + APP_COUNT * 8) {
        int kind = (id - H_WIN) / 8, part = (id - H_WIN) % 8;
        Win& w = g_win[kind];
        switch (part) {
        case WP_CLOSE:    w.open = false; w.min = false; if (g_focus == kind) g_focus = -1;
                          ShowToast(std::wstring(L"Closed ") + WIN_TITLE[kind], (wchar_t)0xE711); break;
        case WP_MIN:      w.min = true; if (g_focus == kind) g_focus = -1; break;
        case WP_MAX:      MaximizeWindow(kind); FocusWindow(kind); break;
        case WP_TITLE:    FocusWindow(kind); break;
        case WP_RIGHT:
        case WP_BOTTOM:
        case WP_CORNER:   FocusWindow(kind); break;
        default:          FocusWindow(kind); break;
        }
        return;
    }

    if (id >= H_HAPP && id < H_HAPP + APP_N) { g_panel = PANEL_NONE; return; }   // click-away while a panel is open
    if (id >= H_HDOT && id < H_HDOT + HOME_PAGES) { GoPage(id - H_HDOT); return; }

    // --- named controls ------------------------------------------------------
    (void)mx; (void)my;
    switch (id) {
    case H_TRAY:
        g_panel = (g_panel == PANEL_SYS) ? PANEL_NONE : PANEL_SYS;
        g_ctxOpen = false;
        break;
    case H_CLOCK:
        g_panel = (g_panel == PANEL_SYS) ? PANEL_NONE : PANEL_SYS;
        break;
    case H_LAUNCHER:
        if (g_panel == PANEL_LAUNCH) g_panel = PANEL_NONE;
        else { g_panel = PANEL_LAUNCH; g_query.clear(); RebuildView(); g_launchPage = 0; g_launchSel = -1; }
        break;
    case H_LAUNCH_CLOSE: g_panel = PANEL_NONE; break;
    case H_LAUNCH_SEARCH: break;
    case H_PLAY: g_playing = !g_playing; break;
    case H_PREV: g_pos = 0.f; ShowToast(L"Previous track", (wchar_t)0xE892); break;
    case H_NEXT: g_pos = 0.f; ShowToast(L"Next track", (wchar_t)0xE893); break;
    case H_SHUFFLE: ShowToast(L"Shuffle on", (wchar_t)0xE8B1); break;
    case H_REPEAT: ShowToast(L"Repeat all", (wchar_t)0xE8EE); break;
    case H_LOCK: g_locked = true; g_panel = PANEL_NONE; break;
    case H_UNLOCK: g_locked = false; ShowToast(L"Welcome back, Buffy", (wchar_t)0xE72E); break;
    case H_SETTINGS: OpenApp(APP_SETTINGS); break;
    case H_POWER: PostMessageW(g_hwnd, WM_CLOSE, 0, 0); break;
    case H_DESK:
        if (g_panel != PANEL_NONE) g_panel = PANEL_NONE;
        g_focus = -1;         // clicking bare desktop blurs the active window
        break;
    default: break;
    }
}

// ----------------------------------------------------------------------------
//  Keyboard
// ----------------------------------------------------------------------------
static void LauncherMove(int dx, int dy) {
    int n = (int)g_view.size();
    if (n <= 0) return;
    int cur = (g_launchSel < 0) ? g_launchPage * LPER : g_launchSel;
    if (cur >= n) cur = n - 1;
    int col = cur % LCOLS;
    int row = (cur / LCOLS) % LROWS;
    if (dx) col = (col + dx + LCOLS) % LCOLS;
    if (dy) row = (row + dy + LROWS) % LROWS;
    int idx = g_launchPage * LPER + row * LCOLS + col;
    if (idx >= n) idx = n - 1;
    if (idx < 0) idx = 0;
    g_launchSel = idx;
}

static void OnKeyDown(WPARAM vk) {
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    if (vk == VK_F11) { ToggleFullscreen(g_hwnd); return; }
    if (ctrl && (vk == 'Q')) { PostMessageW(g_hwnd, WM_CLOSE, 0, 0); return; }

    // Ctrl + / Ctrl - / Ctrl 0: interface text size (like a browser's zoom).
    if (ctrl && (vk == VK_OEM_PLUS || vk == VK_ADD || vk == VK_OEM_MINUS || vk == VK_SUBTRACT || vk == '0' || vk == VK_NUMPAD0)) {
        if (vk == '0' || vk == VK_NUMPAD0) g_textPct = TEXT_PCT_DEFAULT;
        else if (vk == VK_OEM_PLUS || vk == VK_ADD) g_textPct = std::min(TEXT_PCT_MAX, g_textPct + 5);
        else g_textPct = std::max(TEXT_PCT_MIN, g_textPct - 5);
        ShowToast(L"Text size " + std::to_wstring(g_textPct) + L"%", (wchar_t)0xE8E9);
        SaveSettings();
        return;
    }

    if (g_locked) {
        if (vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_SPACE) {
            g_locked = false;
            ShowToast(L"Welcome back, Buffy", (wchar_t)0xE72E);
        }
        return;
    }

    if (vk == VK_ESCAPE) {
        if (g_ctxOpen) { g_ctxOpen = false; return; }
        if (g_panel != PANEL_NONE) { g_panel = PANEL_NONE; return; }
        if (g_focus >= 0 && g_win[g_focus].maxed) { MaximizeWindow(g_focus); return; }
        return;
    }
    if (vk == VK_LWIN || vk == VK_RWIN || (ctrl && vk == VK_SPACE)) {
        if (g_panel == PANEL_LAUNCH) g_panel = PANEL_NONE;
        else {
            g_panel = PANEL_LAUNCH;
            g_query.clear();
            RebuildView();
            g_launchPage = 0;
            g_launchSel = -1;
        }
        return;
    }

    if ((vk == VK_LEFT || vk == VK_RIGHT) && g_panel == PANEL_NONE && g_focus < 0 && !g_ctxOpen) {
        GoPage(g_page + (vk == VK_RIGHT ? 1 : -1));
        return;
    }

    if (g_panel == PANEL_LAUNCH) {
        int n = (int)g_view.size();
        if (vk == VK_LEFT)  { LauncherMove(-1, 0); return; }
        if (vk == VK_RIGHT) { LauncherMove(1, 0); return; }
        if (vk == VK_UP)    { LauncherMove(0, -1); return; }
        if (vk == VK_DOWN)  { LauncherMove(0, 1); return; }
        if (vk == VK_RETURN) {
            if (g_launchSel >= 0 && g_launchSel < n) LaunchViewIndex(g_launchSel);
            else if (n > 0) LaunchViewIndex(0);
            return;
        }
        if (vk == VK_BACK) {
            if (!g_query.empty()) {
                g_query.pop_back();
                RebuildView();
                g_launchPage = 0;
                g_launchSel = -1;
            }
            return;
        }
        if (vk == VK_NEXT) { if (g_launchPage + 1 < PageCount()) g_launchPage++; return; }
        if (vk == VK_PRIOR) { if (g_launchPage > 0) g_launchPage--; return; }
    }

    if (g_focus >= 0 && (g_win[g_focus].open && !g_win[g_focus].min)) {
        Win& w = g_win[g_focus];
        if (g_focus == APP_TERM) {
            if (vk == VK_RETURN) {
                std::wstring cmd = w.input;
                w.input.clear();
                TermRun(cmd);
                return;
            }
            if (vk == VK_BACK) { if (!w.input.empty()) w.input.pop_back(); return; }
        } else if (g_focus == APP_EDITOR) {
            if (vk == VK_RETURN) {
                w.lines.push_back(w.input);
                w.input.clear();
                return;
            }
            if (vk == VK_BACK) { if (!w.input.empty()) w.input.pop_back(); return; }
        }
    }
}

static void OnChar(wchar_t ch) {
    if (g_locked) return;
    if (GetKeyState(VK_CONTROL) & 0x8000) return;   // keep Ctrl+Space out of the text
    if (ch < 32 || ch == 127) return;
    if (g_panel == PANEL_LAUNCH) {
        if (g_query.size() < 48) {
            g_query.push_back(ch);
            RebuildView();
            g_launchPage = 0;
            g_launchSel = -1;
        }
        return;
    }
    if (g_focus >= 0) {
        Win& w = g_win[g_focus];
        if (!w.open || w.min) return;
        if ((g_focus == APP_TERM || g_focus == APP_EDITOR) && w.input.size() < 400)
            w.input.push_back(ch);
    }
}

// ----------------------------------------------------------------------------
//  Fullscreen
// ----------------------------------------------------------------------------
static void ToggleFullscreen(HWND h) {
    if (!g_full) {
        g_savedStyle = (LONG)GetWindowLongPtrW(h, GWL_STYLE);
        MONITORINFO mi;
        ZeroMemory(&mi, sizeof(mi));
        mi.cbSize = sizeof(mi);
        if (GetWindowPlacement(h, &g_wpPlace) &&
            GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLongPtrW(h, GWL_STYLE, (LONG_PTR)(g_savedStyle & ~(WS_CAPTION | WS_THICKFRAME)));
            SetWindowPos(h, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            g_full = true;
        }
    } else {
        SetWindowLongPtrW(h, GWL_STYLE, (LONG_PTR)g_savedStyle);
        SetWindowPlacement(h, &g_wpPlace);
        SetWindowPos(h, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        g_full = false;
    }
}

static void UpdateLayout(HWND h) {
    RECT rc;
    GetClientRect(h, &rc);
    g_clientW = std::max<LONG>(1, rc.right - rc.left);
    g_clientH = std::max<LONG>(1, rc.bottom - rc.top);
    float sx = (float)g_clientW / DW;
    float sy = (float)g_clientH / DH;
    g_scale = std::min(sx, sy);
    g_ox = ((float)g_clientW - DW * g_scale) * 0.5f;
    g_oy = ((float)g_clientH - DH * g_scale) * 0.5f;
    g_base = D2D1::Matrix3x2F::Scale(g_scale, g_scale) * D2D1::Matrix3x2F::Translation(g_ox, g_oy);
}

static bool IsClickableId(int id) {
    if (id == H_NONE || id == H_DESK || id == H_SHELF_BG) return false;
    return true;
}

// Settings carries a two-column page (wallpaper grid + widget list), so it
// refuses to shrink past the point where that layout still reads.
static float MinWinW(int kind) { return kind == APP_SETTINGS ? 760.f : 420.f; }
static float MinWinH(int kind) { return kind == APP_SETTINGS ? 520.f : 300.f; }

// ----------------------------------------------------------------------------
//  Window procedure
// ----------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMaxTrackSize.x = 10000;
        mmi->ptMaxTrackSize.y = 10000;
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        Render();
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        UpdateLayout(h);
        if (g_rt && wp != SIZE_MINIMIZED) {
            g_rt->Resize(D2D1::SizeU((UINT32)std::max<LONG>(1, LOWORD(lp)),
                                     (UINT32)std::max<LONG>(1, HIWORD(lp))));
        }
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_TIMER: {
        double t = NowSec();
        float dt = (float)(t - g_lastT);
        g_lastT = t;
        g_clockT = t;
        g_dt = Clamp(dt, 0.f, 0.08f);
        static int ticks = 0;
        if (ticks++ % 40 == 0) SampleSystem();    // ~every 600 ms
        UpdateAnimations(g_dt);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        float mx, my;
        ToDesign(lp, mx, my);
        if (g_dragSlider >= 0) {
            ApplySliderValue(g_dragSlider, mx);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (g_dragWin >= 0) {
            Win& w = g_win[g_dragWin];
            float dx = mx - g_mx0, dy = my - g_my0;
            if (g_dragPart == WP_TITLE) {
                w.x += dx;
                w.y += dy;
                if (w.y < -6.f) w.y = -6.f;
                if (w.y > DH - 90.f) w.y = DH - 90.f;
                if (w.x + w.w < 160.f) w.x = 160.f - w.w;
                if (w.x > DW - 160.f) w.x = DW - 160.f;
            } else if (g_dragPart == WP_RIGHT) {
                w.w = std::max(MinWinW(g_dragWin), g_sw + dx);
            } else if (g_dragPart == WP_BOTTOM) {
                w.h = std::max(MinWinH(g_dragWin), g_sh + dy);
            } else if (g_dragPart == WP_CORNER) {
                w.w = std::max(MinWinW(g_dragWin), g_sw + dx);
                w.h = std::max(MinWinH(g_dragWin), g_sh + dy);
            }
            g_mx0 = mx;
            g_my0 = my;
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (g_pdown) {
            double now = NowSec();
            g_tracker.Add(mx, now);
            float dx = mx - g_pdx0, dy = my - g_pdy0;
            if (!g_pdrag && !g_appVDrag) {
                if (g_page == 1 && std::fabs(dy) > 10.f && std::fabs(dy) > std::fabs(dx) * 1.15f) {
                    g_appVDrag = true;
                    g_appDragStartY = my;
                    g_appDragStartScroll = g_appTargetScroll;
                    g_hover = H_NONE;
                } else if (std::fabs(dx) > 11.f && std::fabs(dx) > std::fabs(dy)) {
                    g_pdrag = true;                  // horizontal intent: this is a swipe, not a tap
                    g_hover = H_NONE;
                }
            }
            if (g_appVDrag) {
                float newScroll = Clamp(g_appDragStartScroll - (my - g_appDragStartY), 0.f, g_appMaxScroll);
                g_appTargetScroll = newScroll;
                g_appScroll = newScroll;
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            if (g_pdrag) {
                // Rock-solid 1:1 finger tracking during active drag (no lookahead jitter)
                float curDx = mx - g_pdx0;
                float targetPos = RubberPos(g_pstartPos - curDx / DW);
                g_pageA.Follow(targetPos, g_dt);
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
        }
        int id = HitTest(mx, my);
        if (id != g_hover) {
            g_hover = id;
            InvalidateRect(h, nullptr, FALSE);
        }
        g_ctxHover = (id >= H_CTX && id < H_CTX + CTX_N) ? id - H_CTX : -1;
        g_fileHover = (id >= H_FILE && id < H_FILE + FILE_N) ? id - H_FILE : -1;
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            LPCWSTR cur = IDC_ARROW;
            int id = g_hover;
            if (id >= H_WIN && id < H_WIN + APP_COUNT * 8) {
                int part = (id - H_WIN) % 8;
                if (part == WP_RIGHT) cur = IDC_SIZEWE;
                else if (part == WP_BOTTOM) cur = IDC_SIZENS;
                else if (part == WP_CORNER) cur = IDC_SIZENWSE;
            } else if (IsClickableId(id)) {
                cur = IDC_HAND;
            }
            if (g_pdrag) cur = IDC_SIZEWE;
            SetCursor(LoadCursorW(nullptr, cur));
            return TRUE;
        }
        break;
    case WM_LBUTTONDOWN: {
        float mx, my;
        ToDesign(lp, mx, my);
        int id = HitTest(mx, my);

        if (g_locked) {
            g_locked = false;
            ShowToast(L"Welcome back, Buffy", (wchar_t)0xE72E);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (g_ctxOpen && !(id >= H_CTX && id < H_CTX + CTX_N)) {
            g_ctxOpen = false;
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }

        if (id == H_VOL || id == H_BRI || id == H_SEEK) {
            g_dragSlider = (id == H_VOL) ? 0 : (id == H_BRI ? 1 : 2);
            SetCapture(h);
            BeginSlider(g_dragSlider, id, mx, my);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (id >= H_WIN && id < H_WIN + APP_COUNT * 8) {
            int kind = (id - H_WIN) / 8, part = (id - H_WIN) % 8;
            FocusWindow(kind);
            if (!g_win[kind].maxed &&
                (part == WP_TITLE || part == WP_RIGHT || part == WP_BOTTOM || part == WP_CORNER)) {
                Win& w = g_win[kind];
                g_dragWin = kind;
                g_dragPart = part;
                g_mx0 = mx;
                g_my0 = my;
                g_sw = w.w;
                g_sh = w.h;
                SetCapture(h);
            }
            HandleClick(id, mx, my);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }

        if (g_panel == PANEL_NONE && PagerHit(id)) {
            g_pdown = true;
            g_pdrag = false;
            g_pdid = id;
            g_pdx0 = mx;
            g_pdy0 = my;
            g_pstartPos = g_pageA.x;
            g_tracker.Reset();
            g_tracker.Add(mx, NowSec());
            SetCapture(h);
            if (id == H_DESK) HandleClick(id, mx, my);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }

        HandleClick(id, mx, my);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        float mx, my;
        ToDesign(lp, mx, my);
        int id = HitTest(mx, my);
        if (id >= H_WIN && id < H_WIN + APP_COUNT * 8) {
            int kind = (id - H_WIN) / 8, part = (id - H_WIN) % 8;
            if (part == WP_TITLE) {
                MaximizeWindow(kind);
                FocusWindow(kind);
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
        }
        return 0;
    }
    case WM_CAPTURECHANGED:
        // Focus loss or a stolen capture must never leave a drag latched.
        g_dragWin = -1;
        g_dragPart = 0;
        g_dragSlider = -1;
        g_appVDrag = false;
        if (g_pdown) {                           // capture stolen mid-swipe: just settle
            bool was = g_pdrag;
            g_pdown = false;
            g_pdrag = false;
            if (was) GoPage((int)std::floor(g_pageA.x + 0.5f));
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_pdown) {
            float mx, my;
            ToDesign(lp, mx, my);
            const bool wasDrag = g_pdrag;
            const bool wasVDrag = g_appVDrag;
            const int did = g_pdid;
            double now = NowSec();
            g_pdown = false;
            g_pdrag = false;
            g_appVDrag = false;
            ReleaseCapture();
            if (wasDrag) {
                float v = g_tracker.GetVelocity(now);
                float dx = mx - g_pdx0;
                const int base = (int)std::floor(g_pstartPos + 0.5f);
                float projX = dx + v * 0.20f;

                int target = base;
                // High-confidence flick detection with directional confirmation
                if (std::fabs(v) > 340.f && ((v < 0.f && dx < 0.f) || (v > 0.f && dx > 0.f))) {
                    target = base + (v < 0.f ? 1 : -1);
                } else if (std::fabs(projX) > DW * 0.26f) {
                    target = base + (projX < 0.f ? 1 : -1);
                }
                target = std::max(0, std::min(HOME_PAGES - 1, target));

                // Seamless glide: transfer release velocity directly into the 2nd-order spring
                float releaseVel = Clamp(-v / DW, -7.0f, 7.0f);
                GoPage(target, releaseVel);
            } else if (!wasVDrag) {
                if (HitTest(mx, my) == did) {
                    if (did >= H_HAPP && did < H_HAPP + APP_N) HomeAppTap(did - H_HAPP);
                    else if (did != H_DESK) HandleClick(did, mx, my);
                }
            }
        }
        if (g_dragSlider >= 0) { g_dragSlider = -1; ReleaseCapture(); SaveSettings(); }
        if (g_dragWin >= 0) {
            int kind = g_dragWin;
            bool wasMove = (g_dragPart == WP_TITLE);
            g_dragWin = -1;
            ReleaseCapture();
            if (wasMove) ApplySnap(kind);
            g_dragPart = 0;
        }
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_RBUTTONDOWN: {
        float mx, my;
        ToDesign(lp, mx, my);
        g_ctxOpen = true;
        g_ctxX = mx;
        g_ctxY = my;
        g_panel = PANEL_NONE;
        g_ctxHover = -1;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(h, &p);
        float mx, my;
        ToDesignXY(p.x, p.y, mx, my);
        if (!g_locked && g_panel == PANEL_NONE && PagerAreaHit(HitTest(mx, my))) {
            static double lastWheel = 0.0;           // one page per wheel flick
            double now = NowSec();
            int d = GET_WHEEL_DELTA_WPARAM(wp);
            if (g_page == 1) {
                // Side Screen: ALL APPS down scroll with smooth-scrollbar momentum damping
                float scrollDelta = (float)d / 120.f * 130.f;
                if (g_appTargetScroll <= 8.f && d > 0 && (now - lastWheel > 0.22)) {
                    // At the top and scrolling up: flick back to Page 0
                    GoPage(0, -3.2f);
                    lastWheel = now;
                } else {
                    g_appTargetScroll = Clamp(g_appTargetScroll - scrollDelta, 0.f, g_appMaxScroll);
                }
            } else {
                if (d != 0 && now - lastWheel > 0.22) {
                    float v = (d < 0 ? 3.2f : -3.2f);
                    GoPage(g_page + (d < 0 ? 1 : -1), v);
                    lastWheel = now;
                }
            }
        } else {
            ScrollWindowAt(mx, my, (float)GET_WHEEL_DELTA_WPARAM(wp) / 120.f * 60.f);
        }
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEHWHEEL: {                           // two-finger sideways swipe on a touchpad
        if (g_locked || g_panel != PANEL_NONE || g_ctxOpen || g_pdown) return 0;
        const float T = 220.f;
        double now = NowSec();
        if (!g_hg) { g_hg = true; g_hlock = false; g_hacc = 0.f; g_hbase = g_page; }
        g_hlastT = now;
        if (!g_hlock) {
            g_hacc += (float)GET_WHEEL_DELTA_WPARAM(wp) * HSWIPE_DIR;
            if (std::fabs(g_hacc) >= T) {
                int dir = g_hacc > 0.f ? 1 : -1;
                GoPage(g_hbase + dir, (float)dir * 2.8f);
                g_hlock = true;
            } else {
                float peek = RubberPos((float)g_hbase + (g_hacc / T) * 0.32f);
                g_pageA.Follow(peek, g_dt);
            }
        }
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        OnKeyDown(wp);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_CHAR:
        OnChar((wchar_t)wp);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_CLOSE:
        SaveSettings();
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        SafeRelease(g_wicFactory);
        CoUninitialize();
        timeEndPeriod(1);
        g_netRunning = false;
        if (g_netThread.joinable()) g_netThread.join();
        KillTimer(h, 1);
        ReleaseBrushes();
        SafeRelease(g_rt);
        for (auto& kv : g_fmt) kv.second->Release();
        g_fmt.clear();
        SafeRelease(g_dw);
        SafeRelease(g_d2d);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ----------------------------------------------------------------------------
//  Seed content
// ----------------------------------------------------------------------------
static void SeedContent() {
    g_notes.clear();
    g_notes.push_back({ L"Ship the advanced shell build", true });
    g_notes.push_back({ L"Review the palette spec", false });
    g_notes.push_back({ L"Test on Windows 11 at 150% DPI", false });

    g_notifs.clear();
    g_notifs.push_back({ L"Terminal", L"Build succeeded", L"0 errors  ·  0 warnings", L"2m",
                         (wchar_t)0xE756, false });
    g_notifs.push_back({ L"Files", L"3 new files in Projects", L"obsidian_shell.cpp, palette.png", L"18m",
                         (wchar_t)0xE8B7, false });
    g_notifs.push_back({ L"Calendar", L"Design review", L"Today at 16:00  ·  30 min", L"1h",
                         (wchar_t)0xE787, true });

    Win& term = g_win[APP_TERM];
    term.lines.clear();
    term.lines.push_back(L"Obsidian Shell " + std::wstring(VERSION) + L"  ·  Direct2D renderer");
    term.lines.push_back(L"Type 'help' for commands, or drag a window to a screen edge.");
    term.lines.push_back(L"");

    Win& ed = g_win[APP_EDITOR];
    ed.lines.clear();
    ed.lines.push_back(L"// Untitled — type here");
    ed.lines.push_back(L"static const unsigned COL_SURF = 0x121316;");
    ed.lines.push_back(L"// Faint edges, deep surfaces, one green accent.");

    g_view.clear();
    for (int i = 0; i < APP_N; ++i) g_view.push_back(i);
}

// ----------------------------------------------------------------------------
//  Hit-id arithmetic, enforced at compile time. Every interactive region is
//  packed into its own numeric band; these asserts make an accidental overlap
//  (which silently steals clicks) a build error instead of a mystery.
// ----------------------------------------------------------------------------
static_assert(H_TOG0 + TOG_COUNT <= H_VOL, "toggle ids overlap the sliders");
static_assert(H_VOL + 10 <= H_NOTIF, "slider ids overlap notifications");
static_assert(H_NOTIF + 100 <= H_NOTE, "notification ids overlap notes");
static_assert(H_ADDNOTE < H_PREV, "note ids overlap media controls");
static_assert(H_REPEAT < H_LAUNCHER, "media ids overlap the launcher");
static_assert(H_LAUNCH_CLOSE < H_APP, "launcher chrome overlaps the app grid");
static_assert(H_APP + 100 <= H_PAGE, "app grid overlaps the page dots");
static_assert(H_PAGE + 20 <= H_SHELF, "page dots overlap the shelf");
static_assert(H_SHELF + 40 <= H_WIN, "shelf ids overlap window ids");
static_assert(H_WIN + APP_COUNT * 8 <= H_SET_NAV, "window ids overlap the settings tabs");
static_assert(H_SET_NAV + 3 <= H_SET_WP, "settings tabs overlap the wallpaper swatches");
static_assert(H_SET_WP + WP_COUNT <= H_SET_SIZE, "wallpaper swatches overlap the size buttons");
static_assert(H_SET_SIZE + 3 <= H_SET_WIDGET, "size buttons overlap the widget list");
static_assert(H_SET_WIDGET + shelllayout::W_COUNT <= H_SET_MISC, "widget list overlaps preferences");
static_assert(H_SET_MISC + 2 <= H_SET_RESET, "preferences overlap the reset button");
static_assert(H_SET_RESET < H_CTX, "settings ids overlap the context menu");
static_assert(H_CTX + 16 <= H_UNLOCK, "context menu overlaps the unlock region");
static_assert(H_UNLOCK < H_FILE, "unlock region overlaps file rows");
static_assert(H_FILE + FILE_N + 4 <= H_DESK, "file rows overlap the desktop");
static_assert(H_DESK < H_HAPP && H_HAPP + APP_N <= H_HDOT, "home app tiles overlap other ids");
static_assert(H_HDOT + HOME_PAGES <= 1300, "home page dots overlap other ids");

// ----------------------------------------------------------------------------
//  Entry point
// ----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR, int nShow) {
    CoInitialize(NULL);
    // Per-monitor DPI v2 when available (Win10 1703+), plain aware otherwise.
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        typedef BOOL (WINAPI *SetCtxFn)(void*);
        SetCtxFn setCtx = (SetCtxFn)(void*)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setCtx) setCtx((void*)-4);      // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        else SetProcessDPIAware();
    }

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory),
                                 nullptr, (void**)&g_d2d)))
        return 1;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   (IUnknown**)&g_dw)))
        return 1;

    g_clockT = NowSec();
    g_lastT = g_clockT;
    g_wpPlace.length = sizeof(WINDOWPLACEMENT);
    LoadSettings();
    SeedContent();

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"ObsidianShellWindow";
    if (!RegisterClassExW(&wc)) return 1;

    RECT work = { 0, 0, 1600, 1000 };
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int availW = work.right - work.left, availH = work.bottom - work.top;
    int winW = (int)(availW * 0.92f), winH = (int)(availH * 0.92f);
    if (winW < 900) winW = std::min(availW, 900);
    if (winH < 620) winH = std::min(availH, 620);

    RECT rc = { 0, 0, winW, winH };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int fullW = rc.right - rc.left, fullH = rc.bottom - rc.top;
    int posX = work.left + std::max<LONG>(0, (availW - fullW) / 2);
    int posY = work.top + std::max<LONG>(0, (availH - fullH) / 2);

    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"ChromeOS \u2022 Neutral Obsidian Monolith",
                             WS_OVERLAPPEDWINDOW, posX, posY, fullW, fullH,
                             nullptr, nullptr, hi, nullptr);
    if (!g_hwnd) return 1;

    UpdateLayout(g_hwnd);
    ShowWindow(g_hwnd, (nShow == 0 || nShow == SW_HIDE) ? SW_SHOWNORMAL : nShow);
    UpdateWindow(g_hwnd);
    SampleWifi();
    SampleBluetooth();
    g_netThread = std::thread(NetWorkerThread);
    timeBeginPeriod(1);
    SetTimer(g_hwnd, 1, 10, nullptr);
    SetFocus(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
