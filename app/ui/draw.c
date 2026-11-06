/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 GRAINE contributors */
/* Drawing in bands: the screen is drawn 80 rows at a time into fm1-base's canvas (gfx.c cv_px, byte-swapped RGB565),
 * every primitive in screen coordinates and clipped to the band. On top of gfx.c: blending, additive glows,
 * anti-aliased rings. Part of the unity build (app.c). */

#define SW 240
#define BAND_H 80
static int32_t BY;                                   /* the band's first screen row */

static void band_begin(int32_t y0)
{
    BY = y0;
    cv_begin(SW, BAND_H, C_BLACK);
}
static void band_end(void) { cv_blit(0, (uint32_t)BY); }

static inline uint16_t sw16(uint32_t c) { return (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8)); }
static inline int in_rows(int32_t y0, int32_t y1) { return y1 > BY && y0 < BY + BAND_H; }

/* a colour, 8 bits a channel, packed */
typedef uint32_t rgb_t;
#define RGB8(r, g, b) ((rgb_t)(((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))
static inline uint16_t to565(rgb_t c) { return RGB((c >> 16) & 255u, (c >> 8) & 255u, c & 255u); }
static inline rgb_t mix8(rgb_t a, rgb_t b, uint32_t t)   /* t 0..256 toward b */
{
    uint32_t r = (((a >> 16) & 255u) * (256u - t) + ((b >> 16) & 255u) * t) >> 8;
    uint32_t g = (((a >> 8) & 255u) * (256u - t) + ((b >> 8) & 255u) * t) >> 8;
    uint32_t bl = ((a & 255u) * (256u - t) + (b & 255u) * t) >> 8;
    return RGB8(r, g, bl);
}
static inline rgb_t scale8(rgb_t a, uint32_t t)          /* t 0..256 */
{
    return RGB8((((a >> 16) & 255u) * t) >> 8, (((a >> 8) & 255u) * t) >> 8, ((a & 255u) * t) >> 8);
}

/* the palette: ink, parchment, and four knob colours that keep their place on every page */
#define K_BG_TOP RGB8(16, 12, 26)
#define K_BG_BOT RGB8(6, 5, 12)
#define K_INK RGB8(10, 8, 18)
#define K_GRID RGB8(30, 26, 44)
#define K_LINE RGB8(52, 46, 72)
#define K_DIM RGB8(96, 90, 118)
#define K_SOFT RGB8(160, 152, 182)
#define K_TEXT RGB8(240, 234, 248)
#define K_WAVE RGB8(122, 112, 150)
#define K_WAVE_HI RGB8(212, 200, 232)
#define K_REC RGB8(255, 70, 80)
#define K_ICE RGB8(150, 220, 255)
static const rgb_t KNOB_RGB[4] = {RGB8(255, 186, 72), RGB8(255, 98, 132), RGB8(70, 226, 206), RGB8(172, 142, 255)};
static const rgb_t SRC_RGB[NMS] = {RGB8(96, 200, 255), RGB8(120, 250, 160), RGB8(255, 120, 220), RGB8(255, 226, 120),
                                   RGB8(255, 150, 80), RGB8(210, 210, 220), RGB8(170, 255, 250)};
static const rgb_t VOICE_RGB[G_MAXV] = {RGB8(255, 206, 96), RGB8(255, 142, 110), RGB8(244, 112, 176),
                                        RGB8(176, 132, 255), RGB8(104, 186, 255), RGB8(104, 238, 196)};

/* ------------------------------------------------------------------------------------------------ pixels --- */
static inline void px_set(int32_t x, int32_t y, rgb_t c)
{
    if ((uint32_t)x < SW && y >= BY && y < BY + BAND_H)
        cv_px[(uint32_t)(y - BY) * SW + (uint32_t)x] = sw16(to565(c));
}
/* a = 0..255 over what is there */
static inline void px_blend(int32_t x, int32_t y, rgb_t c, uint32_t a)
{
    uint16_t *p;
    uint32_t d, r, g, b;
    if ((uint32_t)x >= SW || y < BY || y >= BY + BAND_H || !a)
        return;
    p = &cv_px[(uint32_t)(y - BY) * SW + (uint32_t)x];
    d = sw16(*p);
    r = ((d >> 11) & 31u) * 255u / 31u;
    g = ((d >> 5) & 63u) * 255u / 63u;
    b = (d & 31u) * 255u / 31u;
    r = (r * (255u - a) + ((c >> 16) & 255u) * a) / 255u;
    g = (g * (255u - a) + ((c >> 8) & 255u) * a) / 255u;
    b = (b * (255u - a) + (c & 255u) * a) / 255u;
    *p = sw16(RGB(r, g, b));
}
/* light added: c scaled by a 0..255, saturating */
static inline void px_add(int32_t x, int32_t y, rgb_t c, uint32_t a)
{
    uint16_t *p;
    uint32_t d, r, g, b;
    if ((uint32_t)x >= SW || y < BY || y >= BY + BAND_H || !a)
        return;
    p = &cv_px[(uint32_t)(y - BY) * SW + (uint32_t)x];
    d = sw16(*p);
    r = ((d >> 11) & 31u) * 255u / 31u + (((c >> 16) & 255u) * a >> 8);
    g = ((d >> 5) & 63u) * 255u / 63u + (((c >> 8) & 255u) * a >> 8);
    b = (d & 31u) * 255u / 31u + ((c & 255u) * a >> 8);
    *p = sw16(RGB(r > 255u ? 255u : r, g > 255u ? 255u : g, b > 255u ? 255u : b));
}

/* ------------------------------------------------------------------------------------------------ shapes --- */
static void d_rect(int32_t x, int32_t y, int32_t w, int32_t h, rgb_t c)
{
    int32_t x1 = x + w, y1 = y + h, i, j;
    uint16_t v = sw16(to565(c));
    if (x < 0)
        x = 0;
    if (x1 > SW)
        x1 = SW;
    if (y < BY)
        y = BY;
    if (y1 > BY + BAND_H)
        y1 = BY + BAND_H;
    for (j = y; j < y1; j++) {
        uint16_t *row = &cv_px[(uint32_t)(j - BY) * SW];
        for (i = x; i < x1; i++)
            row[i] = v;
    }
}
static void d_rect_a(int32_t x, int32_t y, int32_t w, int32_t h, rgb_t c, uint32_t a)
{
    int32_t i, j, y0 = y < BY ? BY : y, y1 = y + h > BY + BAND_H ? BY + BAND_H : y + h;
    for (j = y0; j < y1; j++)
        for (i = x; i < x + w; i++)
            px_blend(i, j, c, a);
}
static void d_hline(int32_t x, int32_t y, int32_t w, rgb_t c, uint32_t a)
{
    int32_t i;
    if (y < BY || y >= BY + BAND_H)
        return;
    for (i = x; i < x + w; i++)
        px_blend(i, y, c, a);
}
static void d_vline(int32_t x, int32_t y, int32_t h, rgb_t c, uint32_t a)
{
    int32_t j, y0 = y < BY ? BY : y, y1 = y + h > BY + BAND_H ? BY + BAND_H : y + h;
    for (j = y0; j < y1; j++)
        px_blend(x, j, c, a);
}
/* a rounded box, filled, its corners anti-aliased */
static void d_rbox(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, rgb_t c, uint32_t a)
{
    int32_t i, j, y0 = y < BY ? BY : y, y1 = y + h > BY + BAND_H ? BY + BAND_H : y + h;
    for (j = y0; j < y1; j++)
        for (i = x; i < x + w; i++) {
            int32_t dx = i < x + r ? x + r - i : i >= x + w - r ? i - (x + w - r - 1) : 0;
            int32_t dy = j < y + r ? y + r - j : j >= y + h - r ? j - (y + h - r - 1) : 0;
            uint32_t aa = a;
            if (dx && dy) {
                float d = fm_sqrtf((float)(dx * dx + dy * dy)) - (float)r + 0.5f;
                if (d >= 1.0f)
                    continue;
                if (d > 0.0f)
                    aa = (uint32_t)((float)a * (1.0f - d));
            }
            px_blend(i, j, c, aa);
        }
}
static void d_rframe(int32_t x, int32_t y, int32_t w, int32_t h, rgb_t c, uint32_t a)
{
    d_hline(x + 2, y, w - 4, c, a);
    d_hline(x + 2, y + h - 1, w - 4, c, a);
    d_vline(x, y + 2, h - 4, c, a);
    d_vline(x + w - 1, y + 2, h - 4, c, a);
    px_blend(x + 1, y + 1, c, a);
    px_blend(x + w - 2, y + 1, c, a);
    px_blend(x + 1, y + h - 2, c, a);
    px_blend(x + w - 2, y + h - 2, c, a);
}
/* a line, anti-aliased (Wu) */
static void d_line(float x0, float y0, float x1, float y1, rgb_t c, uint32_t a)
{
    float dx = x1 - x0, dy = y1 - y0, adx = fm_fabsf(dx), ady = fm_fabsf(dy), t, n;
    int32_t i, steps;
    if (adx < 0.01f && ady < 0.01f)
        return;
    if (adx >= ady) {
        if (x1 < x0) {
            t = x0, x0 = x1, x1 = t;
            t = y0, y0 = y1, y1 = t;
        }
        n = (y1 - y0) / (x1 - x0);
        steps = (int32_t)(x1 - x0 + 0.5f);
        for (i = 0; i <= steps; i++) {
            float y = y0 + n * (float)i, f = y - fm_floorf(y);
            int32_t yi = (int32_t)fm_floorf(y), xi = (int32_t)(x0 + 0.5f) + i;
            px_blend(xi, yi, c, (uint32_t)((1.0f - f) * (float)a));
            px_blend(xi, yi + 1, c, (uint32_t)(f * (float)a));
        }
    } else {
        if (y1 < y0) {
            t = x0, x0 = x1, x1 = t;
            t = y0, y0 = y1, y1 = t;
        }
        n = (x1 - x0) / (y1 - y0);
        steps = (int32_t)(y1 - y0 + 0.5f);
        for (i = 0; i <= steps; i++) {
            float x = x0 + n * (float)i, f = x - fm_floorf(x);
            int32_t xi = (int32_t)fm_floorf(x), yi = (int32_t)(y0 + 0.5f) + i;
            px_blend(xi, yi, c, (uint32_t)((1.0f - f) * (float)a));
            px_blend(xi + 1, yi, c, (uint32_t)(f * (float)a));
        }
    }
}
/* a glowing seed: a bright core of radius r and a halo of radius 2.6 r, light added */
static void d_glow(float cx, float cy, float r, rgb_t c, uint32_t a)
{
    float R = r * 2.6f + 1.0f, inv = 1.0f / (r * r), invh = 1.0f / (R * R);
    int32_t x0 = (int32_t)(cx - R), x1 = (int32_t)(cx + R + 1.0f), y0 = (int32_t)(cy - R), y1 = (int32_t)(cy + R + 1.0f);
    int32_t i, j;
    if (!in_rows(y0, y1 + 1))
        return;
    for (j = y0; j <= y1; j++) {
        float dy = (float)j + 0.5f - cy;
        if (j < BY || j >= BY + BAND_H)
            continue;
        for (i = x0; i <= x1; i++) {
            float dx = (float)i + 0.5f - cx, d2 = dx * dx + dy * dy, k = 0.0f, h;
            if (d2 < r * r) {
                float q = 1.0f - d2 * inv;
                k = q * q;
            }
            h = 1.0f - d2 * invh;
            if (h > 0.0f)
                k += 0.28f * h * h;
            if (k > 0.004f)
                px_add(i, j, c, (uint32_t)(k * (float)a));
        }
    }
}
/* a disc, anti-aliased, blended */
static void d_disc(float cx, float cy, float r, rgb_t c, uint32_t a)
{
    int32_t x0 = (int32_t)(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f), y0 = (int32_t)(cy - r - 1.0f),
            y1 = (int32_t)(cy + r + 1.0f), i, j;
    if (!in_rows(y0, y1 + 1))
        return;
    for (j = y0; j <= y1; j++)
        for (i = x0; i <= x1; i++) {
            float dx = (float)i + 0.5f - cx, dy = (float)j + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy) - r;
            if (d < 0.5f)
                px_blend(i, j, c, d < -0.5f ? a : (uint32_t)((0.5f - d) * (float)a));
        }
}

static inline float fatan2(float y, float x)              /* -pi .. pi, within 0.01 rad */
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax, a, s, r;
    if (mx < 1e-9f)
        return 0.0f;
    a = mn / mx;
    s = a * a;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = 3.14159274f - r;
    if (y < 0.0f)
        r = -r;
    return r;
}

/* a ring of radius ro..ri, the arc t0..t1 of the knob's 270 degrees (t 0: down-left, 1: down-right), AA edges */
#define ARC_SPAN 4.71238898f                               /* 270 degrees */
static void d_arc(float cx, float cy, float ri, float ro, float t0, float t1, rgb_t c, uint32_t a)
{
    int32_t x0 = (int32_t)(cx - ro - 1.0f), x1 = (int32_t)(cx + ro + 1.0f), y0 = (int32_t)(cy - ro - 1.0f),
            y1 = (int32_t)(cy + ro + 1.0f), i, j;
    float lo = t0 < t1 ? t0 : t1, hi = t0 < t1 ? t1 : t0, rm = 0.5f * (ri + ro), hw = 0.5f * (ro - ri);
    float edge = 1.0f / (rm * ARC_SPAN);                  /* a pixel, in t */
    if (!in_rows(y0, y1 + 1) || hi - lo < 0.0005f)
        return;
    for (j = y0; j <= y1; j++) {
        if (j < BY || j >= BY + BAND_H)
            continue;
        for (i = x0; i <= x1; i++) {
            float dx = (float)i + 0.5f - cx, dy = (float)j + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            float cov = hw + 0.5f - fm_fabsf(d - rm), th, t, ce;
            if (cov <= 0.0f)
                continue;
            if (cov > 1.0f)
                cov = 1.0f;
            th = fatan2(dx, -dy);                          /* clockwise from up */
            t = (th + 2.35619449f) / ARC_SPAN;
            if (t < lo - edge || t > hi + edge)
                continue;
            ce = 1.0f;
            if (t < lo)
                ce = 1.0f - (lo - t) / edge;
            else if (t > hi)
                ce = 1.0f - (t - hi) / edge;
            px_blend(i, j, c, (uint32_t)(cov * ce * (float)a));
        }
    }
}

/* ------------------------------------------------------------------------------------------------- text --- */
static int32_t d_text(int32_t x, int32_t y, const felucca_font_t *f, const char *s, rgb_t c)
{
    if (!in_rows(y, y + f->h))
        return x + text_w(f, s);
    return cv_text(x, y - BY, f, s, to565(c));
}
static int32_t d_text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, rgb_t c)
{
    return d_text(cx - text_w(f, s) / 2, y, f, s, c);
}
static int32_t d_text_r(int32_t xr, int32_t y, const felucca_font_t *f, const char *s, rgb_t c)   /* -> its left x */
{
    int32_t x = xr - text_w(f, s);
    d_text(x, y, f, s, c);
    return x;
}
/* the largest of the faces given that fits w */
static const felucca_font_t *fit(const char *s, int32_t w, const felucca_font_t *a, const felucca_font_t *b,
                                 const felucca_font_t *c)
{
    if (text_w(a, s) <= w)
        return a;
    if (!c || text_w(b, s) <= w)
        return b;
    return c;
}
