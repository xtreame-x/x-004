# Dark metallic icon pack — 62 apps, 4 shades

A mobile-theme take on the app list: every icon sits in a gunmetal squircle with a machined
face, brushed-metal texture, a metallic rim and a soft contact shadow — while the artwork inside
stays original vector at full brand colour, so nothing loses its identity to the theme.

**Preview:** open [`icons.html`](icons.html) — 62 icons, live **shade switcher**, adjustable
preview size, light/dark page, click any tile to download.

![Gunmetal, part one](artifacts/theme-a.png)

![Gunmetal, part two](artifacts/theme-b.png)

## What's in the folder

```
svg/                    62 source icons — flat, transparent, no tile (the artwork on its own)
themes/gunmetal/        62 themed icons  (default)
themes/graphite/        62 themed icons  (cool steel-blue)
themes/bronze/          62 themed icons  (warm)
themes/obsidian/        62 themed icons  (near-black)
tools/theme.py          the tile generator — every shade, every icon
tools/gallery.py        builds icons.html + icons.json
icons.html              preview page
icons.json              manifest: name, source path, all four themed paths
```

Each themed icon is ~3.5–16 KB, pure SVG: no rasters, no external references, no fonts —
text can never reflow or fall back.

## The tile recipe

Every icon gets the same construction, which is what makes the set read as one theme:

| Layer | What it does |
|---|---|
| Base | 3-stop linear gradient (top-left → bottom-right) in the preset's metal tone |
| Brushed texture | 1 px diagonal lines at ~5 % opacity, 45° — reads as machined metal, invisible at 24 px |
| Specular sweep | soft radial highlight in the upper-left, so the tile has a light source |
| Machined face | slightly lighter inset rounded square the artwork sits on, with a pale bezel ring |
| Inner shadow | blurred dark stroke clipped to the tile — gives the rim thickness |
| Metallic rim | 1 px gradient stroke, lighter at top-left, darker at bottom-right |
| Artwork | original logo, scaled to 168/256 (66 %), with a contact shadow |
| Halo | only on the 12 dark logos (Unity, Epic, ChatGPT, 7-Zip, Antigravity, VS Code…) so a black mark still separates from black metal |

Artwork is never recoloured or redrawn — the theme is the frame, not the logo.

## Shades

| Preset | Character |
|---|---|
| `gunmetal` | neutral graphite steel — the default |
| `graphite` | cooler, blue-steel rim and sheen |
| `bronze` | warm aged metal |
| `obsidian` | near-black, maximum contrast |

To recolour the entire set, edit the `PRESETS` dict at the top of `tools/theme.py` and re-run:

```bash
python3 tools/theme.py --preset gunmetal --plate --out themes/gunmetal
python3 tools/gallery.py          # refresh the preview page + manifest
```

`--preset` picks the shade, `--plate` adds the machined face (recommended), `--out` sets the
folder. A new preset needs only four colours: `base` (3 stops), `rim` (3 stops), `sheen`, `brush`.

## Naming

| App | File | App | File |
|---|---|---|---|
| ChatGPT | `chatgpt.svg` | Claude | `claude.svg` |
| Rovyl | `rovyl.svg` | PyCharm | `pycharm.svg` |
| Android Studio | `android-studio.svg` | Unity | `unity.svg` |
| Unity Hub | `unity-hub.svg` | Arduino IDE | `arduino-ide.svg` |
| Antigravity | `antigravity.svg` | Antigravity IDE | `antigravity-ide.svg` |
| VS Code | `visual-studio-code.svg` | Visual Studio Installer | `visual-studio-installer.svg` |
| Windows Terminal | `windows-terminal.svg` | Dev Home (Preview) | `dev-home.svg` |
| Node.js | `nodejs.svg` | Git | `git.svg` |
| Google Chrome | `google-chrome.svg` | Epic Games Launcher | `epic-games.svg` |
| Spotify | `spotify.svg` | Telegram Desktop | `telegram.svg` |
| LinkedIn | `linkedin.svg` | Zoho Notebook | `zoho-notebook.svg` |
| Word | `word.svg` | Excel | `excel.svg` |
| PowerPoint | `powerpoint.svg` | OneNote | `onenote.svg` |
| Access | `access.svg` | Publisher | `publisher.svg` |
| Outlook | `outlook.svg` | Teams | `teams.svg` |
| OneDrive | `onedrive.svg` | Power Automate | `power-automate.svg` |
| Copilot | `copilot.svg` | Microsoft To Do | `microsoft-todo.svg` |
| Clipchamp | `clipchamp.svg` | Media Player | `media-player.svg` |
| Phone Link | `phone-link.svg` | Maps | `maps.svg` |
| News | `news.svg` | Weather | `weather.svg` |
| Xbox / Gaming | `xbox-gaming.svg` | Sticky Notes (new) | `sticky-notes.svg` |
| Paint | `paint.svg` | Notepad | `notepad.svg` |
| Photos | `photos.svg` | Snipping Tool | `snipping-tool.svg` |
| Camera | `camera.svg` | Calculator | `calculator.svg` |
| Calendar | `calendar.svg` | Clock | `clock.svg` |
| Character Map | `character-map.svg` | Control Panel | `control-panel.svg` |
| Computer Management | `computer-management.svg` | Component Services | `component-services.svg` |
| Defragment and Optimize | `defrag.svg` | Disk Cleanup | `disk-cleanup.svg` |
| 7-Zip File Manager | `7zip-file-manager.svg` | 7-Zip Help | `7zip-help.svg` |
| Dell Digital Delivery | `dell-digital-delivery.svg` | Dell SupportAssist | `dell-supportassist.svg` |
| Intel Arc | `intel-arc.svg` | Dolby Access | `dolby-access.svg` |

## Using them

- **Web:** `<img src="themes/gunmetal/word.svg" width="64" height="64" alt="Word">` — the tile is
  part of the file, so no CSS background is needed.
- **Mobile / launcher themes:** every icon is a self-contained 256×256 tile; export at 180/192/512
  as your theme requires. Tiles are drawn to the edge of a 240 px area with an 8 px margin, so
  they can also be cropped tighter without clipping artwork.
- **Flat variants:** use `svg/` if you want the logo alone on transparency.

## Sourcing and licensing

- Brand artwork came from official vector sources where they exist (Wikimedia Commons logo
  files, `dashboard-icons`, `devicon`, `simple-icons`); the Windows utilities and the marks with
  no public vector were hand-authored. See the source list in `svg/` — each file is the original
  artwork, unmodified.
- `chatgpt.svg`, `rovyl.svg`, `antigravity.svg`, `antigravity-ide.svg` and `arduino-ide.svg` are
  interpretations, not vendor artwork.
- Product names and logos are trademarks of their respective owners; check each vendor's brand
  guidelines before shipping commercially.
