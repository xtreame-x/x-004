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
  - **App Launcher**: Fullscreen searchable launcher overlay containing 24 curated system apps and utilities with real-time text filtering.
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
    -ld2d1 -ldwrite -luser32 -lgdi32 -lwinhttp -lwlanapi -lbthprops
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
| `Click Shelf Clock` | Toggle Quick Settings flyout |
| `Click Launcher Pill` | Open searchable Application Launcher |
| `Type in Launcher` | Real-time filter apps |

---

## 📄 License

MIT License. Designed and crafted with precision for Windows.
