# x-004: Obsidian Monolith Desktop Shell

A high-performance, hardware-accelerated desktop shell for Windows written in native C++ using **Direct2D**, **DirectWrite**, and the **Win32 API**. Engineered with a dark metallic "Neutral Obsidian" palette, glassmorphism surfaces, and live hardware & network integrations.

---

## ✨ Features

- **Obsidian Monolith Aesthetic & Frosted Glass Transparency**:
  - Precision dark metallic gradient palette (`#090A0D` → `#111317` → `#181B22` → `#0D0E12`).
  - **Translucent Frosted Glass Cards**: Desktop widgets, bottom shelf dock, quick settings flyout, and window chrome render with a translucent frosted glass base (`rgba(14, 17, 24, 0.52)`), directional ambient light wash (`rgba(255, 255, 255, 0.038)`), 1px polished glass rim perimeter (`rgba(255, 255, 255, 0.13)`), and razor-sharp top edge highlight hairline (`rgba(255, 255, 255, 0.22)`).
  - Active emerald accent badges (`#34D399` / `#10B981`) for battery and market indicators.
  - Crisp typography rendered via hardware-accelerated **DirectWrite**.

- **Physics-Based Motion Smoothing (`smooth-scrollbar` & `react-native`)**:
  - **`smooth-scrollbar` Virtual Momentum Scrolling**: Decoupled scroll accumulator with exponential damping ($s_{t+1} = s_t + (s_{\text{target}} - s_t) \cdot (1 - e^{-\lambda \cdot dt})$) across all windows (Terminal, Files, Settings, Notes, Editor), replacing discrete wheel jumps with fluid momentum glides.
  - **Rock-Solid 1:1 Active Dragging**: Eliminates lookahead prediction jitter during active drag so the page adheres smoothly to the pointer without any "cracked" micro-jumps.
  - **`react-native` Critically Damped Harmonic Spring ($\zeta = 1.0, \omega = 13.5\text{ rad/s}$)**: Closed-form analytical ODE evaluation ensuring fastest possible convergence to target pages with zero overshoot.
  - **$C^1$ Velocity Continuity**: 120ms weighted least-squares regression velocity tracking ($WLS$) with quadratic age weighting $(1 - \text{age})^2$ transferring release velocity directly into the spring ODE on mouse/touch release with zero jerk ($\frac{da}{dt} < \infty$).
  - **Wheel & Gesture Momentum Injection**: Mouse wheel flicks immediately launch the spring with momentum velocity ($v = \pm 3.2$) rather than starting from rest.
  - **Predictive Target Projection**: Fling target projection $x_{\text{projected}} = x_0 + \frac{v_0}{1 - d}$ for intuitive page snapping.
  - **Bounded Rubber-Band Boundaries**: Hyperbolic overscroll resistance preventing harsh clamp stops at edges.

- **Live Wallpaper Background Engine & Accessibility**:
  - **Procedural Floating Particle Embers**: 48 organic glowing embers and fireflies floating upward with sinusoidal sway, breathing alpha pulsation, and ambient emerald aura.
  - **3D Mouse Parallax Depth**: Interactive subtle parallax shifting background layers based on mouse coordinates for 3D depth, with an on/off accessibility toggle.
  - **Quick Settings "Live Wallpaper" Tile**: 1-click toggle in the Quick Settings flyout to turn live particle motion on or pause it instantly.
  - **Wallpaper Background Contrast & Dimming Slider (Accessibility)**: Live 0% to 85% background scrim slider in Settings (`H_WPDIM`) allowing users to darken the wallpaper for crystal-clear widget and typography readability.
  - **Desktop Right-Click Quick Access**: Right-click context menu includes direct "Live wallpaper effects" toggle, custom wallpaper picker, and wallpaper cycler.
  - **Native WIC Decoder**: Integrates Windows Imaging Component (`IWICImagingFactory`) to load and aspect-fill any `.jpg`, `.jpeg`, `.png`, `.bmp`, or `.webp` image directly into Direct2D bitmaps (`ID2D1Bitmap`).
  - **Bundled Preset 0 (Misty Castle)**: Packaged atmospheric fantasy landscape (`wallpaper_glass.jpg`) paired with dark frosted glass cards.
  - **Cross-Fade Transitions & Persistence**: Smooth animated cross-fades when changing wallpapers, automatically saved to `shell.ini` via `custom_wp`, `wp_dim`, `parallax`, and `motion`.

- **Live Hardware & OS Telemetry**:
  - **Real Hardware Display Brightness & Master Audio Volume**: Direct hardware backlight control via Windows WMI (`WmiMonitorBrightnessMethods` / `WmiSetBrightness`) for laptop internal displays, with VESA DDC/CI (`highlevelmonitorconfigurationapi.h` / `dxva2.dll`) and GPU hardware gamma ramp (`SetDeviceGammaRamp`) fallbacks for external monitors. Integrates Windows Core Audio endpoint control (`IAudioEndpointVolume`) for physical system master volume. Runs in an asynchronous background hardware worker thread with atomic targets, ensuring 100+ fps slider dragging with zero UI lag, live percentage readouts, and release feedback toasts. Automatically synchronizes with live Windows brightness and volume levels on launch.
  - **Native Wi-Fi**: Directly calls Windows WLAN API (`wlanapi.h`) to detect the active SSID and actual Wi-Fi signal quality percentage.
  - **Native Bluetooth**: Queries Windows Bluetooth API (`bluetoothapis.h`) for radio state and paired/connected devices.
  - **System Metrics**: Real-time CPU load sampling via `GetSystemTimes`, RAM utilization via `GlobalMemoryStatusEx`, root disk free space, and system uptime.
  - **Battery & Power**: Real-time battery charge level and AC charging status via `GetSystemPowerStatus`.

- **Live Network Services (Pure Native WinHTTP)**:
  - **Bitcoin Telemetry**: Queries Coinbase REST API (`api.coinbase.com`) via asynchronous background threads using `winhttp.dll`. Renders dynamic 24-point hourly sparkline graphs with automatic green/red 24-hour gain/loss color-coding.
  - **Live Weather**: Fetches real-time temperature, condition descriptions, and daily highs/lows from `wttr.in` with custom procedural weather glyphs (rain, storm, cloud, sun).

- **Desktop Shell & Productivity**:
  - **Direct Real Windows App Launching**: App icons across the searchable Launcher, Home Screen grid, and Pinned Shelf directly execute real Windows programs and URI protocols (`explorer.exe`, Windows Terminal `wt.exe`/PowerShell, Windows Settings `ms-settings:`, Notepad, Google Chrome, Spotify, Windows Photos `ms-photos:`, Camera `microsoft.windows.camera:`, Calculator `calc.exe`, OneNote, Microsoft Store, Microsoft To Do, and Google Workspace web services) with graceful fallbacks.
  - **Swipeable Home Pager (Side Scroll)**: 2-page desktop experience with clean horizontal side scroll. Page 0 hosts interactive desktop widgets in the classic layout; Page 1 hosts the Side Screen with the full 48-app library. Powered by a 2nd-order critically damped harmonic spring (`\omega = 13.5\text{ rad/s}`) with zero-jerk release velocity carryover, 120ms weighted least-squares regression velocity tracking, and smooth hyperbolic rubber-band boundaries. Side-scroll horizontally via touchpad 2-finger horizontal swipe (`WM_MOUSEHWHEEL`), horizontal pointer swipe (`←` / `→`), Left/Right arrow keys, or the bottom frosted glass switcher capsule (`[ ⊞ Desktop ]` / `[ ▦ All Apps ]`).
  - **Side Screen All-Apps (Up and Down Scroll)**: Page 1 features a full library of 48 real applications with dedicated vertical up and down scroll. Completely separated from side scroll: vertical mouse wheel (`WM_MOUSEWHEEL`) purely scrolls down/up without ever flipping pages sideways, even at boundaries; vertical drag provides fluid momentum glide with release velocity tracking; and a frosted glass vertical scrollbar offers direct track clicking and thumb dragging.
  - **Ultra-Smooth 1ms Frame Pacing**: Utilizes Windows multimedia timers (`timeBeginPeriod(1)`) with a 100 Hz cadence to eliminate micro-stuttering across 60 Hz to 165 Hz displays.
  - **Dynamic Text Zoom**: Scale small interface typography on the fly with `Ctrl +`, `Ctrl -`, and `Ctrl 0` (range 90%–150%, default 115%), persisted automatically to `shell.ini`.
  - **App Launcher**: Fullscreen searchable launcher overlay containing 24 curated system apps and utilities with real-time text filtering and keyboard selection (`Enter`).
  - **Quick Settings Flyout**: ChromeOS-inspired system tray shelf with Wi-Fi, Bluetooth, Do Not Disturb, Night Light, live physical hardware brightness and volume sliders, real Windows workstation locking (`LockWorkStation`), and real Windows Settings launching (`ms-settings:` / `control.exe`).
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
    -lshell32 -lwinmm -lole32 -loleaut32 -lwbemuuid -ldxva2 -lwindowscodecs -lcomdlg32
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
| `←` / `→` | **Side Scroll**: Switch between Desktop (Home) and All Apps (Side Screen) |
| `Horizontal Swipe / Trackpad H-Wheel` | **Side Scroll**: 2-finger horizontal swipe between pages |
| `Click Capsule [ Desktop / All Apps ]` | **Side Scroll**: Direct 1-click page switcher at screen bottom |
| `Click [ ← Desktop ]` | **Side Scroll**: Back pill on the Side Screen header |
| `Mouse Wheel on Page 1` | **Up and Down Scroll**: Scroll through all 48 apps with momentum |
| `Vertical Drag on Page 1` | **Up and Down Scroll**: Direct drag with velocity carryover fling |
| `Drag Frosted Scrollbar` | **Up and Down Scroll**: Click track or drag thumb directly |
| `↑` / `↓` / `PgUp` / `PgDn` | **Up and Down Scroll**: Scroll through apps on the Side Screen |
| `Right-Click Desktop` | Open context menu (Choose wallpaper, Settings, Fullscreen) |
| `Ctrl +` / `Ctrl -` | Increase / decrease interface text size (90% – 150%) |
| `Ctrl 0` | Reset interface text size to 115% |
| `Click Shelf Clock` | Toggle Quick Settings flyout |
| `Click Launcher Pill` | Open searchable Application Launcher |
| `Type in Launcher` | Real-time filter apps |

---

## 📄 License

MIT License. Designed and crafted with precision for Windows.
