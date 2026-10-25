# x-004: Obsidian Monolith Desktop Shell

A high-performance, hardware-accelerated desktop shell for Windows written in native C++ using **Direct2D**, **DirectWrite**, and the **Win32 API**. Engineered with a dark metallic "Neutral Obsidian" palette, glassmorphism surfaces, and live hardware & network integrations.

---

## ✨ Features

- **Obsidian Monolith Aesthetic & Frosted Glassmorphism**:
  - Precision dark metallic gradient palette (`#090A0D` → `#111317` → `#181B22` → `#0D0E12`).
  - **Dark Frosted Glass Transparency**: Translucent dark charcoal base (`rgba(14, 17, 24, 0.52)`), upper ambient specular sheen reflection (`rgba(255, 255, 255, 0.038)`), 1px beveled glass rim perimeter (`rgba(255, 255, 255, 0.13)`), razor-sharp top edge highlight hairline (`rgba(255, 255, 255, 0.22)`), and deep ambient drop-shadow.
  - Active emerald accent badges (`#34D399` / `#10B981`) for battery and market indicators.
  - Crisp typography rendered via hardware-accelerated **DirectWrite**.

- **Custom Wallpaper Engine & Preset Gallery**:
  - **Native WIC Decoder**: Integrates Windows Imaging Component (`IWICImagingFactory`) to load and aspect-fill any `.jpg`, `.jpeg`, `.png`, `.bmp`, or `.webp` image directly into Direct2D bitmaps (`ID2D1Bitmap`).
  - **Bundled Preset 0 (Misty Castle)**: Packaged atmospheric fantasy landscape (`wallpaper_glass.jpg`) paired with dark frosted glass cards.
  - **Custom Image File Picker**: Select any custom image from disk via the native Windows Open File dialog (`GetOpenFileNameW`) in **Settings -> Appearance** or via the desktop context menu.
  - **Desktop Right-Click Context Menu**: Right-click anywhere on the empty desktop canvas to quickly select "Choose wallpaper...", "Open Settings", or toggle fullscreen.
  - **Cross-Fade Transitions & Persistence**: Smooth animated cross-fades when changing wallpapers, automatically saved to `shell.ini` via `custom_wp` and `wallpaper`.

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
  - **Swipeable Home Pager & Predictive Physics**: 2-page desktop experience. Page 0 hosts interactive desktop widgets; Page 1 hosts a direct 24-app grid. Powered by a 2nd-order critically damped harmonic spring (`\omega = 14.5\text{ rad/s}`) with zero-jerk release velocity carryover, 120ms weighted least-squares regression velocity tracking, 18ms latency-compensating lookahead prediction, and smooth hyperbolic rubber-band boundaries. Supports mouse drags, touchpad 2-finger horizontal swipes (`WM_MOUSEHWHEEL`), arrow keys, and animated indicator dots.
  - **Ultra-Smooth 1ms Frame Pacing**: Utilizes Windows multimedia timers (`timeBeginPeriod(1)`) with a 100 Hz cadence to eliminate micro-stuttering across 60 Hz to 165 Hz displays.
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
    -ld2d1 -ldwrite -luser32 -lgdi32 -lwinhttp -lwlanapi -lbthprops \
    -lshell32 -lwinmm -lole32 -lwindowscodecs -lcomdlg32
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
| `Esc` | Dismiss App Launcher, Settings, or Quick Settings flyout |
| `←` / `→` | Switch between Home Screen pages (Widgets / Apps) |
| `Swipe / Drag` | Smooth horizontal drag or 2-finger touchpad swipe between pages |
| `Right-Click Desktop` | Open context menu (Choose wallpaper, Settings, Fullscreen) |
| `Ctrl +` / `Ctrl -` | Increase / decrease interface text size (90% – 150%) |
| `Ctrl 0` | Reset interface text size to 115% |
| `Click Shelf Clock` | Toggle Quick Settings flyout |
| `Click Launcher Pill` | Open searchable Application Launcher |
| `Type in Launcher` | Real-time filter apps |

---

## 📄 License

MIT License. Designed and crafted with precision for Windows.
