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

} // namespace shelllayout

#endif // SHELL_LAYOUT_H
