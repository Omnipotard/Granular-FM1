/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 GRAINE contributors */
/* The screen: a header, the cloud (the source's waveform with the grains glowing where they read), and the four
 * knobs, each its own colour on every page. Drawn in three bands of 80 rows. Part of the unity build (app.c). */

#define HEAD_H 22
#define WX 8
#define WW 224
#define WY0 28
#define WY1 122
#define WC 75
#define WA 44
#define INFO_Y 124
#define OV_Y 140
#define OV_H 16
#define STRIP_Y 160

/* ------------------------------------------------------------------------------------- the waveforms --- */
static struct {
    int src;
    int32_t ws, wl;
    uint32_t len, t;
    int8_t mn[WW], mx[WW];                        /* the window */
    int8_t omn[WW], omx[WW];                      /* the whole source */
} wc = {.src = -1};

static inline float src_at(const gsrc_t *s, uint32_t i)
{
    return s->u8 ? g_ulaw[s->u8[i]] : (float)s->s16[i] * (1.0f / 32768.0f);
}
static void cols(const gsrc_t *s, int32_t from, int32_t span, int8_t *mn, int8_t *mx)
{
    int c;
    for (c = 0; c < WW; c++) {
        int32_t a = from + (int32_t)((float)span * (float)c / (float)WW), b = from + (int32_t)((float)span * (float)(c + 1) / (float)WW), i;
        int32_t step;
        float lo = 0.0f, hi = 0.0f;
        if (b <= a)
            b = a + 1;
        step = (b - a) / 24 + 1;
        for (i = a; i < b && i < (int32_t)s->len; i += step) {
            float v = src_at(s, (uint32_t)i);
            if (v < lo)
                lo = v;
            if (v > hi)
                hi = v;
        }
        mn[c] = (int8_t)clamp((int32_t)(lo * 127.0f), -127, 0);
        mx[c] = (int8_t)clamp((int32_t)(hi * 127.0f), 0, 127);
    }
}
static void wave_cache(uint32_t now)
{
    int si = clamp(G.v[P_SAMPLE], 0, G_NSRC - 1);
    const gsrc_t *s = &G_SRC[si];
    int32_t ws, wl;
    if (s->len < 64u) {
        int c;
        for (c = 0; c < WW; c++)
            wc.mn[c] = wc.mx[c] = wc.omn[c] = wc.omx[c] = 0;
        wc.src = si;
        wc.len = s->len;
        return;
    }
    ws = (int32_t)(pnorm(P_START) * (float)s->len);
    wl = (int32_t)(pnorm(P_LEN) * (float)s->len);
    if (ws > (int32_t)s->len - 64)
        ws = (int32_t)s->len - 64;
    if (ws + wl > (int32_t)s->len)
        wl = (int32_t)s->len - ws;
    if (wl < 32)
        wl = 32;
    if (wc.src == si && wc.ws == ws && wc.wl == wl && wc.len == s->len)
        return;
    if (si == G_SRC_CAPT && GV.cap_on && now - wc.t < 150u && wc.src == si)
        return;                                    /* capturing: again a few times a second */
    cols(s, ws, wl, wc.mn, wc.mx);
    cols(s, 0, (int32_t)s->len, wc.omn, wc.omx);
    wc.src = si;
    wc.ws = ws;
    wc.wl = wl;
    wc.len = s->len;
    wc.t = now;
}

/* ------------------------------------------------------------------------------------------- header ---- */
static void chip(int32_t *x, const char *s, rgb_t c, int filled)
{
    int32_t w = text_w(&FONT_T, s) + 8;
    *x -= w;
    if (filled)
        d_rbox(*x, 5, w, 13, 4, c, 255);
    else
        d_rframe(*x, 5, w, 13, c, 200);
    d_text(*x + 4, 5, &FONT_T, s, filled ? K_INK : c);
    *x -= 4;
}

static void draw_header(uint32_t now)
{
    int pg = cur_page(), i;
    int32_t x;
    char b[24];
    d_rect(0, 0, SW, HEAD_H, RGB8(20, 16, 32));
    d_hline(0, HEAD_H - 1, SW, K_LINE, 255);
    if (ui.mod_src >= 0 && (held(B_LFO) || held(B_ENV))) {   /* assigning: the source fills the header */
        rgb_t c = SRC_RGB[ui.mod_src];
        d_rect(0, 0, SW, HEAD_H - 1, scale8(c, 70));
        x = d_text(8, 3, &FONT_B, SRC_NAME[ui.mod_src][LG], c);
        d_text(x + 8, 5, &FONT_XS, TR("> TOURNE UN KNOB", "> TURN A KNOB"), K_TEXT);
        d_text_r(234, 5, &FONT_XS, "OCT -/+", K_SOFT);
        return;
    }
    if (held(B_REC) && !ui.acted[B_REC] && now - ui.down_at[B_REC] > 250u) {
        d_rect(0, 0, SW, HEAD_H - 1, RGB8(70, 18, 26));
        x = d_text(8, 3, &FONT_B, TR("GESTE", "GESTURE"), K_REC);
        d_text(x + 8, 5, &FONT_XS, TR("> TOURNE UN KNOB", "> TURN A KNOB"), K_TEXT);
        return;
    }
    x = d_text(8, 3, &FONT_B, PAGES[pg].name[LG], K_TEXT);
    if (GROUP_N[ui.group] > 1)                       /* the sub-pages: dots, this one lit */
        for (i = 0; i < GROUP_N[ui.group]; i++)
            d_disc((float)(x + 8 + i * 8), 11.0f, 2.2f, i == ui.sub[ui.group] % GROUP_N[ui.group] ? K_TEXT : K_LINE,
                   255);
    /* right: the patch, then what is on */
    s_int(cur_preset + 1, b, 0);
    if (cur_preset < 9) {
        b[1] = b[0];
        b[0] = '0';
        b[2] = 0;
    }
    x = d_text_r(234, 5, &FONT_XS, patch_name, ui.dirty ? KNOB_RGB[1] : K_SOFT);
    x = d_text_r(x - 4, 5, &FONT_XS, b, K_DIM);
    x -= 6;
    if (GV.cap_on)
        chip(&x, "REC", K_REC, (now / 250u) & 1u);
    if (GP.freeze || (GP.pads & 1u << PAD_GEL))
        chip(&x, TR("GEL", "FRZ"), K_ICE, 1);
    if (tapis_on())
        chip(&x, "TAPIS", KNOB_RGB[1], 1);
    if (GP.running) {
        s_int((int32_t)(GV.tempo + 0.5f), b, 0);
        chip(&x, b, RGB8(120, 240, 150), 0);
    }
    if (GP.octave) {
        char o[8] = "OCT";
        s_int(GP.octave, o + 3, 1);
        chip(&x, o, K_SOFT, 0);
    }
}

/* ---------------------------------------------------------------------------------------------- scene --- */
static void draw_backdrop(void)
{
    int32_t y;
    for (y = HEAD_H; y < STRIP_Y; y++) {
        uint32_t t = (uint32_t)((y - HEAD_H) * 256 / (STRIP_Y - HEAD_H));
        if (y >= BY && y < BY + BAND_H)
            d_hline(0, y, SW, mix8(K_BG_TOP, K_BG_BOT, t), 255);
    }
    for (y = 0; y <= 8; y++)                         /* a faint grid: eighths of the window */
        d_vline(WX + y * WW / 8, WY0, WY1 - WY0, K_GRID, 120);
    d_hline(WX, WC, WW, K_GRID, 200);
}

static void draw_wave(int newest_v)
{
    int c;
    float hot = newest_v >= 0 ? (float)GV.vpos[newest_v] / 65535.0f * (float)WW : -999.0f;
    int frozen = GP.freeze || (GP.pads & 1u << PAD_GEL);
    for (c = 0; c < WW; c++) {
        int32_t top = WC - wc.mx[c] * WA / 127, bot = WC - wc.mn[c] * WA / 127;
        float d = fm_fabsf((float)c - hot);
        uint32_t heat = d < 18.0f ? (uint32_t)((18.0f - d) * 12.0f) : 0u;
        rgb_t col = mix8(K_WAVE, frozen ? K_ICE : K_WAVE_HI, heat > 200u ? 200u : heat);
        if (bot - top < 1)
            bot = top + 1;
        d_vline(WX + c, top, bot - top + 1, col, 210);
    }
}

static int newest_voice(void)
{
    int j, nv = -1;
    uint32_t best = 0;
    for (j = 0; j < G_MAXV; j++)
        if (GV.vlev[j] > best) {
            best = GV.vlev[j];
            nv = j;
        }
    return nv;
}
static void draw_spray(int nv)
{
    if (nv >= 0) {                                   /* the spray: where the newest voice's grains may land */
        float sp = g_clamp01(pnorm(P_SPRAY)) * (float)WW;
        int32_t cx = WX + (int32_t)((float)GV.vpos[nv] / 65535.0f * (float)WW);
        if (sp > 2.0f)
            d_rect_a(cx - (int32_t)(sp * 0.5f), WY0, (int32_t)sp, WY1 - WY0, VOICE_RGB[nv], 22);
    }
}
static void draw_playheads(void)
{
    int j;
    for (j = 0; j < G_MAXV; j++) {
        int32_t x;
        uint32_t a;
        if (!GV.vlev[j])
            continue;
        x = WX + (int32_t)((float)GV.vpos[j] / 65535.0f * (float)WW);
        a = 60u + GV.vlev[j] * 150u / 255u;
        d_vline(x, WY0, WY1 - WY0, VOICE_RGB[j], a / 2u);
        d_line((float)x - 3.0f, (float)WY0 - 4.0f, (float)x, (float)WY0, VOICE_RGB[j], a);
        d_line((float)x + 3.0f, (float)WY0 - 4.0f, (float)x, (float)WY0, VOICE_RGB[j], a);
    }
}

static void draw_grains(void)
{
    int i, n = GV.n;
    float crowd = n > 12 ? fm_sqrtf(12.0f / (float)n) : 1.0f;   /* many at once: each a little dimmer */
    for (i = 0; i < n; i++) {
        const gsnap_t *g = &GV.g[i];
        float x = (float)WX + (float)g->x / 65535.0f * (float)WW;
        float y = (float)WC - (float)g->semis * 2.3f + (float)g->pan * 0.42f;
        float r = 1.0f + (float)g->size / 255.0f * 4.2f;
        uint32_t a = (uint32_t)((60.0f + (float)g->amp * 0.75f) * crowd);
        rgb_t c = g->voice < G_MAXV ? VOICE_RGB[g->voice] : K_TEXT;
        if (g->rev)
            c = mix8(c, K_ICE, 140);
        y = fm_clampf(y, (float)WY0 + 3.0f, (float)WY1 - 3.0f);
        d_glow(x, y, r, c, a);
    }
}

static void draw_overview(void)
{
    int c;
    const gsrc_t *s = &G_SRC[clamp(G.v[P_SAMPLE], 0, G_NSRC - 1)];
    int32_t mid = OV_Y + OV_H / 2, x0, x1, j;
    if (s->len < 64u)
        return;
    for (c = 0; c < WW; c++) {
        int32_t t = mid - wc.omx[c] * (OV_H / 2) / 127, b = mid - wc.omn[c] * (OV_H / 2) / 127;
        d_vline(WX + c, t, b - t + 1, K_DIM, 160);
    }
    x0 = WX + (int32_t)((float)wc.ws * (float)WW / (float)s->len);
    x1 = WX + (int32_t)((float)(wc.ws + wc.wl) * (float)WW / (float)s->len);
    if (x1 - x0 < 3)
        x1 = x0 + 3;
    d_rect_a(x0, OV_Y - 1, x1 - x0, OV_H + 2, K_WAVE_HI, 34);
    d_vline(x0, OV_Y - 2, OV_H + 4, K_WAVE_HI, 230);
    d_vline(x1 - 1, OV_Y - 2, OV_H + 4, K_WAVE_HI, 230);
    for (j = 0; j < G_MAXV; j++)
        if (GV.vlev[j]) {
            int32_t x = x0 + (int32_t)((float)GV.vpos[j] / 65535.0f * (float)(x1 - x0));
            d_vline(x, OV_Y, OV_H, VOICE_RGB[j], 255);
        }
}

static const char *const PAD_LBL[NPADS][2] = {{"GEL", "FRZ"}, {"ENV", "REV"}, {"-8", "-8"}, {"+8", "+8"},
                                              {"5TE", "5TH"}, {"NUE", "SWM"}, {"POU", "DST"}, {"BEG", "STT"},
                                              {"DER", "DRF"}, {"SOM", "DRK"}, {"ECH", "ECH"}};
static void draw_tapis(void)
{
    int i;
    uint32_t held_slices = 0;
    for (i = 0; i < 16; i++)
        if (ui.keys >> WHITE[i] & 1u)
            held_slices |= 1u << i;
    for (i = 0; i < 16; i++) {
        int32_t x = WX + i * WW / 16, w = WW / 16;
        char n[4];
        if (held_slices >> i & 1u)
            d_rect_a(x, WY0, w, WY1 - WY0, KNOB_RGB[1], 40);
        if (i)
            d_vline(x, WY0, WY1 - WY0, KNOB_RGB[1], 70);
        s_int(i + 1, n, 0);
        d_text_c(x + w / 2, WY0 + 1, &FONT_T, n, held_slices >> i & 1u ? K_TEXT : K_DIM);
    }
    for (i = 0; i < NPADS; i++) {                    /* the pads, where the overview was */
        int32_t x = 5 + i * 21;
        int on = GP.pads >> i & 1u;
        if (on)
            d_rbox(x, OV_Y - 2, 20, 18, 4, KNOB_RGB[1], 255);
        else
            d_rframe(x, OV_Y - 2, 20, 18, K_LINE, 255);
        d_text_c(x + 10, OV_Y + 1, &FONT_T, PAD_LBL[i][LG], on ? K_INK : K_SOFT);
    }
}

static void draw_info(void)
{
    const gsrc_t *s = &G_SRC[clamp(G.v[P_SAMPLE], 0, G_NSRC - 1)];
    char b[32], t[12];
    int32_t ms = s->rate ? (int32_t)((float)s->len * 1000.0f / (float)s->rate) : 0;
    b[0] = 0;
    s_cat(b, src_name(G.v[P_SAMPLE]));
    if (s->len >= 64u) {
        s_cat(b, "  ");
        s_fix(ms / 10, 2, t, 0);
        s_cat(b, t);
        s_cat(b, "s");
    }
    d_text(WX, INFO_Y, &FONT_T, b, K_DIM);
    b[0] = 0;
    s_int(GV.n, t, 0);
    s_cat(b, t);
    s_cat(b, TR(" GRAINS", " GRAINS"));
    if (GV.grains_dropped)
        s_cat(b, " +");
    d_text_r(WX + WW, INFO_Y, &FONT_T, b, GV.n > 48 ? KNOB_RGB[0] : K_DIM);
}

static void draw_empty(void)                     /* an empty source says how to fill it */
{
    const gsrc_t *s = &G_SRC[clamp(G.v[P_SAMPLE], 0, G_NSRC - 1)];
    if (s->len >= 64u || GV.cap_on)
        return;
    d_rbox(30, WC - 26, SW - 60, 52, 10, K_INK, 230);
    d_text_c(SW / 2, WC - 21, &FONT_B, TR("SOURCE VIDE", "EMPTY SOURCE"), K_SOFT);
    d_text_c(SW / 2, WC + 1, &FONT_XS, G.v[P_SAMPLE] == G_SRC_CAPT ? TR("REC CAPTURE CE QUE TU JOUES", "REC CAPTURES WHAT YOU PLAY")
                                                                 : TR("SELECT: UNE AUTRE SOURCE", "SELECT: ANOTHER SOURCE"), K_DIM);
}

static void draw_capture_border(uint32_t now)
{
    uint32_t a = 120u + (uint32_t)(100.0f * (0.5f + 0.5f * fm_sin_turns((float)(now % 1000u) / 1000.0f)));
    char b[20], t[12];
    d_rect_a(0, HEAD_H, SW, 2, K_REC, a);
    d_rect_a(0, STRIP_Y - 2, SW, 2, K_REC, a);
    d_rect_a(0, HEAD_H, 2, STRIP_Y - HEAD_H, K_REC, a);
    d_rect_a(SW - 2, HEAD_H, 2, STRIP_Y - HEAD_H, K_REC, a);
    b[0] = 0;
    s_cat(b, "REC ");
    s_fix((int32_t)(GV.cap_len * 100u / G_CAP_RATE), 2, t, 0);
    s_cat(b, t);
    s_cat(b, "s");
    d_disc(14.0f, (float)WY0 + 6.0f, 3.5f, K_REC, a);
    d_text(21, WY0, &FONT_T, b, K_REC);
}

static void draw_toast(uint32_t now)
{
    char v[28];
    int32_t y = WY0 + 2, x, w, h = 24, tw, vw;
    rgb_t c;
    const char *a;
    const felucca_font_t *vf;
    float bar = -1.0f, b0 = 0.0f;
    if (!ui.toast_until || now >= ui.toast_until)
        return;
    if (ui.toast_p >= 1000) {                        /* a modulation's depth */
        int p = ui.toast_p - 1000, dep = mod_depth(ui.mod_src < 0 ? 0 : ui.mod_src, p);
        c = SRC_RGB[ui.mod_src < 0 ? 0 : ui.mod_src];
        a = PDEF[p].name[LG];
        s_int(dep, v, 1);
        s_cat(v, "%");
        bar = 0.5f + (float)dep * 0.005f;
        b0 = 0.5f;
    } else if (ui.toast_p >= 0) {
        int p = ui.toast_p, k;
        c = K_TEXT;
        for (k = 0; k < 4; k++)
            if (PAGES[cur_page()].p[k] == p)
                c = KNOB_RGB[k];
        if (p == P_SAMPLE)
            c = KNOB_RGB[0];
        if (p == P_SPEED)
            c = KNOB_RGB[3];
        a = PDEF[p].name[LG];
        pfmt(p, G.v[p], v);
        if (!(PDEF[p].flags & (PF_ENUM | PF_ACTION)))
            bar = pnorm(p);
        if (p == P_SPEED || p == P_PITCH || p == P_SKEW || p == P_KEYPOS || p == P_FINE || p == P_GLIDE)
            b0 = 0.5f;
    } else {
        c = ui.toast_c;
        a = ui.toast_a;
        v[0] = 0;
        s_cat(v, ui.toast_b);
    }
    vf = fit(v, 120, &FONT_B, &FONT_XS, &FONT_T);
    tw = text_w(&FONT_XS, a);
    vw = text_w(vf, v);
    w = tw + vw + 26;
    if (w > SW - 16)
        w = SW - 16;
    x = (SW - w) / 2;
    d_rbox(x, y, w, h, 11, K_INK, 232);
    d_rbox(x + 1, y + 1, w - 2, h - 2, 10, scale8(c, 34), 255);
    d_text(x + 10, y + 5, &FONT_XS, a, c);
    d_text_r(x + w - 10, y + 4, vf, v, K_TEXT);
    if (bar >= 0.0f) {
        int32_t bx = x + 10, bw = w - 20, p0 = bx + (int32_t)(b0 * (float)bw), p1 = bx + (int32_t)(bar * (float)bw);
        d_rect(bx, y + h - 3, bw, 1, K_LINE);
        d_rect(p0 < p1 ? p0 : p1, y + h - 3, (p1 > p0 ? p1 - p0 : p0 - p1) + 1, 2, c);
    }
}

static void draw_overlay(void)
{
    char b[24];
    int32_t y = WY0 - 2;
    if (ui.ov == OV_NONE)
        return;
    d_rbox(14, y, SW - 28, 108, 10, K_INK, 240);
    if (ui.ov == OV_SAVE) {
        d_rframe(14, y, SW - 28, 108, KNOB_RGB[2], 255);
        d_text_c(SW / 2, y + 6, &FONT_XS, TR("SAUVER LE SON DANS", "SAVE THE PATCH TO"), KNOB_RGB[2]);
        s_int(ui.ov_sel + 1, b, 0);
        d_text_c(SW / 2, y + 22, &FONT_L, b, K_TEXT);
        d_text_c(SW / 2, y + 58, &FONT_B, bank.p[ui.ov_sel].name, K_SOFT);
        d_text_c(SW / 2, y + 84, &FONT_XS, TR("OCT+ OUI     OCT- NON", "OCT+ YES     OCT- NO"), K_DIM);
    } else if (ui.ov == OV_KEEP) {
        const gsrc_t *s = &G_SRC[G_SRC_CAPT + 1 + ui.ov_sel];
        d_rframe(14, y, SW - 28, 108, K_REC, 255);
        d_text_c(SW / 2, y + 6, &FONT_XS, TR("GARDER LA CAPTURE DANS", "KEEP THE CAPTURE IN"), K_REC);
        b[0] = 'U';
        b[1] = (char)('1' + ui.ov_sel);
        b[2] = 0;
        d_text_c(SW / 2, y + 22, &FONT_L, b, K_TEXT);
        d_text_c(SW / 2, y + 58, &FONT_B, s->len ? s->name : TR("(VIDE)", "(EMPTY)"), s->len ? KNOB_RGB[1] : K_SOFT);
        d_text_c(SW / 2, y + 84, &FONT_XS, TR("OCT+ OUI     OCT- NON", "OCT+ YES     OCT- NO"), K_DIM);
    } else {
        d_rframe(14, y, SW - 28, 108, K_REC, 255);
        d_text_c(SW / 2, y + 40, &FONT_B, TR("ECRITURE...", "WRITING..."), K_TEXT);
    }
}

/* the boot: seeds gather into the name */
static void draw_splash(uint32_t t)
{
    int i;
    float k = t < 1100u ? (float)t / 1100.0f : 1.0f, e = 1.0f - (1.0f - k) * (1.0f - k);
    d_rect(0, BY, SW, BAND_H, K_INK);
    for (i = 0; i < 44; i++) {
        uint32_t h = (uint32_t)i * 2654435761u;
        float sx = (float)(h % 240u), sy = (float)((h >> 8) % 240u);
        float tx = 50.0f + (float)(i % 11) * 13.5f, ty = 104.0f + (float)(i / 11) * 7.0f;
        float x = sx + (tx - sx) * e, y = sy + (ty - sy) * e;
        d_glow(x, y, 1.6f + (float)(h >> 20 & 3u) * 0.5f, VOICE_RGB[i % G_MAXV], (uint32_t)(120.0f + 120.0f * k));
    }
    if (t > 600u) {
        uint32_t a = t > 1100u ? 255u : (t - 600u) * 255u / 500u;
        d_text_c(SW / 2, 96, &FONT_L, "GRAINE", scale8(K_TEXT, a + 1u));
        d_text_c(SW / 2, 134, &FONT_XS, TR("synthe granulaire  -  FM-1", "granular synth  -  FM-1"),
                 scale8(K_SOFT, a + 1u));
    }
}

/* -------------------------------------------------------------------------------------------- the knobs --- */
static int bipolar(int p)
{
    return p == P_PITCH || p == P_SKEW || p == P_KEYPOS || p == P_FINE || p == P_GLIDE || p == P_SPEED;
}
static void draw_knob(int k, uint32_t now)
{
    int p = PAGES[cur_page()].p[k], assigning = ui.mod_src >= 0 && (held(B_LFO) || held(B_ENV));
    float cx = 30.0f + 60.0f * (float)k, cy = (float)STRIP_Y + 33.0f, v = pnorm(p), m = GV.pmod[p];
    rgb_t c = KNOB_RGB[k];
    char b[24];
    const char *lbl = PDEF[p].lbl[LG];
    int lane = lane_of(p);
    /* the label, in the knob's colour (a gesture: a wave before it) */
    if (lane >= 0) {
        float ph = (float)GV.lane_step / (float)G_LANE_STEPS;
        int32_t lx = (int32_t)cx - text_w(&FONT_XS, lbl) / 2 - 9, i;
        for (i = 0; i < 7; i++) {
            float y0 = (float)STRIP_Y + 9.0f - 3.0f * fm_sin_turns((float)i / 7.0f + ph);
            float y1 = (float)STRIP_Y + 9.0f - 3.0f * fm_sin_turns((float)(i + 1) / 7.0f + ph);
            d_line((float)(lx + i), y0, (float)(lx + i + 1), y1, SRC_RGB[MS_ENV], 230);
        }
    }
    d_text_c((int32_t)cx, STRIP_Y + 3, fit(lbl, 58, &FONT_XS, &FONT_T, 0), lbl, c);
    /* the ring: its track, the value (from the middle for a two-sided one), the modulation outside */
    d_arc(cx, cy, 14.0f, 19.0f, 0.0f, 1.0f, K_LINE, 255);
    if (bipolar(p))
        d_arc(cx, cy, 14.0f, 19.0f, 0.5f, v, c, 255);
    else
        d_arc(cx, cy, 14.0f, 19.0f, 0.0f, v, c, 255);
    if (assigning) {
        int dep = mod_depth(ui.mod_src, p);
        rgb_t sc = SRC_RGB[ui.mod_src];
        float to = g_clamp01(v + (float)dep * 0.01f);
        if (dep)
            d_arc(cx, cy, 21.0f, 24.5f, v, to, sc, 255);
        d_disc(cx, cy, 6.0f, scale8(sc, PDEF[p].flags & PF_NOMOD ? 40 : 200), 255);
        if (PDEF[p].flags & PF_NOMOD)
            s_cat((b[0] = 0, b), "-");
        else {
            s_int(dep, b, 1);
            s_cat(b, "%");
        }
        d_text_c((int32_t)cx, STRIP_Y + 57, fit(b, 58, &FONT_B, &FONT_XS, &FONT_T), b, sc);
        return;
    }
    if (m >= 0.0f) {                                 /* modulated: the swing outside, a dot where it is now */
        float a = fm_minf(v, m), z = fm_maxf(v, m), t = m * ARC_SPAN - 2.35619449f;
        int ms = 0, i;
        for (i = 0; i < G_NMOD; i++)
            if (G.mod[i].dst == p && G.mod[i].depth) {
                ms = G.mod[i].src;
                break;
            }
        if (lane >= 0 && !(GP.running == 0))
            ms = MS_ENV;
        d_arc(cx, cy, 21.0f, 23.5f, a, z, SRC_RGB[ms], 220);
        d_disc(cx + 22.2f * fm_sinf(t), cy - 22.2f * fm_cosf(t), 2.4f, K_TEXT, 255);
    }
    {                                                /* the value's point on the ring, a small seed in the middle */
        float t = v * ARC_SPAN - 2.35619449f;
        d_disc(cx + 16.5f * fm_sinf(t), cy - 16.5f * fm_cosf(t), 2.6f, K_TEXT, 255);
        d_glow(cx, cy, 2.0f, c, 90u + (uint32_t)(v * 120.0f));
    }
    pfmt(p, G.v[p], b);
    d_text_c((int32_t)cx, STRIP_Y + 57, fit(b, 58, &FONT_B, &FONT_XS, &FONT_T), b, K_TEXT);
    (void)now;
}

static void draw_strip(uint32_t now)
{
    int k;
    d_rect(0, STRIP_Y, SW, SW - STRIP_Y, K_BG_BOT);
    d_hline(0, STRIP_Y, SW, K_LINE, 255);
    for (k = 1; k < 4; k++)
        d_vline(60 * k, STRIP_Y + 8, 64, K_GRID, 255);
    for (k = 0; k < 4; k++)
        draw_knob(k, now);
}

#include "viz.c"                                 /* the page's picture, over the cloud */

/* ------------------------------------------------------------------------------------------- a frame --- */
static uint32_t last_draw;
static uint32_t drawn_seq;

static int strip_animates(void)
{
    int k;
    if (ui.mod_src >= 0)
        return 1;
    for (k = 0; k < 4; k++) {
        int p = PAGES[cur_page()].p[k];
        if (GV.pmod[p] >= 0.0f || lane_of(p) >= 0)
            return 1;
    }
    return 0;
}

static void screen_frame(void)
{
    uint32_t now = plat_ms(), t = now - ui.boot;
    int anim, newest_v = -1, j;
    if (t < 1500u) {                                 /* the boot */
        if (now - last_draw < 33u)
            return;
        last_draw = now;
        for (j = 0; j < 3; j++) {
            band_begin(j * BAND_H);
            draw_splash(t);
            band_end();
        }
        ui.strip_dirty = ui.head_dirty = 1;
        return;
    }
    anim = GV.n || GV.cap_on || ui.ov || (ui.toast_until && now < ui.toast_until + 40u);
    for (j = 0; j < G_MAXV; j++)
        if (GV.vlev[j])
            anim = 1;
    if (viz_kind() != VZ_NONE && (ui.strip_dirty || viz_live() || (strip_animates() && GV.seq != drawn_seq)))
        anim = 1;                                    /* the page's picture follows its knobs */
    if (now - last_draw < 33u)
        return;
    if (!anim && !ui.head_dirty && !ui.strip_dirty && !(strip_animates() && GV.seq != drawn_seq))
        return;
    last_draw = now;
    drawn_seq = GV.seq;
    wave_cache(now);
    if (anim || ui.head_dirty) {
        for (j = 0; j < 2; j++) {
            band_begin(j * BAND_H);
            newest_v = newest_voice();
            draw_backdrop();
            draw_header(now);
            draw_spray(newest_v);
            draw_wave(newest_v);
            draw_playheads();
            if (tapis_on())
                draw_tapis();
            else
                draw_overview();
            draw_grains();
            draw_viz();
            draw_info();
            draw_empty();
            if (GV.cap_on)
                draw_capture_border(now);
            draw_toast(now);
            draw_overlay();
            band_end();
        }
        ui.head_dirty = 0;
    }
    if (ui.strip_dirty || strip_animates()) {
        band_begin(STRIP_Y);
        draw_strip(now);
        band_end();
        ui.strip_dirty = 0;
    }
}
