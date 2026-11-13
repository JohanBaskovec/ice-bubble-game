#include "gfx.h"

#include <math.h>

#define PI_F 3.14159265f
#define MAX_VERTS 2048
#define MAX_INDICES (MAX_VERTS * 3)

static SDL_Renderer *g_r;
static float g_ox, g_oy;
static float g_alpha = 1.0f;

static SDL_Vertex g_v[MAX_VERTS];
static int g_idx[MAX_INDICES];

void gfx_init(SDL_Renderer *r) { g_r = r; }
SDL_Renderer *gfx_renderer(void) { return g_r; }

/* ------------------------------------------------------------------------ */
/* Couleurs                                                                 */

Color rgba(Uint32 hex, float a)
{
    Color c = { ((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f, a };
    return c;
}

Color rgb(Uint32 hex) { return rgba(hex, 1.0f); }

static float hue2rgb(float p, float q, float t)
{
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0f / 6) return p + (q - p) * 6 * t;
    if (t < 0.5f) return q;
    if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
    return p;
}

Color hsl(float h, float s, float l, float a)
{
    h = fmodf(h, 360.0f);
    if (h < 0) h += 360.0f;
    h /= 360.0f;
    Color c;
    c.a = a;
    if (s <= 0) {
        c.r = c.g = c.b = l;
        return c;
    }
    float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
    float p = 2 * l - q;
    c.r = hue2rgb(p, q, h + 1.0f / 3);
    c.g = hue2rgb(p, q, h);
    c.b = hue2rgb(p, q, h - 1.0f / 3);
    return c;
}

Color color_lerp(Color a, Color b, float t)
{
    Color c = { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
    return c;
}

Color color_alpha(Color c, float a)
{
    c.a = a;
    return c;
}

/* ------------------------------------------------------------------------ */
/* Etat                                                                     */

void gfx_set_offset(float x, float y) { g_ox = x; g_oy = y; }
void gfx_get_offset(float *x, float *y) { *x = g_ox; *y = g_oy; }
void gfx_set_alpha(float a) { g_alpha = a; }
float gfx_get_alpha(void) { return g_alpha; }

void gfx_clip(const SDL_FRect *r)
{
    if (!r) {
        SDL_SetRenderClipRect(g_r, NULL);
        return;
    }
    SDL_Rect ir = { (int)floorf(r->x + g_ox), (int)floorf(r->y + g_oy), (int)ceilf(r->w), (int)ceilf(r->h) };
    SDL_SetRenderClipRect(g_r, &ir);
}

/* ------------------------------------------------------------------------ */
/* Construction de maillages                                                */

static int g_nv, g_ni;

static void begin(void) { g_nv = 0; g_ni = 0; }

static int vert(float x, float y, Color c)
{
    if (g_nv >= MAX_VERTS) return MAX_VERTS - 1;
    SDL_Vertex *v = &g_v[g_nv];
    v->position.x = x + g_ox;
    v->position.y = y + g_oy;
    c.a *= g_alpha;
    v->color = c;
    v->tex_coord.x = v->tex_coord.y = 0;
    return g_nv++;
}

static void tri(int a, int b, int c)
{
    if (g_ni + 3 > MAX_INDICES) return;
    g_idx[g_ni++] = a;
    g_idx[g_ni++] = b;
    g_idx[g_ni++] = c;
}

static void flush(void)
{
    if (g_ni > 0) SDL_RenderGeometry(g_r, NULL, g_v, g_nv, g_idx, g_ni);
    g_nv = g_ni = 0;
}

static int segments_for(float r)
{
    int n = (int)(r * 0.9f) + 10;
    return n < 12 ? 12 : (n > 72 ? 72 : n);
}

/* ------------------------------------------------------------------------ */
/* Formes pleines                                                           */

void gfx_rect_vgrad(float x, float y, float w, float h, Color top, Color bottom)
{
    begin();
    int a = vert(x, y, top), b = vert(x + w, y, top), c = vert(x + w, y + h, bottom), d = vert(x, y + h, bottom);
    tri(a, b, c);
    tri(a, c, d);
    flush();
}

void gfx_rect_hgrad(float x, float y, float w, float h, Color left, Color right)
{
    begin();
    int a = vert(x, y, left), b = vert(x + w, y, right), c = vert(x + w, y + h, right), d = vert(x, y + h, left);
    tri(a, b, c);
    tri(a, c, d);
    flush();
}

void gfx_rect(float x, float y, float w, float h, Color c) { gfx_rect_vgrad(x, y, w, h, c, c); }

void gfx_rrect_vgrad(float x, float y, float w, float h, float r, Color top, Color bottom)
{
    if (r <= 0.5f) {
        gfx_rect_vgrad(x, y, w, h, top, bottom);
        return;
    }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    begin();
    const int seg = 6;
    float cxs[4] = { x + w - r, x + w - r, x + r, x + r };
    float cys[4] = { y + r, y + h - r, y + h - r, y + r };
    float start[4] = { -PI_F / 2, 0, PI_F / 2, PI_F };
    int center = vert(x + w / 2, y + h / 2, color_lerp(top, bottom, 0.5f));
    int first = -1, prev = -1;
    for (int k = 0; k < 4; k++) {
        for (int i = 0; i <= seg; i++) {
            float a = start[k] + (PI_F / 2) * i / seg;
            float px = cxs[k] + cosf(a) * r, py = cys[k] + sinf(a) * r;
            int v = vert(px, py, color_lerp(top, bottom, (py - y) / h));
            if (prev >= 0) tri(center, prev, v);
            else first = v;
            prev = v;
        }
    }
    tri(center, prev, first);
    flush();
}

void gfx_rrect(float x, float y, float w, float h, float r, Color c) { gfx_rrect_vgrad(x, y, w, h, r, c, c); }

void gfx_ellipse(float x, float y, float rx, float ry, Color c)
{
    begin();
    int n = segments_for(rx > ry ? rx : ry);
    int center = vert(x, y, c);
    int first = vert(x + rx, y, c), prev = first;
    for (int i = 1; i < n; i++) {
        float a = 2 * PI_F * i / n;
        int v = vert(x + cosf(a) * rx, y + sinf(a) * ry, c);
        tri(center, prev, v);
        prev = v;
    }
    tri(center, prev, first);
    flush();
}

void gfx_circle(float x, float y, float r, Color c) { gfx_ellipse(x, y, r, r, c); }

void gfx_arc_fill(float x, float y, float r, float a0, float a1, Color c)
{
    begin();
    int n = segments_for(r);
    int steps = (int)(n * fabsf(a1 - a0) / (2 * PI_F)) + 2;
    int center = vert(x, y, c), prev = -1;
    for (int i = 0; i <= steps; i++) {
        float a = a0 + (a1 - a0) * i / steps;
        int v = vert(x + cosf(a) * r, y + sinf(a) * r, c);
        if (prev >= 0) tri(center, prev, v);
        prev = v;
    }
    flush();
}

void gfx_tri(float x1, float y1, float x2, float y2, float x3, float y3, Color c)
{
    begin();
    tri(vert(x1, y1, c), vert(x2, y2, c), vert(x3, y3, c));
    flush();
}

void gfx_quad(const SDL_FPoint p[4], Color c)
{
    begin();
    int a = vert(p[0].x, p[0].y, c), b = vert(p[1].x, p[1].y, c), d = vert(p[2].x, p[2].y, c),
        e = vert(p[3].x, p[3].y, c);
    tri(a, b, d);
    tri(a, d, e);
    flush();
}

void gfx_quad_rot(float cx, float cy, float w, float h, float angle, Color c)
{
    float ca = cosf(angle), sa = sinf(angle);
    float hw = w / 2, hh = h / 2;
    SDL_FPoint p[4];
    const float sx[4] = { -1, 1, 1, -1 }, sy[4] = { -1, -1, 1, 1 };
    for (int i = 0; i < 4; i++) {
        float lx = sx[i] * hw, ly = sy[i] * hh;
        p[i].x = cx + lx * ca - ly * sa;
        p[i].y = cy + lx * sa + ly * ca;
    }
    gfx_quad(p, c);
}

void gfx_poly(const SDL_FPoint *pts, int n, Color c)
{
    if (n < 3) return;
    begin();
    int first = vert(pts[0].x, pts[0].y, c);
    int prev = vert(pts[1].x, pts[1].y, c);
    for (int i = 2; i < n; i++) {
        int v = vert(pts[i].x, pts[i].y, c);
        tri(first, prev, v);
        prev = v;
    }
    flush();
}

void gfx_ball(float x, float y, float r, float hx, float hy, Color inner, Color mid, Color outer)
{
    /* anneaux concentriques dont le centre glisse du reflet vers le centre */
    const int rings = 6;
    int n = segments_for(r);
    begin();
    int prev_ring = -1;
    for (int k = 0; k <= rings; k++) {
        float t = (float)k / rings;
        float cx = hx + (x - hx) * t, cy = hy + (y - hy) * t, rr = r * t;
        Color c = t < 0.2f ? color_lerp(inner, mid, t / 0.2f) : color_lerp(mid, outer, (t - 0.2f) / 0.8f);
        if (k == 0) {
            prev_ring = vert(cx, cy, c);
            continue;
        }
        int ring = g_nv;
        for (int i = 0; i < n; i++) {
            float a = 2 * PI_F * i / n;
            vert(cx + cosf(a) * rr, cy + sinf(a) * rr, c);
        }
        for (int i = 0; i < n; i++) {
            int i2 = (i + 1) % n;
            if (k == 1) {
                tri(prev_ring, ring + i, ring + i2);
            } else {
                tri(prev_ring + i, ring + i, ring + i2);
                tri(prev_ring + i, ring + i2, prev_ring + i2);
            }
        }
        prev_ring = ring;
    }
    flush();
}

/* ------------------------------------------------------------------------ */
/* Traits                                                                   */

void gfx_line(float x1, float y1, float x2, float y2, float thick, Color c)
{
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.0001f) return;
    float nx = -dy / len * thick / 2, ny = dx / len * thick / 2;
    SDL_FPoint p[4] = { { x1 + nx, y1 + ny }, { x2 + nx, y2 + ny }, { x2 - nx, y2 - ny }, { x1 - nx, y1 - ny } };
    gfx_quad(p, c);
}

void gfx_polyline(const SDL_FPoint *pts, int n, float thick, Color c)
{
    for (int i = 0; i + 1 < n; i++) gfx_line(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, thick, c);
    /* petites jointures rondes */
    if (thick > 2.5f)
        for (int i = 1; i + 1 < n; i++) gfx_circle(pts[i].x, pts[i].y, thick / 2, c);
}

void gfx_ring(float x, float y, float r, float thick, Color c)
{
    begin();
    int n = segments_for(r);
    float r0 = r - thick / 2, r1 = r + thick / 2;
    for (int i = 0; i <= n; i++) {
        float a = 2 * PI_F * i / n;
        vert(x + cosf(a) * r0, y + sinf(a) * r0, c);
        vert(x + cosf(a) * r1, y + sinf(a) * r1, c);
        if (i > 0) {
            int b = i * 2;
            tri(b - 2, b - 1, b + 1);
            tri(b - 2, b + 1, b);
        }
    }
    flush();
}

void gfx_rrect_outline(float x, float y, float w, float h, float r, float thick, Color c)
{
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    SDL_FPoint pts[4 * 7 + 1];
    int n = 0;
    float cxs[4] = { x + w - r, x + w - r, x + r, x + r };
    float cys[4] = { y + r, y + h - r, y + h - r, y + r };
    float start[4] = { -PI_F / 2, 0, PI_F / 2, PI_F };
    for (int k = 0; k < 4; k++)
        for (int i = 0; i <= 6; i++) {
            float a = start[k] + (PI_F / 2) * i / 6;
            pts[n].x = cxs[k] + cosf(a) * r;
            pts[n].y = cys[k] + sinf(a) * r;
            n++;
        }
    pts[n++] = pts[0];
    for (int i = 0; i + 1 < n; i++) gfx_line(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, thick, c);
}

/* ------------------------------------------------------------------------ */
/* Icones                                                                   */

void gfx_heart(float cx, float cy, float s, Color c)
{
    float r = s * 0.28f;
    gfx_circle(cx - r * 0.9f, cy - s * 0.12f, r, c);
    gfx_circle(cx + r * 0.9f, cy - s * 0.12f, r, c);
    gfx_tri(cx - s * 0.5f, cy - s * 0.04f, cx + s * 0.5f, cy - s * 0.04f, cx, cy + s * 0.45f, c);
}

void gfx_snowflake(float cx, float cy, float s, float thick, Color c)
{
    for (int i = 0; i < 6; i++) {
        float a = i * PI_F / 3, ca = cosf(a), sa = sinf(a);
        float ex = cx + ca * s, ey = cy + sa * s;
        gfx_line(cx, cy, ex, ey, thick, c);
        float mx = cx + ca * s * 0.55f, my = cy + sa * s * 0.55f;
        for (int k = -1; k <= 1; k += 2) {
            float b = a + k * 0.7f;
            gfx_line(mx, my, mx + cosf(b) * s * 0.35f, my + sinf(b) * s * 0.35f, thick * 0.8f, c);
        }
    }
}

void gfx_arrow(float cx, float cy, float s, int dir, Color c)
{
    float h = s * 0.5f;
    switch (dir & 3) {
    case 0: gfx_tri(cx, cy - h, cx + h, cy + h * 0.6f, cx - h, cy + h * 0.6f, c); break;
    case 1: gfx_tri(cx + h, cy, cx - h * 0.6f, cy + h, cx - h * 0.6f, cy - h, c); break;
    case 2: gfx_tri(cx, cy + h, cx - h, cy - h * 0.6f, cx + h, cy - h * 0.6f, c); break;
    default: gfx_tri(cx - h, cy, cx + h * 0.6f, cy - h, cx + h * 0.6f, cy + h, c); break;
    }
}

void gfx_shield_icon(float cx, float cy, float s, Color c)
{
    float h = s * 0.5f;
    SDL_FPoint p[5] = { { cx - h, cy - h * 0.8f }, { cx, cy - h }, { cx + h, cy - h * 0.8f },
                        { cx + h * 0.8f, cy + h * 0.2f }, { cx, cy + h } };
    gfx_poly(p, 5, c);
    gfx_tri(cx - h, cy - h * 0.8f, cx, cy + h, cx - h * 0.8f, cy + h * 0.2f, c);
}
