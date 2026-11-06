/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 GRAINE contributors */
/* The page's picture: over the cloud, half see-through, what the page's four knobs shape — the envelope's curve,
 * the LFO's wave, the filter's response, the echoes, the space's tail, the grain's window, the matter's steps, the
 * arpeggio, the scan's path, the scale, the clock. Each part takes the colour of the knob that moves it, and follows
 * the parameters as modulated now. Included by screen.c (its geometry), in the unity build. */

#define VX0 (WX + 6)
#define VX1 (WX + WW - 6)
#define VW (VX1 - VX0)
#define VY0 (WY0 + 8)
#define VY1 (WY1 - 6)
#define VH (VY1 - VY0)

enum { VZ_NONE, VZ_GRAIN, VZ_SCAN, VZ_AENV, VZ_MENV, VZ_LFO1, VZ_LFO2, VZ_FILT, VZ_ECHO, VZ_SPACE, VZ_MATTER,
       VZ_ARP, VZ_SCALE, VZ_CLOCK, VZ_SCENES, VZ_DRONE, VZ_CHAOS, VZ_HALO, VZ_CHOPPER, VZ_RAIN };

static int viz_kind(void)                        /* the page, by its first knob */
{
    switch (PAGES[cur_page()].p[0]) {
    case P_SCAN: return VZ_SCAN;
    case P_WIN: return VZ_GRAIN;
    case P_AATK: return VZ_AENV;
    case P_MATK: return VZ_MENV;
    case P_L1RATE: return VZ_LFO1;
    case P_L2RATE: return VZ_LFO2;
    case P_FTYPE: return VZ_FILT;
    case P_DTIME: return VZ_ECHO;
    case P_RSIZE: return VZ_SPACE;
    case P_BITS: return VZ_MATTER;
    case P_ARP: return VZ_ARP;
    case P_VOICES: return VZ_SCALE;
    case P_TEMPO: return VZ_CLOCK;
    case P_MORPH: return VZ_SCENES;
    case P_STRETCH: return VZ_DRONE;
    case P_CHAOS: return VZ_CHAOS;
    case P_SHIM: return VZ_HALO;
    case P_GATE: return VZ_CHOPPER;
    case P_RAIN: return VZ_RAIN;
    default: return VZ_NONE;
    }
}
/* the pictures that move on their own (an LFO runs, the clock ticks, the morph sweeps, the rain falls) */
static int viz_live(void)
{
    int k = viz_kind();
    return k == VZ_LFO1 || k == VZ_LFO2 || (k == VZ_CLOCK && GP.running) || (k == VZ_SCENES && G.v[P_AUTOM]) ||
           (k == VZ_CHAOS && G.v[P_CHOP]) || (k == VZ_CHOPPER && G.v[P_GATE]) || (k == VZ_RAIN && (G.v[P_RAIN] || GV.rain_n));
}

static float vz_n(int p)                       /* the parameter as playing now, 0..1 */
{
    float m = GV.pmod[p];
    return g_clamp01(m >= 0.0f ? m : pnorm(p));
}
static uint32_t vz_hash(uint32_t i)            /* a fixed random, for the pictures of random things */
{
    i ^= i >> 16;
    i *= 0x7feb352du;
    i ^= i >> 15;
    i *= 0x846ca68bu;
    return i ^ (i >> 16);
}
static float vz_rnd(uint32_t i) { return (float)(vz_hash(i) >> 8) * (1.0f / 16777216.0f); }

/* a curve from x0 to x1 (ys at each column), a soft fill under it down to the base and a 2-pixel line */
static void vz_seg(float x0, float y0, float x1, float y1, rgb_t c, uint32_t a)
{
    d_line(x0, y0, x1, y1, c, a);
    d_line(x0, y0 + 1.0f, x1, y1 + 1.0f, c, a * 2u / 3u);
}
static void vz_fill(int32_t x, float y, rgb_t c, uint32_t a)
{
    int32_t t = (int32_t)y;
    if (t < VY1)
        d_vline(x, t, VY1 - t, c, a);
}

static void vz_panel(const char *title, rgb_t tc)
{
    d_rbox(WX, WY0, WW, WY1 - WY0, 8, K_INK, 190);
    d_hline(VX0, VY1, VW, K_LINE, 200);
    (void)title;                                                /* the header names the page already */
    (void)tc;
}

/* ---------------------------------------------------------------------------------------- the envelopes --- */
static void vz_env(int pa, int pd, int ps, int pr, const char *title)
{
    float na = vz_n(pa), nd = vz_n(pd), su = vz_n(ps), nr = vz_n(pr);
    float wa = 6.0f + 52.0f * na, wd = 6.0f + 52.0f * nd, wr = 6.0f + 52.0f * nr, ws, x, sc;
    float top = (float)VY0 + 4.0f, bot = (float)VY1, h = bot - top, ysu = bot - h * su;
    float xa, xd, xs, xr, px, py;
    int32_t c;
    sc = (float)VW / (wa + wd + wr + 40.0f);
    if (sc > 1.0f)
        sc = 1.0f;
    wa *= sc;
    wd *= sc;
    wr *= sc;
    ws = (float)VW - wa - wd - wr;
    xa = (float)VX0 + wa;
    xd = xa + wd;
    xs = xd + ws;
    xr = xs + wr;
    vz_panel(title, K_SOFT);
    d_vline((int32_t)xs, VY0, VH, K_GRID, 255);                 /* the key let go */
    px = (float)VX0;
    py = bot;
    for (c = VX0; c <= (int32_t)xr; c++) {
        float t, y;
        int k;
        x = (float)c;
        if (x <= xa) {
            t = (x - (float)VX0) / fm_maxf(wa, 1.0f);
            y = bot - h * (1.0f - (1.0f - t) * (1.0f - t));       /* the attack: quick, then easing */
            k = 0;
        } else if (x <= xd) {
            t = (x - xa) / fm_maxf(wd, 1.0f);
            y = bot - h * (su + (1.0f - su) * fm_expf(-4.5f * t) * (1.0f - t));
            k = 1;
        } else if (x <= xs) {
            y = ysu;
            k = 2;
        } else {
            t = (x - xs) / fm_maxf(wr, 1.0f);
            y = bot - h * su * fm_expf(-4.5f * t) * (1.0f - t);
            k = 3;
        }
        vz_fill(c, y + 2.0f, KNOB_RGB[k], 46);
        if (c > VX0)
            vz_seg(px, py, x, y, KNOB_RGB[k], 255);
        px = x;
        py = y;
    }
    d_glow(xa, top, 3.0f, KNOB_RGB[0], 220);
    d_glow(xd, ysu, 3.0f, KNOB_RGB[1], 220);
    d_glow(xs, ysu, 3.0f, KNOB_RGB[2], 220);
    d_glow(xr, bot, 3.0f, KNOB_RGB[3], 220);
}

/* ---------------------------------------------------------------------------------------------- the LFOs --- */
static float vz_lfo_at(int shape, float ph, uint32_t cyc)
{
    switch (shape) {
    case 0: return fm_sin_turns(ph);
    case 1: return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
    case 2: return 2.0f * ph - 1.0f;
    case 3: return 1.0f - 2.0f * ph;
    case 4: return ph < 0.5f ? 1.0f : -1.0f;
    case 5: return vz_rnd(cyc) * 2.0f - 1.0f;
    default: {
        float s = 0.5f - 0.5f * fm_sin_turns(ph + 0.25f);
        float a = vz_rnd(cyc) * 2.0f - 1.0f, b = vz_rnd(cyc + 1u) * 2.0f - 1.0f;
        return a + (b - a) * s;
    }
    }
}
static void vz_lfo(int i)
{
    int pr = i ? P_L2RATE : P_L1RATE, psh = i ? P_L2SHAPE : P_L1SHAPE, psy = i ? P_L2SYNC : P_L1SYNC;
    int shape = clamp(G.v[psh], 0, 6);
    float cycles = 1.0f + 3.0f * vz_n(pr), mid = (float)(VY0 + VY1) * 0.5f, amp = (float)VH * 0.42f;
    float px = 0.0f, py = 0.0f, now = GV.lfo[i];
    rgb_t c = KNOB_RGB[G.v[psy] ? 2 : 0];
    int32_t x;
    vz_panel(i ? "LFO 2" : "LFO 1", K_SOFT);
    d_hline(VX0, (int32_t)mid, VW, K_GRID, 255);
    for (x = VX0; x <= VX1; x++) {
        float t = (float)(x - VX0) / (float)VW * cycles, ph = t - fm_floorf(t);
        float y = mid - amp * vz_lfo_at(shape, ph, (uint32_t)t + 7u * (uint32_t)i);
        rgb_t col = c;
        if (x > VX0 && !(shape >= 4 && shape <= 5 && fm_fabsf(y - py) > 4.0f))
            vz_seg(px, py, (float)x, y, col, 230);
        else if (x > VX0)
            d_vline(x, (int32_t)fm_minf(y, py), (int32_t)fm_fabsf(y - py) + 1, col, 160);
        px = (float)x;
        py = y;
    }
    /* where it is now: a light on the right edge, and a line across */
    d_hline(VX0, (int32_t)(mid - amp * now), VW, KNOB_RGB[3], 70);
    d_glow((float)VX1 - 2.0f, mid - amp * now, 4.5f, KNOB_RGB[3], 255);
}

/* -------------------------------------------------------------------------------------------- the filter --- */
static void vz_filter(void)
{
    float ft = vz_n(P_FTYPE), cut = pval(P_CUT, vz_n(P_CUT)), res = vz_n(P_RES), drive = vz_n(P_DRIVE);
    float k = 2.0f - 1.96f * res, mlp, mbp, mhp, px = 0.0f, py = 0.0f, fc;
    float lo = fm_log2f(30.0f), span = fm_log2f(18000.0f) - lo;
    rgb_t c = mix8(KNOB_RGB[1], KNOB_RGB[3], (uint32_t)(drive * 200.0f));
    int32_t x;
    if (GP.pads & 1u << PAD_SOMBRE)
        cut *= 0.12f;
    if (ft < 0.5f) {
        mlp = 1.0f - 2.0f * ft;
        mbp = 2.0f * ft;
        mhp = 0.0f;
    } else {
        mlp = 0.0f;
        mbp = 2.0f - 2.0f * ft;
        mhp = 2.0f * ft - 1.0f;
    }
    vz_panel(TR("FILTRE", "FILTER"), K_SOFT);
    for (x = 0; x < 6; x++) {                                   /* 30 100 300 1k 3k 10k */
        static const float F[6] = {30.0f, 100.0f, 300.0f, 1000.0f, 3000.0f, 10000.0f};
        d_vline(VX0 + (int32_t)((fm_log2f(F[x]) - lo) / span * (float)VW), VY0, VH, K_GRID, 200);
    }
    fc = (fm_log2f(fm_maxf(cut, 30.0f)) - lo) / span;
    d_vline(VX0 + (int32_t)(fc * (float)VW), VY0, VH, KNOB_RGB[1], 110);
    for (x = VX0; x <= VX1; x++) {
        float f = fm_exp2f(lo + span * (float)(x - VX0) / (float)VW), r = f / cut, r2 = r * r;
        float nr = mlp - mhp * r2, ni = mbp * r, dr = 1.0f - r2, di = k * r;
        float g2 = (nr * nr + ni * ni) / fm_maxf(dr * dr + di * di, 1e-9f);
        float db = 3.0103f * fm_log2f(fm_maxf(g2, 1e-9f)) * (1.0f + 0.25f * drive);   /* 10 log10 */
        float y = (float)VY0 + 14.0f - db * ((float)VH - 14.0f) / 42.0f;
        y = fm_clampf(y, (float)VY0, (float)VY1);
        vz_fill(x, y + 2.0f, c, 40);
        if (x > VX0)
            vz_seg(px, py, (float)x, y, c, 255);
        px = (float)x;
        py = y;
    }
    d_hline(VX0, VY0 + 14, VW, K_LINE, 120);                    /* 0 dB */
}

/* ----------------------------------------------------------------------------------------------- the echo --- */
static void vz_echo(void)
{
    float fb = pval(P_DFB, vz_n(P_DFB)), tone = vz_n(P_DTONE), mix = vz_n(P_DMIX), lev = 0.35f + 0.65f * mix, g = 1.0f;
    float step = 8.0f + 38.0f * (float)clamp(G.v[P_DTIME], 0, 12) / 12.0f, x = (float)VX0 + 4.0f;
    rgb_t ec = mix8(KNOB_RGB[3], KNOB_RGB[2], (uint32_t)(tone * 256.0f));
    int n = 0;
    vz_panel(TR("ECHO", "ECHO"), K_SOFT);
    d_rect_a((int32_t)x - 2, VY0 + 2, 5, VH - 2, K_SOFT, 200);  /* the sound */
    for (x += step; x < (float)VX1 && n < 40; x += step, n++) {
        float h, y;
        uint32_t a;
        if (n)
            g *= fb;
        h = fm_minf(g * lev, 1.15f) * (float)(VH - 4);
        if (h < 1.0f)
            break;
        y = (float)VY1 - h;
        a = 140u + (uint32_t)(fm_minf(g, 1.0f) * 110.0f);
        d_rect_a((int32_t)x - 2, (int32_t)y, 4, (int32_t)h, fb > 1.0f ? mix8(ec, K_REC, 160) : ec, a);
        d_glow(x, y, 2.5f, KNOB_RGB[0], a);
        /* the tone: each repeat a little darker when it is low */
        ec = mix8(ec, K_INK, (uint32_t)((1.0f - tone) * 40.0f));
    }
}

/* ---------------------------------------------------------------------------------------------- the space --- */
static void vz_space(void)
{
    float size = vz_n(P_RSIZE), rt = pval(P_RDEC, vz_n(P_RDEC)), damp = vz_n(P_RDAMP), mix = vz_n(P_RMIX);
    float span = 2.5f + 6.0f * size, pre = 3.0f + 22.0f * size, lev = 0.3f + 0.7f * mix;
    float secs = 12.0f;                                         /* the picture's width: 12 s, in a log time */
    int32_t x;
    vz_panel(TR("ESPACE", "SPACE"), K_SOFT);
    d_rect_a(VX0, VY0 + 2, 3, VH - 2, K_SOFT, 200);
    for (x = VX0 + (int32_t)pre; x <= VX1; x++) {
        float u = (float)(x - VX0 - (int32_t)pre) / (float)VW, t = secs * u * u;
        float env = fm_exp2f(-9.97f * t / fm_maxf(rt, 0.05f)) * lev;   /* 10^(-3 t / RT60) */
        float jag = 0.55f + 0.45f * vz_rnd((uint32_t)x * 13u);
        float h = env * jag * (float)(VH - 4);
        rgb_t c = mix8(KNOB_RGB[2], KNOB_RGB[0], (uint32_t)(fm_minf(u * damp * 2.2f, 1.0f) * 230.0f));
        if (h < 0.6f)
            break;
        if (u * (float)VW < span * 4.0f && (x % (int32_t)span))   /* the first reflections, apart */
            continue;
        d_vline(x, VY1 - (int32_t)h, (int32_t)h, c, 120u + (uint32_t)(env * 120.0f));
    }
}

/* -------------------------------------------------------------------------------------------- the grain --- */
static float vz_base(int k, float t)
{
    float x = t * 2.0f - 1.0f, e = fm_fabsf(x);
    switch (k) {
    case 0: return fm_expf(-x * x * 9.0f);
    case 1: return 0.5f - 0.5f * fm_sin_turns(t + 0.25f);
    case 2: return e < 0.5f ? 1.0f : 0.5f + 0.5f * fm_sin_turns((e - 0.5f) + 0.25f);
    default: return e < 0.94f ? 1.0f : 0.5f + 0.5f * fm_sin_turns((e - 0.94f) / 0.12f + 0.25f);
    }
}
static void vz_grain(void)
{
    float m = vz_n(P_WIN) * 3.0f, pk = 0.5f + 0.47f * pval(P_SKEW, vz_n(P_SKEW)), a, px = 0.0f, py = 0.0f;
    int k = m >= 3.0f ? 2 : (int)m, tm = clamp(G.v[P_TIMING], 0, 2), i;
    int32_t x, gx0 = VX0 + 20, gw = VW - 40;
    a = m - (float)k;
    vz_panel(TR("FORME DU GRAIN", "GRAIN SHAPE"), K_SOFT);
    d_vline(gx0 + (int32_t)(pk * (float)gw), VY0, VH, KNOB_RGB[1], 90);
    for (x = gx0; x <= gx0 + gw; x++) {
        float t = (float)(x - gx0) / (float)gw, u = t < pk ? 0.5f * t / pk : 0.5f + 0.5f * (t - pk) / (1.0f - pk);
        float w = vz_base(k, u) + (vz_base(k + 1, u) - vz_base(k, u)) * a, y;
        if (x == gx0 || x == gx0 + gw)
            w = 0.0f;
        y = (float)VY1 - w * (float)(VH - 10);
        vz_fill(x, y + 2.0f, KNOB_RGB[0], 50);
        if (x > gx0)
            vz_seg(px, py, (float)x, y, KNOB_RGB[0], 255);
        px = (float)x;
        py = y;
    }
    for (i = 0; i < 14; i++) {                                  /* the timing: even, random, on the clock */
        float t = tm == 1 ? vz_rnd((uint32_t)i + 99u) : ((float)i + 0.5f) / 14.0f;
        if (tm == 2 && (i & 1))
            continue;
        d_glow((float)VX0 + t * (float)VW, (float)VY1 + 3.0f, 1.6f, KNOB_RGB[3], 200);
    }
}

/* ---------------------------------------------------------------------------------------------- the scan --- */
static void vz_scan(void)
{
    int mode = clamp(G.v[P_SCAN], 0, 4);
    float sp = pval(P_SPEED, vz_n(P_SPEED)), pos = vz_n(P_POS), y, px = 0.0f, py = 0.0f;
    float per = fm_fabsf(sp) < 0.02f ? 1e9f : 1.6f / fm_fabsf(sp);   /* the picture spans 1.6 s-ish of reading */
    int32_t x;
    vz_panel(TR("BALAYAGE", "SCAN"), K_SOFT);
    for (x = VX0; x <= VX1; x++) {
        float t = (float)(x - VX0) / (float)VW, c = t / per, p;
        switch (mode) {
        case 0: p = pos + (sp < 0.0f ? -c : c); p -= fm_floorf(p); break;
        case 1: p = pos + c; p -= 2.0f * fm_floorf(p * 0.5f); if (p > 1.0f) p = 2.0f - p; break;
        case 2: p = g_clamp01(pos + (sp < 0.0f ? -c : c)); break;
        case 3: p = pos + fm_fabsf(sp) * 0.4f * (fm_sin_turns(t * 1.7f) * 0.6f + fm_sin_turns(t * 4.3f + 0.2f) * 0.3f);
            p -= fm_floorf(p); break;
        default: p = vz_rnd((uint32_t)(t * fm_maxf(fm_fabsf(sp) * 3.0f, 0.6f) * 4.0f) + 5u); break;
        }
        y = (float)VY1 - p * (float)(VH - 2);
        if (x > VX0 && fm_fabsf(y - py) < (float)VH * 0.5f)
            vz_seg(px, py, (float)x, y, KNOB_RGB[1], 240);
        px = (float)x;
        py = y;
    }
    d_hline(VX0, VY1 - (int32_t)(pos * (float)(VH - 2)), VW, KNOB_RGB[0], 60);
}

/* --------------------------------------------------------------------------------------------- the matter --- */
static void vz_matter(void)
{
    float bits = vz_n(P_BITS), down = pval(P_DOWN, vz_n(P_DOWN)), width = pval(P_WIDTH, vz_n(P_WIDTH));
    float lev = 0.25f + 0.75f * vz_n(P_LEVEL), mid = (float)(VY0 + VY1) * 0.5f, amp = (float)VH * 0.45f * lev;
    float q = bits > 0.005f ? fm_exp2f(15.0f - 13.5f * bits) / 32768.0f : 0.0f, hold = 0.0f;
    float off = 3.0f * fm_minf(width, 2.0f), cnt = 1e9f;
    int32_t x;
    vz_panel(TR("MATIERE", "MATTER"), K_SOFT);
    d_hline(VX0, (int32_t)mid, VW, K_GRID, 255);
    for (x = VX0; x <= VX1; x++) {
        float v;
        cnt += 1.0f;
        if (cnt >= down * 0.6f) {                                /* the sample-and-hold, at the picture's scale */
            float t = (float)(x - VX0) / (float)VW;
            cnt = 0.0f;
            hold = fm_sin_turns(t * 2.0f);
            if (q > 0.0f)
                hold = (float)(int32_t)(hold / q + (hold < 0.0f ? -0.5f : 0.5f)) * q;
        }
        v = hold;
        if (off > 0.4f) {                                      /* the width: left and right apart */
            d_glow((float)x, mid - amp * v - off, 0.8f, KNOB_RGB[2], 90);
            d_glow((float)x, mid - amp * v + off, 0.8f, KNOB_RGB[3], 90);
        }
        d_vline(x, (int32_t)(mid - amp * v), 2, mix8(KNOB_RGB[0], KNOB_RGB[1], (uint32_t)(fm_minf(down / 8.0f, 1.0f) * 256.0f)), 240);
    }
}

/* ------------------------------------------------------------------------------------------- the arpeggio --- */
static void vz_arp(void)
{
    int mode = clamp(G.v[P_ARP], 0, 5), octs = clamp(G.v[P_ARPOCT], 1, 4), n = 3 * octs, steps, i;
    int div = clamp(G.v[P_ARPDIV], 0, 5);
    float gate = (float)clamp(G.v[P_ARPGATE], 5, 100) / 100.0f, cw;
    static const uint8_t PER_BEAT[6] = {1, 2, 3, 4, 6, 8};
    vz_panel(TR("ARPEGE", "ARP"), K_SOFT);
    if (!mode) {
        d_text_c(SW / 2, (VY0 + VY1) / 2 - 6, &FONT_XS, TR("ARPEGE OFF", "ARP OFF"), K_DIM);
        return;
    }
    steps = PER_BEAT[div] * 4;                                  /* one bar */
    if (steps > 24)
        steps = 24;
    cw = (float)VW / (float)steps;
    for (i = 0; i < steps; i++) {
        int s, cyc = mode == 3 ? 2 * n - 2 : n;
        float y;
        switch (mode) {
        case 1: s = i % n; break;
        case 2: s = n - 1 - i % n; break;
        case 3: s = i % (cyc < 1 ? 1 : cyc); if (s >= n) s = cyc - s; break;
        case 4: s = (int)(vz_rnd((uint32_t)i + 31u) * (float)n); break;
        default: s = i % n; s = (s * 2) % n; break;
        }
        y = (float)VY1 - 4.0f - (float)s * (float)(VH - 10) / (float)(n > 1 ? n - 1 : 1);
        d_rbox(VX0 + (int32_t)((float)i * cw) + 1, (int32_t)y - 2, (int32_t)fm_maxf(cw * gate - 1.0f, 2.0f), 5, 2,
               KNOB_RGB[s / 3 % 4], 230);
    }
    for (i = 1; i < octs; i++)                                  /* the octaves */
        d_hline(VX0, VY1 - 4 - (int32_t)((float)(3 * i) * (float)(VH - 10) / (float)(n - 1)) + 3, VW, K_GRID, 255);
}

/* ---------------------------------------------------------------------------------------------- the scale --- */
static void vz_scale(void)
{
    static const uint8_t WHITE_OF[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    static const uint8_t IS_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint16_t mask = G_SCALE_MASK[clamp(G.v[P_SCALE], 0, G_NSCALE - 1)];
    int tonic = clamp(G.v[P_TONIC], 0, 11), oct, n, voices = clamp(G.v[P_VOICES], 1, G_MAXV);
    int32_t kw = VW / 14, ky = VY0 + 10, kh = VH - 20;
    vz_panel(TR("GAMME", "SCALE"), K_SOFT);
    for (oct = 0; oct < 2; oct++)
        for (n = 0; n < 12; n++) {
            int deg = (n - tonic + 12) % 12, in = mask >> deg & 1;
            int32_t x = VX0 + (WHITE_OF[n] + 7 * oct) * kw;
            if (IS_BLACK[n])
                continue;
            d_rbox(x + 1, ky, kw - 2, kh, 3, in ? (deg ? KNOB_RGB[2] : KNOB_RGB[0]) : K_LINE, in ? 210 : 255);
        }
    for (oct = 0; oct < 2; oct++)
        for (n = 0; n < 12; n++) {
            int deg = (n - tonic + 12) % 12, in = mask >> deg & 1;
            int32_t x = VX0 + (WHITE_OF[n] + 7 * oct) * kw + kw * 2 / 3;
            if (!IS_BLACK[n])
                continue;
            d_rbox(x, ky, kw * 2 / 3, kh * 3 / 5, 3, K_INK, 255);
            d_rbox(x + 1, ky, kw * 2 / 3 - 2, kh * 3 / 5 - 1, 3, in ? (deg ? KNOB_RGB[2] : KNOB_RGB[0]) : K_GRID, 255);
        }
    for (n = 0; n < G_MAXV; n++)                                /* the voices */
        d_glow((float)(VX1 - 6 - 9 * (G_MAXV - 1 - n)), (float)WY0 + 5.0f, 2.4f, n < voices ? KNOB_RGB[1] : K_LINE,
               n < voices ? 230 : 160);
}

/* ---------------------------------------------------------------------------------------------- the clock --- */
static void vz_clock(void)
{
    float beat = (float)GV.beat_q16 / 65536.0f, ph = beat - fm_floorf(beat);
    int b = (int)beat % 4, i, len = 1 << clamp(G.v[P_GESTLEN], 0, 3);
    int32_t cx;
    char s[12];
    vz_panel(TR("HORLOGE", "CLOCK"), K_SOFT);
    s_int((int32_t)(GV.tempo + 0.5f), s, 0);
    d_text_c(SW / 2, VY0 + 2, &FONT_B, s, GP.running ? K_TEXT : K_DIM);
    for (i = 0; i < 4; i++) {
        float lit = GP.running && i == b ? 1.0f - ph : 0.0f;
        cx = VX0 + VW / 8 + i * VW / 4;
        d_glow((float)cx, (float)VY1 - 22.0f, 5.0f + 3.0f * lit, i ? KNOB_RGB[0] : KNOB_RGB[1],
               90u + (uint32_t)(lit * 165.0f));
    }
    for (i = 0; i < len * 4 && i < 32; i++) {                   /* the gesture loop's beats */
        int on = GP.running && ((int)beat % (len * 4)) == i;
        int32_t w = VW / (len * 4 < 32 ? len * 4 : 32);
        d_rect_a(VX0 + i * w + 1, VY1 - 6, w - 2, 4, on ? KNOB_RGB[3] : K_LINE, on ? 255 : 200);
    }
}


/* --------------------------------------------------------------------------------------------- the scenes --- */
static void vz_scenes(void)
{
    float m = GV.morph;
    int p, n = 0;
    int32_t x, bx = VX0 + 14, bw = VW - 28, y = VY0 + 10;
    vz_panel(0, 0);
    d_text(VX0, VY0 - 2, &FONT_XS, "A", KNOB_RGB[0]);
    d_text_r(VX1, VY0 - 2, &FONT_XS, "B", KNOB_RGB[2]);
    if (!G_SCENEB_OK) {
        d_text_c(SW / 2, (VY0 + VY1) / 2 - 12, &FONT_XS, TR("PAS DE SCENE B", "NO SCENE B"), K_SOFT);
        d_text_c(SW / 2, (VY0 + VY1) / 2 + 2, &FONT_XS, TR("K2: PRENDRE   K3: MUTER", "K2: TAKE   K3: MUTATE"), K_DIM);
        return;
    }
    /* the morph: a bar from A to B */
    d_rbox(bx, y, bw, 6, 3, K_LINE, 255);
    d_rbox(bx, y, (int32_t)((float)bw * m) + 1, 6, 3, mix8(KNOB_RGB[0], KNOB_RGB[2], (uint32_t)(m * 256.0f)), 255);
    d_glow((float)bx + (float)bw * m, (float)y + 3.0f, 4.0f, K_TEXT, 230);
    /* what differs: a line for each parameter that B moves, where it is now between A and B */
    for (p = 0; p < NP && n < 48; p++) {
        float a, b, now;
        int32_t cy;
        if ((PDEF[p].flags & (PF_GLOBAL | PF_ACTION | PF_ENUM)) || p == P_MORPH || p == P_AUTOM || G.v[p] == G_SCENEB[p])
            continue;
        a = pnorm(p);
        b = pnorm_raw(p, G_SCENEB[p]);
        now = a + (b - a) * m;
        x = bx + (n % 24) * bw / 24 + 2;
        cy = y + 14 + (n / 24) * 30;
        d_vline(x, cy, 26, K_GRID, 255);
        d_rect_a(x - 1, cy + 26 - (int32_t)(a * 26.0f), 3, 1, KNOB_RGB[0], 255);
        d_rect_a(x - 1, cy + 26 - (int32_t)(b * 26.0f), 3, 1, KNOB_RGB[2], 255);
        d_glow((float)x, (float)cy + 26.0f - now * 26.0f, 1.6f, K_TEXT, 220);
        n++;
    }
    if (!n)
        d_text_c(SW / 2, y + 30, &FONT_XS, TR("A = B : CHANGE DES KNOBS", "A = B: TURN SOME KNOBS"), K_DIM);
}

/* ---------------------------------------------------------------------------------------------- the drone --- */
static void vz_drone(void)
{
    float st = pval(P_STRETCH, vz_n(P_STRETCH)), det = vz_n(P_DETUNE), bl = vz_n(P_XFADE);
    float len = fm_minf(0.92f, 0.12f * st), mid = (float)(VY0 + VY1) * 0.5f;
    int i, voices = clamp(G.v[P_VOICES], 1, G_MAXV);
    int32_t x;
    vz_panel(0, 0);
    /* the long grains: windows that overlap, more of them as they stretch */
    for (i = 0; i < 9; i++) {
        float x0 = (float)i / 9.0f - len * 0.5f, px = 0.0f, py = 0.0f;
        rgb_t c = vz_rnd((uint32_t)i + 3u) < bl ? KNOB_RGB[3] : KNOB_RGB[0];
        for (x = 0; x <= 40; x++) {
            float t = (float)x / 40.0f, gx = (float)VX0 + (x0 + t * len) * (float)VW;
            float gy = mid + 8.0f - (0.5f - 0.5f * fm_sin_turns(t + 0.25f)) * (float)(VH / 2 - 6);
            if (gx < (float)VX0 || gx > (float)VX1)
                continue;
            if (x)
                d_line(px, py, gx, gy, c, 150);
            px = gx;
            py = gy;
        }
    }
    /* the chorus: each voice a line, apart in pitch and in space */
    for (i = 0; i < voices; i++) {
        static const float VD[G_MAXV] = {0.0f, 0.18f, -0.18f, 0.09f, -0.27f, 0.27f};
        float y = (float)VY1 - 10.0f - VD[i] * det * 40.0f;
        d_hline(VX0 + 10 + i * 4, (int32_t)y, VW - 20 - i * 8, KNOB_RGB[1], 90u + (uint32_t)(det * 140.0f));
    }
    /* A and B: how much of each */
    d_rect_a(VX0, VY0 - 2, (int32_t)((float)VW * (1.0f - bl)), 3, KNOB_RGB[0], 220);
    d_rect_a(VX0 + (int32_t)((float)VW * (1.0f - bl)), VY0 - 2, (int32_t)((float)VW * bl), 3, KNOB_RGB[3], 220);
}

/* ---------------------------------------------------------------------------------------------- the chaos --- */
static void vz_chaos(void)
{
    static const float DV[4] = {1.0f, 0.5f, 0.25f, 0.125f};
    float ch = vz_n(P_CHAOS), prob = vz_n(P_CHPROB), beat = (float)GV.beat_q16 / 65536.0f;
    float div = DV[clamp(G.v[P_CHDIV], 0, 3)];
    int mode = G.v[P_CHOP], i, steps = (int)(4.0f / div), now = (int)(fm_floorf(beat / div)) % (steps > 0 ? steps : 1);
    int32_t cw;
    vz_panel(0, 0);
    /* the wild grains: dots thrown about, more as CHAOS rises */
    for (i = 0; i < 40; i++)
        if (vz_rnd((uint32_t)i * 7u + 1u) < ch)
            d_glow((float)VX0 + vz_rnd((uint32_t)i * 7u + 2u) * (float)VW,
                   (float)VY0 + vz_rnd((uint32_t)i * 7u + 3u) * (float)(VH - 30), 1.4f + 2.0f * vz_rnd((uint32_t)i + 9u),
                   KNOB_RGB[0], 200);
    /* the chopper: a bar of steps, each lit with the chance it acts; the one acting now */
    if (steps > 32)
        steps = 32;
    cw = VW / steps;
    for (i = 0; i < steps; i++) {
        int on = mode && i == now && GV.chop;
        rgb_t c = on ? (GV.chop == 3 ? K_REC : GV.chop == 4 ? KNOB_RGB[2] : KNOB_RGB[1]) : K_LINE;
        int32_t h = 4 + (int32_t)(prob * 14.0f);
        d_rbox(VX0 + i * cw + 1, VY1 - h, cw - 2, h, 2, c, on ? 255 : mode ? 200 : 120);
    }
    if (GV.chop == 3 || GV.chop == 4) {                         /* the tape's speed */
        int32_t w = (int32_t)(fm_minf(GV.tape, 2.0f) * 0.5f * (float)VW);
        d_rect_a(VX0, VY1 - 24, w, 2, GV.chop == 3 ? K_REC : KNOB_RGB[2], 255);
    }
}

/* ----------------------------------------------------------------------------------------------- the halo --- */
static void vz_halo(void)
{
    float sh = vz_n(P_SHIM), rs = vz_n(P_RESO), inf = vz_n(P_INFINI);
    int iv = clamp(G.v[P_SHINT], 0, 3), i, k;
    vz_panel(0, 0);
    /* the shimmer: copies of the sound climbing (or falling), each fainter */
    for (k = 0; k < 5; k++) {
        float y = iv == 3 ? (float)VY0 + 8.0f + (float)k * 14.0f : (float)VY1 - 8.0f - (float)k * 14.0f;
        uint32_t a = sh < 0.005f ? 0u : (uint32_t)((90.0f + sh * 165.0f) * (1.0f - (float)k * 0.17f));
        if (a < 10u)
            continue;
        for (i = 0; i < 5; i++)
            d_glow((float)VX0 + 18.0f + (float)i * 12.0f + (float)k * (iv == 1 ? 4.0f : 7.0f), y, 2.0f, KNOB_RGB[0], a);
    }
    /* the resonators: three strings on the chord, ringing with RESONAT */
    for (k = 0; k < 3; k++) {
        static const float FR[3] = {1.0f, 1.4983f, 2.5198f};
        int32_t x = VX0 + 100 + k * 34, x2;
        float amp = rs * 9.0f, py = 0.0f;
        for (x2 = 0; x2 <= VH; x2++) {
            float y = (float)(VY0 + x2), dx = amp * fm_sin_turns((float)x2 / (float)VH * FR[k] * 2.0f);
            if (x2)
                d_line((float)x + py, y - 1.0f, (float)x + dx, y, KNOB_RGB[2], 120u + (uint32_t)(rs * 130.0f));
            py = dx;
        }
    }
    /* INFINI: a frozen bar across, as it holds */
    if (inf > 0.01f) {
        d_rect_a(VX0, VY1 - 6, VW, 4, KNOB_RGB[3], (uint32_t)(inf * 200.0f));
        d_glow((float)VX1 - 4.0f, (float)VY1 - 4.0f, 3.0f + 3.0f * inf, KNOB_RGB[3], 230);
    }
}

/* -------------------------------------------------------------------------------------------- the chopper --- */
static void vz_chopper(void)
{
    static const uint16_t PAT[8] = {0xFFFF, 0x5555, 0x1449, 0x4949, 0xDDDD, 0xFFFF, 0x1111, 0xFFFF};
    static const float LEN[8] = {1.0f, 0.6f, 0.7f, 0.7f, 0.55f, 0.3f, 0.5f, 0.75f};
    int pat = clamp(G.v[P_GATE], 0, 7), i;
    float sw = vz_n(P_SWING), ring = vz_n(P_RING), fold = vz_n(P_FOLD), mid = (float)VY0 + 16.0f, px = 0.0f, py = 0.0f;
    int32_t x, cw = VW / 16;
    vz_panel(0, 0);
    /* the gate: sixteen steps, the odd ones late with the swing */
    for (i = 0; i < 16; i++) {
        int on = pat ? (pat == 7 ? (int)(vz_hash((uint32_t)i) & 1u) : (int)(PAT[pat] >> i & 1u)) : 1;
        float off = (i & 1) ? sw * 0.5f * (float)cw : 0.0f, w = (float)cw * LEN[pat] * ((i & 1) ? 1.0f - sw * 0.5f : 1.0f);
        int now = pat && GV.gate_step == i;
        d_rbox(VX0 + i * cw + (int32_t)off, VY1 - 22, (int32_t)fm_maxf(w - 1.0f, 2.0f), 20, 2,
               on ? (now && GV.gate_open ? K_TEXT : KNOB_RGB[0]) : K_LINE, on ? 230 : 140);
    }
    /* the ring and the fold, on a sine */
    for (x = VX0; x <= VX1; x++) {
        float t = (float)(x - VX0) / (float)VW, v = fm_sin_turns(t * 2.0f), y;
        if (fold > 0.005f) {
            float g = 1.0f + 7.0f * fold, w = fm_minf(1.0f, fold * 3.0f);
            v += (0.8f * fm_sin_turns(v * g * 0.25f) - v) * w;
        }
        v *= 1.0f - ring + ring * 1.6f * fm_sin_turns(t * 2.0f * 7.07f);
        y = mid + 10.0f - v * 16.0f;
        if (x > VX0)
            vz_seg(px, py, (float)x, y, ring > fold ? KNOB_RGB[2] : KNOB_RGB[3], 220);
        px = (float)x;
        py = y;
    }
}

/* ----------------------------------------------------------------------------------------------- the rain --- */
static void vz_rain(void)
{
    uint16_t mask = G_SCALE_MASK[clamp(G.v[P_SCALE], 0, G_NSCALE - 1)];
    int tonic = clamp(G.v[P_TONIC], 0, 11), range = clamp(G.v[P_RRANGE], 1, 4), n = range * 12 + 1, i, j;
    float rate = vz_n(P_RAIN), len = vz_n(P_RLEN);
    uint32_t t = plat_ms() / 30u;
    int32_t kw = VW / n > 1 ? VW / n : 1;
    vz_panel(0, 0);
    for (i = 0; i < n; i++) {                                   /* the notes the rain may fall on */
        int in = mask >> ((i + 12) % 12) & 1;
        d_rect_a(VX0 + i * kw, VY1 - 6, kw - 1, 6, in ? ((i % 12) ? KNOB_RGB[2] : KNOB_RGB[0]) : K_GRID, in ? 200 : 255);
    }
    (void)tonic;
    if (!G.v[P_RAIN])
        return;
    for (j = 0; j < 14; j++) {                                  /* drops, more as it rains harder */
        uint32_t seed = (uint32_t)j * 97u, cyc;
        float speed = 0.6f + vz_rnd(seed) * 0.8f, ph;
        int k;
        if (vz_rnd(seed + 1u) > rate * 1.4f)
            continue;
        ph = (float)t * speed * 0.03f + vz_rnd(seed + 2u);
        cyc = (uint32_t)ph;
        ph -= (float)cyc;
        k = (int)(vz_rnd(seed + cyc * 13u) * (float)n);
        for (i = 0; i < 12 && !(mask >> ((k + 12) % 12) & 1); i++)
            k = (k + 1) % n;
        {
            float x = (float)(VX0 + k * kw) + (float)kw * 0.5f, y = (float)VY0 + ph * (float)(VH - 8);
            d_line(x, y - 4.0f - len * 10.0f, x, y, KNOB_RGB[1], 180);
            d_glow(x, y, 2.0f, KNOB_RGB[1], 230);
        }
    }
}

static void draw_viz(void)
{
    switch (viz_kind()) {
    case VZ_GRAIN: vz_grain(); break;
    case VZ_SCAN: vz_scan(); break;
    case VZ_AENV: vz_env(P_AATK, P_ADEC, P_ASUS, P_AREL, TR("AMPLI", "AMP ENV")); break;
    case VZ_MENV: vz_env(P_MATK, P_MDEC, P_MSUS, P_MREL, TR("ENV MOD", "MOD ENV")); break;
    case VZ_LFO1: vz_lfo(0); break;
    case VZ_LFO2: vz_lfo(1); break;
    case VZ_FILT: vz_filter(); break;
    case VZ_ECHO: vz_echo(); break;
    case VZ_SPACE: vz_space(); break;
    case VZ_MATTER: vz_matter(); break;
    case VZ_ARP: vz_arp(); break;
    case VZ_SCALE: vz_scale(); break;
    case VZ_CLOCK: vz_clock(); break;
    case VZ_SCENES: vz_scenes(); break;
    case VZ_DRONE: vz_drone(); break;
    case VZ_CHAOS: vz_chaos(); break;
    case VZ_HALO: vz_halo(); break;
    case VZ_CHOPPER: vz_chopper(); break;
    case VZ_RAIN: vz_rain(); break;
    default: break;
    }
}
