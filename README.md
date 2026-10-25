# x-004: Obsidian Monolith Desktop Shell

A high-performance, hardware-accelerated desktop shell for Windows written in native C++ using **Direct2D**, **DirectWrite**, and the **Win32 API**. Engineered with a dark metallic "Neutral Obsidian" palette, glassmorphism surfaces, and live hardware & network integrations.

---

## ✨ Features

- **Obsidian Monolith Aesthetic**:
  - Precision dark metallic gradient palette (`#090A0D` → `#111317` → `#181B22` → `#0D0E12`).
  - Translucent glassmorphism cards with 5–15% white borders and subtle drop-shadows.
  - Active emerald accent badges (`#34D399` / `#10B981`) for battery and market indicators.
  - Crisp typography rendered via hardware-accelerated **DirectWrite**.

- **Live Hardware & OS Telemetry**:
  - **Native Wi-Fi**: Directly calls Windows WLAN API (`wlanapi.h`) to detect the active SSID and actual Wi-Fi signal quality percentage.
  - **Native Bluetooth**: Queries Windows Bluetooth API (`bluetoothapis.h`) for radio state and paired/connected devices.
  - **System Metrics**: Real-time CPU load sampling via `GetSystemTimes`, RAM utilization via `GlobalMemoryStatusEx`, root disk free space, and system uptime.
  - **Battery & Power**: Real-time battery charge level and AC charging status via `GetSystemPowerStatus`.

- **Live Network Services (Pure Native WinHTTP)**:
  - **Bitcoin Telemetry**: Queries Coinbase REST API (`api.coinbase.com`) via asynchronous background threads using `winhttp.dll`. Renders dynamic 24-point hourly sparkline graphs with automatic green/red 24-hour gain/loss color-coding.
  - **Live Weather**: Fetches real-time temperature, condition descriptions, and daily highs/lows from `wttr.in` with custom procedural weather glyphs (rain, storm, cloud, sun).

- **Desktop Shell & Productivity**:
  - **Direct Real Windows App Launching**: App icons across the searchable Launcher, Home Screen grid, and Pinned Shelf directly execute real Windows programs and URI protocols (`explorer.exe`, Windows Terminal `wt.exe`/PowerShell, Windows Settings `ms-settings:`, Notepad, Google Chrome, Spotify, Windows Photos `ms-photos:`, Camera `microsoft.windows.camera:`, Calculator `calc.exe`, OneNote, Microsoft Store, Microsoft To Do, and Google Workspace web services) with graceful fallbacks.
  - **Swipeable Home Pager**: 2-page desktop experience. Page 0 hosts interactive desktop widgets; Page 1 hosts a direct 24-app grid. Supports mouse drag with flick momentum and rubber-band overscroll, touchpad 2-finger horizontal swipes (`WM_MOUSEHWHEEL`), arrow keys, and animated indicator dots.
  - **Dynamic Text Zoom**: Scale small interface typography on the fly with `Ctrl +`, `Ctrl -`, and `Ctrl 0` (range 90%–150%, default 115%), persisted automatically to `shell.ini`.
  - **App Launcher**: Fullscreen searchable launcher overlay containing 24 curated system apps and utilities with real-time text filtering and keyboard selection (`Enter`).
  - **Quick Settings Flyout**: ChromeOS-inspired system tray shelf with Wi-Fi, Bluetooth, Do Not Disturb, Night Light, and Volume controls.
  - **Adaptive Bounds**: DPI-aware layout engine (`shell_layout.h`) ensuring consistent proportions across 16:10 and 16:9 displays without double-scaling bugs.
  - **Fullscreen Toggle**: Press `F11` to toggle between desktop window mode and borderless immersion mode.

---

## 🛠️ Building from Source

### Prerequisites

- Windows 10 / 11 (64-bit)
- **MinGW-w64** (GCC 10+ recommended) with `g++` available in your system `PATH`.
- Native Windows SDK headers (included by default in MinGW-w64 / MSYS2 / WinLibs).

### Compilation

You can build the standalone executable with the provided batch script:

```bat
build_mingw.bat
```

Or run `g++` directly:

```bash
g++ -std=c++17 -O2 -s -mwindows -static UI_shell.cpp -o UI_shell.exe \
    -ld2d1 -ldwrite -luser32 -lgdi32 -lwinhttp -lwlanapi -lbthprops -lshell32
```

### Running

Launch the compiled executable:

```bat
UI_shell.exe
```

---

## ⌨️ Controls & Shortcuts

| Key / Action | Description |
| :--- | :--- |
| `F11` | Toggle borderless fullscreen mode |
| `Esc` | Dismiss App Launcher or Quick Settings flyout |
| `←` / `→` | Switch between Home Screen pages (Widgets / Apps) |
| `Swipe / Drag` | Smooth horizontal drag or 2-finger touchpad swipe between pages |
| `Ctrl +` / `Ctrl -` | Increase / decrease interface text size (90% – 150%) |
| `Ctrl 0` | Reset interface text size to 115% |
| `Click Shelf Clock` | Toggle Quick Settings flyout |
| `Click Launcher Pill` | Open searchable Application Launcher |
| `Type in Launcher` | Real-time filter apps |

---

## 📄 License

MIT License. Designed and crafted with precision for Windows.
