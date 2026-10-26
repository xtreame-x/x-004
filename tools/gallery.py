#!/usr/bin/env python3
"""Build icons.html: a preview page for the dark metallic icon packs.

Reads the file names from svg/ (the untitled source set) and points at
themes/<preset>/<name>.svg, with a live shade switcher.
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "svg"
PRESETS = ["gunmetal", "graphite", "bronze", "obsidian"]

LABELS = {
    "chatgpt": "ChatGPT", "claude": "Claude", "rovyl": "Rovyl", "pycharm": "PyCharm",
    "android-studio": "Android Studio", "unity": "Unity", "unity-hub": "Unity Hub",
    "visual-studio-code": "VS Code", "visual-studio-installer": "Visual Studio Installer",
    "windows-terminal": "Windows Terminal", "nodejs": "Node.js", "git": "Git",
    "google-chrome": "Google Chrome", "epic-games": "Epic Games Launcher",
    "spotify": "Spotify", "telegram": "Telegram Desktop", "linkedin": "LinkedIn",
    "zoho-notebook": "Zoho Notebook", "clipchamp": "Clipchamp",
    "microsoft-todo": "Microsoft To Do", "media-player": "Media Player",
    "phone-link": "Phone Link", "xbox-gaming": "Xbox / Gaming",
    "intel-arc": "Intel Arc", "dolby-access": "Dolby Access",
    "dev-home": "Dev Home (Preview)", "7zip-file-manager": "7-Zip File Manager",
    "7zip-help": "7-Zip Help", "antigravity": "Antigravity",
    "antigravity-ide": "Antigravity IDE", "arduino-ide": "Arduino IDE",
    "dell-digital-delivery": "Dell Digital Delivery",
    "dell-supportassist": "Dell SupportAssist", "character-map": "Character Map",
    "component-services": "Component Services",
    "computer-management": "Computer Management", "control-panel": "Control Panel",
    "defrag": "Defragment and Optimize", "disk-cleanup": "Disk Cleanup",
    "snipping-tool": "Snipping Tool", "sticky-notes": "Sticky Notes (new)",
    "power-automate": "Power Automate", "onenote": "OneNote",
    "powerpoint": "PowerPoint", "word": "Word", "excel": "Excel",
    "access": "Access", "publisher": "Publisher", "outlook": "Outlook",
    "teams": "Microsoft Teams", "onedrive": "OneDrive", "copilot": "Copilot",
    "calculator": "Calculator", "calendar": "Calendar", "camera": "Camera",
    "clock": "Clock", "paint": "Paint", "notepad": "Notepad", "photos": "Photos",
    "maps": "Maps", "news": "News", "weather": "Weather",
}

PAGE = """<!doctype html>
<html lang="en" data-theme="dark">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Dark Metallic Icon Pack — @@COUNT@@ icons</title>
<style>
:root{--bg:#0d1015;--surface:#161b22;--surface-2:#1d232c;--text:#e8eef6;--muted:#8b98ab;--line:#2a323d;--accent:#7fb2e5}
[data-theme=light]{--bg:#eef1f6;--surface:#fff;--surface-2:#f3f6fa;--text:#0f172a;--muted:#5a6a7d;--line:#dbe3ec;--accent:#0a66c2}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.5 ui-sans-serif,system-ui,"Segoe UI",Roboto,sans-serif}
header{position:sticky;top:0;z-index:10;background:color-mix(in srgb,var(--bg) 86%,transparent);backdrop-filter:blur(14px);border-bottom:1px solid var(--line)}
.bar{max-width:1200px;margin:0 auto;padding:16px 22px;display:flex;gap:14px;align-items:center;flex-wrap:wrap}
h1{font-size:16px;margin:0;font-weight:640;letter-spacing:-.01em}
h1 span{color:var(--muted);font-weight:500}
.tools{margin-left:auto;display:flex;gap:9px;align-items:center;flex-wrap:wrap}
input,select,button{background:var(--surface);border:1px solid var(--line);color:var(--text);border-radius:10px;padding:8px 11px;font:inherit;outline:none}
input[type=search]{min-width:180px}
input:focus,select:focus{border-color:var(--accent)}
button{cursor:pointer}
button:hover{border-color:var(--accent);color:var(--accent)}
main{max-width:1200px;margin:0 auto;padding:22px 22px 80px}
.hint{color:var(--muted);font-size:13px;margin:0 0 20px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(126px,1fr));gap:14px}
.card{display:flex;flex-direction:column;align-items:center;gap:9px;padding:16px 10px 13px;background:var(--surface);border:1px solid var(--line);border-radius:18px;text-decoration:none;color:inherit;transition:.16s ease}
.card:hover{transform:translateY(-2px);border-color:var(--accent);background:var(--surface-2)}
.art{display:grid;place-items:center;width:86px;height:86px}
.art img{width:var(--s,72px);height:var(--s,72px);display:block}
.name{font-size:12.5px;font-weight:600;text-align:center;line-height:1.25}
.file{font-size:10.5px;color:var(--muted);font-family:ui-monospace,SFMono-Regular,Menlo,monospace}
.empty{color:var(--muted);text-align:center;padding:44px 0}
</style></head>
<body>
<header><div class="bar">
  <h1>Dark metallic icon pack <span>· @@COUNT@@ icons</span></h1>
  <div class="tools">
    <input type="search" id="q" placeholder="Filter…" autocomplete="off">
    <select id="shade" title="Metal shade">@@SHADES@@</select>
    <select id="size" title="Preview size">
      <option value="36">36 px</option><option value="52">52 px</option>
      <option value="72" selected>72 px</option><option value="104">104 px</option><option value="132">132 px</option>
    </select>
    <button id="theme">Light page</button>
  </div>
</div></header>
<main>
  <p class="hint">Click an icon to download the SVG. Artwork is original vector, the tile is generated by <code>tools/theme.py</code> — switch the shade above to recolour every icon at once.</p>
  <div class="grid" id="grid">@@CARDS@@</div>
  <p class="empty" id="empty" hidden>Nothing matches that filter.</p>
</main>
<script>
const grid = document.getElementById('grid');
const q = document.getElementById('q');
const empty = document.getElementById('empty');
const shade = document.getElementById('shade');

q.addEventListener('input', () => {
  const v = q.value.trim().toLowerCase();
  let shown = 0;
  grid.querySelectorAll('.card').forEach(c => {
    const hit = !v || c.dataset.name.includes(v);
    c.style.display = hit ? '' : 'none';
    if (hit) shown++;
  });
  empty.hidden = shown > 0;
});

shade.addEventListener('change', () => {
  const preset = shade.value;
  grid.querySelectorAll('.card').forEach(c => {
    const img = c.querySelector('img');
    img.src = `themes/${preset}/${img.dataset.file}`;
    c.href = `themes/${preset}/${c.dataset.file}`;
  });
});

const size = document.getElementById('size');
size.addEventListener('change', () => {
  document.documentElement.style.setProperty('--s', size.value + 'px');
});

const root = document.documentElement, btn = document.getElementById('theme');
btn.addEventListener('click', () => {
  const light = root.dataset.theme === 'light';
  root.dataset.theme = light ? 'dark' : 'light';
  btn.textContent = light ? 'Light page' : 'Dark page';
});
</script></body></html>
"""


def main() -> int:
    names = sorted(p.stem for p in SRC.glob("*.svg"))
    cards = []
    for n in names:
        label = LABELS.get(n, n.replace("-", " ").title())
        cards.append(
            f'<a class="card" href="themes/gunmetal/{n}.svg" download="{n}.svg" '
            f'data-file="{n}.svg" data-name="{label.lower()}">'
            f'<span class="art"><img src="themes/gunmetal/{n}.svg" alt="{label}" '
            f'width="72" height="72" data-file="{n}.svg" loading="lazy"></span>'
            f'<span class="name">{label}</span>'
            f'<span class="file">{n}.svg</span></a>'
        )
    shade_options = "".join(
        f'<option value="{p}"{" selected" if p == "gunmetal" else ""}>{p.title()}</option>'
        for p in PRESETS
    )
    html = (PAGE.replace("@@COUNT@@", str(len(names)))
                .replace("@@CARDS@@", "\n  ".join(cards))
                .replace("@@SHADES@@", shade_options))
    (ROOT / "icons.html").write_text(html, encoding="utf-8")

    manifest = {
        "count": len(names),
        "presets": PRESETS,
        "default": "gunmetal",
        "icons": [
            {"name": LABELS.get(n, n.replace("-", " ").title()),
             "source": f"svg/{n}.svg",
             "themed": {p: f"themes/{p}/{n}.svg" for p in PRESETS}}
            for n in names
        ],
    }
    (ROOT / "icons.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"gallery + manifest written for {len(names)} icons, presets: {', '.join(PRESETS)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
