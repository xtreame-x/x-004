// ============================================================================
//  shell_layout.h  -  pure layout and hit-transform maths for Obsidian Shell.
//
//  Deliberately free of <windows.h> and of every other platform header, so the
//  same code that positions the desktop widgets and maps their hit rectangles
//  can be compiled and exercised by a plain host compiler (tests/layout_test.cpp).
//  Nothing here draws or allocates: it only turns sizes and a scale factor into
//  rectangles in the 1920x1200 design space.
// ============================================================================
#ifndef SHELL_LAYOUT_H
#define SHELL_LAYOUT_H

namespace shelllayout {

// Design-space canvas. Kept in sync with DW/DH in UI_shell.cpp.
constexpr float kDesignW = 1920.f;
constexpr float kDesignH = 1200.f;

enum WidgetId {
    W_CLOCK = 0,
    W_WEATHER,
    W_FINANCE,
    W_NOTES,
    W_MEDIA,
    W_SYSTEM,
    W_RECENT,
    W_SHORTCUTS,
    W_COUNT
};

// Base geometry of each widget, authored at scale 1.0 and grouped into three
// columns: the tall left stack, the middle stack, and the small right stack.
struct Spec {
    int   id;
    int   column;
    float w, h;
};

inline const Spec* Specs(int& n) {
    static const Spec s[] = {
        { W_CLOCK,     0, 420.f, 200.f },
        { W_NOTES,     0, 420.f, 352.f },
        { W_MEDIA,     0, 420.f, 240.f },
        { W_WEATHER,   1, 300.f, 200.f },
        { W_FINANCE,   1, 300.f, 200.f },
        { W_SYSTEM,    1, 300.f, 206.f },
        { W_RECENT,    2, 300.f, 216.f },
        { W_SHORTCUTS, 2, 300.f, 232.f },
    };
    n = (int)(sizeof(s) / sizeof(s[0]));
    return s;
}

// The three sizes offered in Settings > Appearance. The largest is chosen so
// the tallest column (clock + notes + media) just fills the band between the
// top margin and the shelf on the 1920x1200 canvas.
inline const float* Scales(int& n) {
    static const float s[3] = { 1.0f, 1.1f, 1.2f };
    n = 3;
    return s;
}

struct Rect  { int id; float x, y, w, h; };
struct Point { float x, y; };

// Local (widget) space -> design space. Drawing uses the equivalent D2D matrix
// scale(s) * translate(tx,ty); hit rectangles are mapped with this struct so
// both paths agree exactly.
struct Xform {
    float s  = 1.f;
    float tx = 0.f;
    float ty = 0.f;
};

inline Point MapPoint(Point p, Xform t) {
    Point r;
    r.x = p.x * t.s + t.tx;
    r.y = p.y * t.s + t.ty;
    return r;
}

inline void MapRect(float& x, float& y, float& w, float& h, Xform t) {
    const Point p = MapPoint({ x, y }, t);
    x = p.x;
    y = p.y;
    w *= t.s;
    h *= t.s;
}

// Largest scale (<= the requested one) at which the tallest visible column still
// ends above shelfTop. Callers draw and hit-test with this value, so a widget
// is never silently dropped by Compute() just because the user picked a size
// that is too big for the currently visible set.
inline float FitScale(const bool visible[W_COUNT], float scale, float top, float rowGap,
                      float shelfTop) {
    int n = 0;
    const Spec* spec = Specs(n);
    if (scale < 0.5f) scale = 0.5f;
    float colH[3] = { 0.f, 0.f, 0.f };
    int   cnt[3]  = { 0, 0, 0 };
    for (int i = 0; i < n; ++i) {
        if (!visible[spec[i].id]) continue;
        colH[spec[i].column] += spec[i].h;
        ++cnt[spec[i].column];
    }
    float tallest = 0.f;
    for (int c = 0; c < 3; ++c) {
        if (cnt[c] == 0) continue;
        const float hh = colH[c] + rowGap * (float)(cnt[c] - 1);
        if (hh > tallest) tallest = hh;
    }
    if (tallest <= 0.f) return scale;
    const float maxS = (shelfTop - top) / tallest * 0.999f;
    return scale < maxS ? scale : maxS;
}

// Stack the visible widgets: column by column from originX, row by row from
// top. Hidden widgets take no space, and an entirely hidden column collapses
// instead of leaving a gap. Rectangles that would reach the shelf are dropped
// rather than drawn underneath it. Returns the number of rects written to out,
// which must have room for W_COUNT entries.
inline int Compute(const bool visible[W_COUNT], float scale, float originX, float top,
                   float columnGap, float rowGap, float shelfTop, Rect out[W_COUNT]) {
    int n = 0;
    const Spec* spec = Specs(n);
    if (scale < 0.5f) scale = 0.5f;

    float colW[3] = { 0.f, 0.f, 0.f };
    for (int i = 0; i < n; ++i) {
        const Spec& s = spec[i];
        if (visible[s.id] && s.w > colW[s.column]) colW[s.column] = s.w;
    }

    float colX[3] = { 0.f, 0.f, 0.f };
    float cursor = originX;
    for (int c = 0; c < 3; ++c) {
        if (colW[c] <= 0.f) continue;
        colX[c] = cursor;
        cursor += (colW[c] + columnGap) * scale;
    }

    float y[3] = { top, top, top };
    int used = 0;
    for (int i = 0; i < n; ++i) {
        const Spec& s = spec[i];
        if (!visible[s.id]) continue;

        Rect r;
        r.id = s.id;
        r.x = colX[s.column];
        r.y = y[s.column];
        r.w = s.w * scale;
        r.h = s.h * scale;

        if (r.y + r.h > shelfTop) continue;          // would collide with the shelf
        out[used++] = r;
        y[s.column] += r.h + rowGap * scale;
    }
    return used;
}

// ---------------------------------------------------------------------------
//  Layout presets.
//
//  A layout is an ordered list of rows; a row is an ordered list of cells; a
//  cell is a stack of widget ids with a fixed design-space width and an
//  optional vertical offset (dy) used to stagger a row. Rows advance by their
//  tallest cell, hidden widgets take no space inside their stack, and a stack
//  whose widgets are all hidden contributes only its dy.
//
//  Widgets keep their authored height but stretch horizontally to the cell
//  width, so every drawer may safely be handed a wider card than it was
//  authored at (its right-aligned pieces are anchored to w). The classic
//  three-column flow is expressed by preset 0, which reproduces Compute()
//  exactly: same origin, gaps and column widths.
// ---------------------------------------------------------------------------
constexpr int LAY_COUNT     = 7;   // classic + six new arrangements
constexpr int LAY_MAX_ROWS  = 4;
constexpr int LAY_MAX_CELLS = 4;
constexpr int LAY_MAX_IDS   = 4;

struct Cell { float w; float dy; int ids[LAY_MAX_IDS]; int n; };
struct Row  { Cell cells[LAY_MAX_CELLS]; int n; };
struct LayoutDef {
    const wchar_t* name;
    const wchar_t* blurb;
    float originX, top, colGap, rowGap;
    int   n;
    Row   rows[LAY_MAX_ROWS];
};

inline float SpecH(int id) {
    int n = 0;
    const Spec* s = Specs(n);
    for (int i = 0; i < n; ++i) if (s[i].id == id) return s[i].h;
    return 0.f;
}

inline const LayoutDef* LayoutTable(int& n) {
    static const LayoutDef L[LAY_COUNT] = {
        // 0 — Classic: the original left-aligned three-column flow.
        { L"Classic", L"The original three-column flow",
          96.f, 88.f, 18.f, 18.f, 1, {
            { { { 420.f, 0.f, { W_CLOCK, W_NOTES, W_MEDIA }, 3 },
                { 300.f, 0.f, { W_WEATHER, W_FINANCE, W_SYSTEM }, 3 },
                { 300.f, 0.f, { W_RECENT, W_SHORTCUTS }, 2 } }, 3 },
        } },
        // 1 — Bento: full-width mosaic, three rows of unequal tiles.
        { L"Bento", L"Full-width mosaic of unequal tiles",
          96.f, 132.f, 24.f, 20.f, 3, {
            { { { 780.f, 0.f, { W_CLOCK }, 1 },
                { 460.f, 0.f, { W_WEATHER }, 1 },
                { 440.f, 0.f, { W_FINANCE }, 1 } }, 3 },
            { { { 560.f, 0.f, { W_NOTES }, 1 },
                { 560.f, 0.f, { W_MEDIA }, 1 },
                { 560.f, 0.f, { W_SYSTEM }, 1 } }, 3 },
            { { { 852.f, 0.f, { W_RECENT }, 1 },
                { 852.f, 0.f, { W_SHORTCUTS }, 1 } }, 2 },
        } },
        // 2 — Console: three full-width bands, hero clock on top.
        { L"Console", L"Full-width bands, hero clock on top",
          96.f, 96.f, 24.f, 18.f, 3, {
            { { { 1728.f, 0.f, { W_CLOCK }, 1 } }, 1 },
            { { { 414.f, 0.f, { W_WEATHER }, 1 },
                { 414.f, 0.f, { W_FINANCE }, 1 },
                { 414.f, 0.f, { W_SYSTEM }, 1 },
                { 414.f, 0.f, { W_RECENT }, 1 } }, 4 },
            { { { 690.f, 0.f, { W_NOTES }, 1 },
                { 560.f, 0.f, { W_MEDIA }, 1 },
                { 430.f, 0.f, { W_SHORTCUTS }, 1 } }, 3 },
        } },
        // 3 — Feed: two generous centred columns, tight vertical rhythm.
        { L"Feed", L"Two generous centred columns",
          88.f, 88.f, 24.f, 14.f, 1, {
            { { { 860.f, 0.f, { W_CLOCK, W_NOTES, W_WEATHER, W_FINANCE }, 4 },
                { 860.f, 0.f, { W_MEDIA, W_SYSTEM, W_RECENT, W_SHORTCUTS }, 4 } }, 2 },
        } },
        // 4 — Matrix: an even four-by-two grid of uniform tiles.
        { L"Matrix", L"Even four-by-two grid of uniform tiles",
          60.f, 232.f, 24.f, 24.f, 2, {
            { { { 430.f, 0.f, { W_CLOCK }, 1 },
                { 430.f, 0.f, { W_WEATHER }, 1 },
                { 430.f, 0.f, { W_FINANCE }, 1 },
                { 430.f, 0.f, { W_SYSTEM }, 1 } }, 4 },
            { { { 430.f, 0.f, { W_NOTES }, 1 },
                { 430.f, 0.f, { W_MEDIA }, 1 },
                { 430.f, 0.f, { W_RECENT }, 1 },
                { 430.f, 0.f, { W_SHORTCUTS }, 1 } }, 4 },
        } },
        // 5 — Rail: a narrow stats rail on the left, two wide cards right.
        { L"Rail", L"Narrow stats rail with wide cards beside it",
          106.f, 112.f, 24.f, 16.f, 1, {
            { { { 340.f, 0.f, { W_WEATHER, W_FINANCE, W_SYSTEM }, 3 },
                { 660.f, 0.f, { W_CLOCK, W_NOTES }, 2 },
                { 660.f, 0.f, { W_MEDIA, W_RECENT, W_SHORTCUTS }, 3 } }, 3 },
        } },
        // 6 — Cascade: three equal columns dropped to staggered heights.
        { L"Cascade", L"Three equal columns at staggered heights",
          96.f, 88.f, 24.f, 18.f, 1, {
            { { { 560.f, 0.f, { W_CLOCK, W_NOTES }, 2 },
                { 560.f, 96.f, { W_WEATHER, W_FINANCE, W_SYSTEM }, 3 },
                { 560.f, 48.f, { W_MEDIA, W_RECENT, W_SHORTCUTS }, 3 } }, 3 },
        } },
    };
    n = LAY_COUNT;
    return L;
}

// Total design-space height of the tallest cell in a layout (dy included) and
// the width of its widest row, both at scale 1. Used by FitLayout().
inline void LayoutNatural(int lay, const bool visible[W_COUNT],
                          float& tallest, float& widest) {
    int n = 0;
    const LayoutDef* L = LayoutTable(n);
    tallest = 0.f;
    widest = 0.f;
    if (lay < 0 || lay >= n) { tallest = 1.f; return; }
    const LayoutDef& d = L[lay];
    for (int r = 0; r < d.n; ++r) {
        float rowW = 0.f;
        for (int c = 0; c < d.rows[r].n; ++c) {
            const Cell& cell = d.rows[r].cells[c];
            float h = -d.rowGap;
            for (int k = 0; k < cell.n; ++k) {
                if (visible[cell.ids[k]]) h += SpecH(cell.ids[k]) + d.rowGap;
            }
            if (h < 0.f) h = 0.f;
            h += cell.dy;
            if (h > tallest) tallest = h;
            rowW += cell.w;
            if (c) rowW += d.colGap;
        }
        if (rowW > widest) widest = rowW;
    }
    if (tallest <= 0.f) tallest = 1.f;
}

// Largest scale (<= the requested one) at which the layout still fits between
// its top margin and the shelf, and still fits inside the design canvas.
inline float FitLayout(int lay, const bool visible[W_COUNT], float scale, float shelfTop) {
    if (scale < 0.5f) scale = 0.5f;
    int n = 0;
    const LayoutDef* L = LayoutTable(n);
    if (lay < 0 || lay >= n) return scale;
    float tallest = 0.f, widest = 0.f;
    LayoutNatural(lay, visible, tallest, widest);
    float fit = scale;
    const float maxH = (shelfTop - L[lay].top) / tallest * 0.999f;
    if (maxH < fit) fit = maxH;
    const float avail = kDesignW - 2.f * L[lay].originX;
    if (widest > 0.f && avail > 0.f) {
        const float maxW = avail / widest * 0.999f;
        if (maxW < fit) fit = maxW;
    }
    if (fit < 0.5f) fit = 0.5f;
    return fit;
}

// Place every visible widget of a layout. out must hold W_COUNT rects. Returns
// how many were written; rects that would reach the shelf are dropped rather
// than drawn underneath it.
inline int ComputeLayout(int lay, const bool visible[W_COUNT], float scale,
                         float shelfTop, Rect out[W_COUNT]) {
    int n = 0;
    const LayoutDef* L = LayoutTable(n);
    if (lay < 0 || lay >= n) lay = 0;
    scale = FitLayout(lay, visible, scale, shelfTop);

    const LayoutDef& d = L[lay];
    int used = 0;
    float y = d.top;
    for (int r = 0; r < d.n; ++r) {
        float x = d.originX;
        float rowH = 0.f;
        for (int c = 0; c < d.rows[r].n; ++c) {
            const Cell& cell = d.rows[r].cells[c];
            float sy = y + cell.dy * scale;
            float h = -d.rowGap;
            for (int k = 0; k < cell.n; ++k) {
                const int id = cell.ids[k];
                if (!visible[id]) continue;
                const float wh = SpecH(id);
                h += wh + d.rowGap;
                if (sy + wh * scale > shelfTop) {          // would hit the shelf
                    sy += (wh + d.rowGap) * scale;
                    continue;
                }
                if (used >= W_COUNT) break;
                Rect& o = out[used++];
                o.id = id;
                o.x  = x;
                o.y  = sy;
                o.w  = cell.w * scale;
                o.h  = wh * scale;
                sy += (wh + d.rowGap) * scale;
            }
            if (h < 0.f) h = 0.f;
            h += cell.dy;
            if (h > rowH) rowH = h;
            x += (cell.w + d.colGap) * scale;
        }
        y += (rowH + d.rowGap) * scale;
    }
    return used;
}

} // namespace shelllayout

#endif // SHELL_LAYOUT_H
