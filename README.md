# 🪟 WinWidgets — Native Windows Desktop Widgets Suite

A lightweight, modern Windows desktop widgets application written in **C++20** and the native **Win32 API / Direct2D**. Inspired by classic Windows desktop gadgets and Rainmeter, designed with modern acrylic dark glass styling, smooth animations, and high performance.

---

## ✨ Available Widgets

1. **🕒 Clock & Dynamic Greeting**
   - Live time display with pulsing seconds.
   - Dynamic greetings: *Good Morning*, *Good Afternoon*, *Good Evening*, *Good Night*.
   - Full date & weekday format.
   - Click anywhere to toggle between **12-hour (AM/PM)** and **24-hour** format.

2. **💻 System Performance & Battery Monitor**
   - **Real-time CPU Usage %** with dynamic colored meter (Cyan → Amber → Rose).
   - **RAM / Memory Consumption**: Used GB / Total GB with progress bar.
   - **Battery & Power Gauge**: Percentage bar, charging status (`⚡`), and AC desktop indicator.
   - **System Uptime**: Precise counter (`X days, Y hrs, Z mins`).

3. **⛅ Weather Forecast**
   - Live weather fetched asynchronously via **Open-Meteo REST API** (no API key required).
   - Crisp custom Direct2D vector icons (Sun, Moon, Clouds, Rain, Thunderstorm, Snow).
   - Temperature display (click to toggle **°C** and **°F**).
   - City presets (click city to cycle between New York, London, Tokyo, Paris, San Francisco, Mumbai, Sydney, Berlin).
   - Wind speed and condition details.
   - Full offline fallback mode if disconnected.

4. **📅 Interactive Calendar**
   - Full month view with previous (`◀`) and next (`▶`) month navigation.
   - `Today` shortcut button to quickly return to current date.
   - Today highlighted with glowing cyan accent ring.
   - Interactive day selection.

5. **✅ Tasks Checklist & Quick Notes**
   - Interactive checklist with checkmark toggle.
   - Strikethrough completed task styling.
   - Inline Direct2D text input box at the bottom (`Add a task...` + Enter or `+` button).
   - Delete button (`✕`) for removing tasks.
   - Persistent storage to `tasks.json` across app reboots.

6. **📰 World News & Headlines Reader**
   - Live RSS news feed fetched in a non-blocking background thread via **WinHTTP**.
   - Carousel navigation (`◀ Previous`, `Next ▶`, and `↻ Refresh`).
   - Auto-rotates headlines every 18 seconds.
   - Click any headline card to open the story directly in your default browser.

7. **🧭 WinWidgets Control Dock / Hub**
   - Sleek horizontal floating control bar.
   - One-click toggle pills for each widget with active glow indicators.
   - Quick launcher to show/hide any individual widget.

---

## 🛠 Features & Desktop Integration

- **Smooth Dragging**: Click and drag any widget across your desktop(s) to place it anywhere.
- **Pin to Desktop**: Right-click any widget and select **Pin to Desktop (Bottom)** to anchor it directly to your wallpaper behind all other windows.
- **Always on Top**: Option to float widgets above normal windows.
- **Position Locking**: Prevent accidental moving by toggling **Lock Position**.
- **Adjustable Opacity**: Set opacity to 100%, 90%, 80%, or 70% for true translucent acrylic glass appearance.
- **Automatic Position Persistence**: Widget positions, opacities, and visibility are automatically remembered in `widgets_config.json`.
- **System Tray Icon**:
  - Left-click tray icon: Bring all widgets to front.
  - Right-click tray icon: Full menu to toggle individual widgets, pin all, lock all, reset layout, or exit.
- **Ultra Lightweight**: Consumes less than 15 MB of RAM and negligible CPU cycles.

---

## 🚀 Building & Running

### Requirements
- Windows 10 or 11 (64-bit)
- MinGW-w64 (`g++` supporting C++20) and CMake / Ninja, or MSVC

### One-Click Build
Run the provided build script:
```bat
build.bat
```

### Launch
Run:
```bat
run.bat
```
or double-click `WinWidgets.exe`.

---

## 📁 Architecture

```
sharp-curie/
├── CMakeLists.txt            # CMake configuration
├── build.bat                 # One-click compiler script
├── run.bat                   # Launch script
├── tasks.json                # User saved checklist tasks
├── widgets_config.json       # Saved widget desktop coordinates & options
└── src/
    ├── main.cpp              # WinMain entry point & DPI setup
    ├── app.h / .cpp          # WidgetManager, tray icon, hidden message window
    ├── graphics/
    │   ├── d2d_context.h     # Direct2D & DirectWrite engine
    │   └── d2d_context.cpp   # Vector rendering (weather icons, battery, meters)
    ├── utils/
    │   ├── http_client.h     # WinHTTP background asynchronous client
    │   ├── http_client.cpp
    │   ├── json_mini.h       # Lightweight C++ JSON parser & serializer
    │   ├── xml_mini.h        # RSS/Atom XML feed parser
    │   ├── system_stats.h    # Win32 CPU, RAM, Battery & Uptime collector
    │   └── system_stats.cpp
    ├── widgets/
    │   ├── widget_base.h     # Base class for layered floating Win32 windows
    │   ├── widget_base.cpp
    │   ├── widget_clock.h / .cpp       # Clock & Greeting widget
    │   ├── widget_system.h / .cpp      # System Monitor widget
    │   ├── widget_weather.h / .cpp     # Weather Forecast widget
    │   ├── widget_calendar.h / .cpp    # Calendar widget
    │   ├── widget_tasks.h / .cpp       # Tasks & Checklist widget
    │   ├── widget_news.h / .cpp        # News Reader widget
    │   └── widget_hub.h / .cpp         # Control Hub dock
    └── resources/
        ├── manifest.xml      # High-DPI & Common Controls v6 manifest
        └── resource.rc       # Embedded Windows resource
```
