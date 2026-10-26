#!/usr/bin/env python3
"""Wrap the source icons in a dark metallic mobile-theme tile.

Usage:
    python3 tools/theme.py                  # build ./themed with the default shade
    python3 tools/theme.py --preset bronze  # build with another shade
    python3 tools/theme.py --list           # show available shades

Each output keeps the original artwork untouched (pure vector, brand colours) and
adds a gunmetal tile, brushed texture, metallic rim and a soft drop shadow, so the
set reads as one mobile icon pack without redrawing any logo.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "svg"

# ---------------------------------------------------------------- shade presets
# base   : tile gradient, three stops (top-left -> middle -> bottom-right)
# rim    : hairline edge, three stops
# sheen  : colour of the specular sweep
# brush  : colour of the brushed-metal lines
PRESETS = {
    "gunmetal": {
        "label": "Gunmetal (default)",
        "base": ("#3c444f", "#242a32", "#11151a"),
        "rim": ("#a8b4c2", "#39414b", "#79879a"),
        "sheen": "#dfe8f2",
        "brush": "#ffffff",
        "sheen_opacity": 0.12,
        "brush_opacity": 0.05,
    },
    "graphite": {
        "label": "Graphite + steel blue",
        "base": ("#39424e", "#212933", "#0f141b"),
        "rim": ("#9fb6cf", "#2f3b48", "#6d89a6"),
        "sheen": "#cfe4ff",
        "brush": "#dbe9ff",
        "sheen_opacity": 0.14,
        "brush_opacity": 0.055,
    },
    "bronze": {
        "label": "Bronze",
        "base": ("#4a4038", "#2b2420", "#15110e"),
        "rim": ("#c9ab84", "#453a30", "#8d7355"),
        "sheen": "#ffe6c4",
        "brush": "#ffd9a8",
        "sheen_opacity": 0.13,
        "brush_opacity": 0.055,
    },
    "obsidian": {
        "label": "Obsidian (near black)",
        "base": ("#2c323b", "#191d23", "#0a0c0f"),
        "rim": ("#8f9cad", "#2a313a", "#5f6b7a"),
        "sheen": "#e8f0fa",
        "brush": "#ffffff",
        "sheen_opacity": 0.10,
        "brush_opacity": 0.045,
    },
}

# icons whose artwork is dark enough to disappear into the metal: a light halo
# is added behind the artwork so the silhouette still separates.
DARK_ART = {
    "unity", "epic-games", "windows-terminal", "7zip-file-manager", "7zip-help",
    "chatgpt", "antigravity", "antigravity-ide", "visual-studio-code", "nodejs",
    "unity-hub", "rovyl", "terminal",
}

CANVAS = 256
TILE = (8, 8, 240, 240)          # x, y, w, h of the metal tile
ART = 168                         # square box the artwork is fitted into
CORNER = 58                       # tile corner radius

# Adobe Illustrator exports carry a DOCTYPE entity list we drop; resolve the few
# entities those files actually reference so the markup stays valid XML.
ENTITIES = {
    "ns_extend": "http://ns.adobe.com/Extensibility/1.0/",
    "ns_ai": "http://ns.adobe.com/AdobeIllustrator/10.0/",
    "ns_graphs": "http://ns.adobe.com/Graphs/1.0/",
    "ns_sfw": "http://ns.adobe.com/SaveForWeb/1.0/",
    "ns_custom": "http://ns.adobe.com/GenericCustomNamespace/1.0/",
    "ns_adobe_xpath": "http://ns.adobe.com/XPath/1.0/",
}
BUILTIN = {"amp", "lt", "gt", "quot", "apos"}

INHERIT_ATTRS = {
    "fill", "stroke", "stroke-width", "stroke-linecap", "stroke-linejoin",
    "stroke-dasharray", "stroke-miterlimit", "fill-rule", "clip-rule",
    "fill-opacity", "stroke-opacity", "opacity", "color",
}


def resolve_entities(text: str) -> str:
    def sub(m: re.Match) -> str:
        name = m.group(1)
        if name in BUILTIN:
            return m.group(0)
        return ENTITIES.get(name, "")
    return re.sub(r"&([\w.-]+);", sub, text)


def read_svg(path: Path) -> tuple[str, str]:
    """Return (viewBox, inner markup) for a source svg."""
    raw = path.read_text(encoding="utf-8", errors="replace")
    m = re.search(r'<svg\b([^>]*)>', raw, re.S)
    if not m:
        raise ValueError(f"{path.name}: no <svg> root")
    attrs = m.group(1)
    vb = re.search(r'viewBox="([^"]+)"', attrs)
    if vb:
        view_box = vb.group(1).strip()
    else:  # fall back to width/height
        w = re.search(r'width="([\d.]+)', attrs)
        h = re.search(r'height="([\d.]+)', attrs)
        if not (w and h):
            raise ValueError(f"{path.name}: no viewBox or width/height")
        view_box = f"0 0 {float(w.group(1)):g} {float(h.group(1)):g}"
    inner = raw[m.end():]
    inner = re.sub(r"</svg>\s*$", "", inner.strip(), flags=re.S)
    # strip <title>/<desc> wrappers that would sit oddly inside a tile
    inner = re.sub(r"<title>.*?</title>", "", inner, flags=re.S)
    # drop Illustrator save-for-web / metadata blocks that need DTD entities
    inner = re.sub(r"<sfw\b.*?</sfw>", "", inner, flags=re.S)
    inner = resolve_entities(inner)
    # keep every namespace prefix the artwork relies on (xlink:, svg:, ...)
    ns = [n for n in re.findall(r'(xmlns:[\w-]+="[^"]*")', attrs) if '="&' not in n]
    # presentation attributes on the source root must travel with the artwork,
    # otherwise shapes that relied on them inherit SVG defaults (black fill).
    inherit = [
        f'{k}="{v}"'
        for k, v in re.findall(r'([\w:-]+)="([^"]*)"', attrs)
        if k in INHERIT_ATTRS
    ]
    return view_box, inner, ns, " ".join(inherit)


def fit(view_box: str, box: int):
    """Scale + centre the source viewBox inside a box x box square."""
    parts = [float(p) for p in re.split(r"[\s,]+", view_box) if p.strip()]
    vx, vy, vw, vh = (parts + [0, 0, 256, 256])[:4]
    s = box / max(vw, vh)
    tx = (CANVAS - vw * s) / 2 - vx * s
    ty = (CANVAS - vh * s) / 2 - vy * s
    return f"translate({tx:.4f} {ty:.4f}) scale({s:.6f})"


def tile_markup(p: dict, plate: bool = False) -> str:
    x, y, w, h = TILE
    plate_markup = ""
    if plate:
        plate_markup = (
            '  <!-- machined face the artwork sits on -->\n'
            '  <rect x="42" y="42" width="172" height="172" rx="40" fill="#ffffff" fill-opacity="0.055"/>\n'
            '  <rect x="42" y="42" width="172" height="172" rx="40" fill="none" '
            'stroke="url(#mt-rim)" stroke-opacity="0.75" stroke-width="2"/>\n'
        )
    base, rim = p["base"], p["rim"]
    return f"""  <defs>
    <linearGradient id="mt-base" x1="0.12" y1="0" x2="0.88" y2="1">
      <stop offset="0" stop-color="{base[0]}"/>
      <stop offset="0.52" stop-color="{base[1]}"/>
      <stop offset="1" stop-color="{base[2]}"/>
    </linearGradient>
    <radialGradient id="mt-sheen" cx="0.24" cy="0.12" r="0.85">
      <stop offset="0" stop-color="{p['sheen']}" stop-opacity="{p['sheen_opacity']}"/>
      <stop offset="0.55" stop-color="{p['sheen']}" stop-opacity="{p['sheen_opacity'] * 0.28:.3f}"/>
      <stop offset="1" stop-color="{p['sheen']}" stop-opacity="0"/>
    </radialGradient>
    <linearGradient id="mt-rim" x1="0.1" y1="0" x2="0.9" y2="1">
      <stop offset="0" stop-color="{rim[0]}" stop-opacity="0.62"/>
      <stop offset="0.5" stop-color="{rim[1]}" stop-opacity="0.35"/>
      <stop offset="1" stop-color="{rim[2]}" stop-opacity="0.5"/>
    </linearGradient>
    <linearGradient id="mt-top" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#ffffff" stop-opacity="0.26"/>
      <stop offset="0.35" stop-color="#ffffff" stop-opacity="0.05"/>
      <stop offset="1" stop-color="#ffffff" stop-opacity="0"/>
    </linearGradient>
    <pattern id="mt-brush" width="6" height="6" patternUnits="userSpaceOnUse"
             patternTransform="rotate(45)">
      <path d="M0 0v6" stroke="{p['brush']}" stroke-opacity="{p['brush_opacity']}" stroke-width="1"/>
    </pattern>
    <clipPath id="mt-clip">
      <rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{CORNER}"/>
    </clipPath>
    <filter id="mt-shadow" x="-30%" y="-30%" width="160%" height="160%">
      <feDropShadow dx="0" dy="3" stdDeviation="5" flood-color="#000000" flood-opacity="0.5"/>
    </filter>
    <filter id="mt-inset" x="-20%" y="-20%" width="140%" height="140%">
      <feGaussianBlur stdDeviation="9"/>
    </filter>
    <filter id="mt-halo" x="-40%" y="-40%" width="180%" height="180%">
      <feDropShadow dx="0" dy="0" stdDeviation="8" flood-color="#c8dcf2" flood-opacity="0.62"/>
    </filter>
  </defs>

  <!-- metal tile -->
  <rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{CORNER}" fill="url(#mt-base)"/>
  <g clip-path="url(#mt-clip)">
    <rect x="{x}" y="{y}" width="{w}" height="{h}" fill="url(#mt-brush)"/>
    <rect x="{x}" y="{y}" width="{w}" height="{h}" fill="url(#mt-sheen)"/>
    <!-- inner shadow around the edges -->
    <rect x="{x + 7}" y="{y + 7}" width="{w - 14}" height="{h - 14}" rx="{CORNER - 6}"
          fill="none" stroke="#000000" stroke-opacity="0.55" stroke-width="16" filter="url(#mt-inset)"/>
    <!-- top face highlight -->
    <rect x="{x}" y="{y}" width="{w}" height="{h}" fill="url(#mt-top)"/>
  </g>
  <!-- metallic rim -->
  <rect x="{x + 1}" y="{y + 1}" width="{w - 2}" height="{h - 2}" rx="{CORNER - 1}"
        fill="none" stroke="url(#mt-rim)" stroke-width="2"/>
{plate_markup}"""


def build(name: str, preset: str, plate: bool = False) -> str:
    p = PRESETS[preset]
    view_box, inner, ns, inherit = read_svg(SRC / f"{name}.svg")
    transform = fit(view_box, ART)
    ns_attr = (" " + " ".join(ns)) if ns else ""
    inherit_attr = (" " + inherit) if inherit else ""
    if name in DARK_ART:
        art_open = (
            '    <g filter="url(#mt-halo)">\n'
            '      <g filter="url(#mt-shadow)">\n'
        )
        art_close = "      </g>\n    </g>"
    else:
        art_open = '    <g filter="url(#mt-shadow)">\n'
        art_close = "    </g>"
    return f"""<svg xmlns="http://www.w3.org/2000/svg"{ns_attr} viewBox="0 0 256 256" width="256" height="256">
<metadata>Dark metallic mobile theme — {p['label']} — generated from svg/{name}.svg</metadata>
{tile_markup(p, plate)}
  <!-- artwork -->
  <g transform="{transform}"{inherit_attr}>
{art_open}{inner}
{art_close}
  </g>
</svg>
"""


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--preset", default="gunmetal", choices=sorted(PRESETS))
    ap.add_argument("--out", default="themed")
    ap.add_argument("--plate", action="store_true",
                    help="add a subtle machined face behind the artwork")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args(argv)

    if args.list:
        for k, v in PRESETS.items():
            print(f"  {k:10s} {v['label']}")
        return 0

    out_dir = ROOT / args.out
    out_dir.mkdir(parents=True, exist_ok=True)
    names = sorted(p.stem for p in SRC.glob("*.svg"))
    if not names:
        print("no source icons found", file=sys.stderr)
        return 1

    ok = 0
    for n in names:
        try:
            (out_dir / f"{n}.svg").write_text(build(n, args.preset, args.plate), encoding="utf-8")
            ok += 1
        except Exception as exc:  # noqa: BLE001 - report and continue
            print(f"  !! {n}: {exc}", file=sys.stderr)
    print(f"{ok}/{len(names)} themed with preset '{args.preset}' -> {out_dir}")
    return 0 if ok == len(names) else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
