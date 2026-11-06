/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 GRAINE contributors */
/* GRAINE's sound: grains read from a source, gathered in voices, through a filter each, then the effects.
 * Its own unit at -O2 (fm1.toml o2), called by the audio interrupt (eng_render) in chunks of CH frames. Float only,
 * no double, no C library: the FM-1's toolchain has neither. */
#include <stdint.h>
#include "plat.h"
#include "fastmath.h"
#include "../graine.h"
#include "../sounds/bank.h"

#define CH 64u                                /* frames a chunk: the control rate (1.45 ms) */
#define WIN_N 512u                            /* the grain window's table */
#define DLY_LEN 26460u                        /* the echo: 0.6 s a side */
#define PI_F 3.14159265f

gstate_t G;
glane_t G_LANE[G_NLANE];
gsrc_t G_SRC[G_NSRC];
gview_t GV;
gplay_t GP;
int16_t G_CAP[G_CAP_LEN] PLAT_POOL;
const float *const g_ulaw = SND_ULAW;        /* the mu-law table, for the screen's waveforms */

/* scales: bit k = semitone k above the tonic */
const uint16_t G_SCALE_MASK[G_NSCALE] = {0xFFF, 0xAB5, 0x5AD, 0x6AD, 0x5AB, 0xAD5, 0x6B5, 0x295, 0x4A9, 0x4E9};

/* ------------------------------------------------------------------------------------------------ random --- */
static uint32_t rng = 0x9E3779B9u;
uint32_t g_rand(void)
{
    uint32_t x = rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng = x;
}
float g_randf(void) { return (float)(g_rand() >> 8) * (1.0f / 16777216.0f); }
static inline float randb(void) { return g_randf() * 2.0f - 1.0f; }

static inline float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float expr(float n, float lo, float hi_over_lo) { return lo * fm_exp2f(n * fm_log2f(hi_over_lo)); }
static inline float semis_ratio(float s) { return fm_exp2f(s * (1.0f / 12.0f)); }

/* --------------------------------------------------------------------------------------------- the units --- */
float pval(int p, float n)
{
    n = clamp01(n);
    switch (p) {
    case P_SIZE: return expr(n, 0.005f, 400.0f);              /* s: 5 ms .. 2 s */
    case P_DENS: return expr(n, 0.5f, 300.0f);                /* grains/s: 0.5 .. 150 */
    case P_PITCH: return -24.0f + 48.0f * n;                  /* semitones */
    case P_SPREAD: return 24.0f * n * n;                      /* semitones */
    case P_FINE: return -100.0f + 200.0f * n;                 /* cents */
    case P_SPEED: return -4.0f + 8.0f * n;                    /* x */
    case P_KEYPOS: case P_SKEW: return -1.0f + 2.0f * n;
    case P_GLIDE: return -24.0f + 48.0f * n;                  /* semitones over a grain */
    case P_AATK: case P_MATK: return expr(n, 0.001f, 8000.0f);   /* s: 1 ms .. 8 s */
    case P_ADEC: case P_MDEC: case P_AREL: case P_MREL: return expr(n, 0.003f, 4000.0f);   /* 3 ms .. 12 s */
    case P_L1RATE: case P_L2RATE: return expr(n, 0.02f, 1500.0f);   /* Hz: 0.02 .. 30 */
    case P_CUT: return expr(n, 30.0f, 600.0f);               /* Hz: 30 .. 18 k */
    case P_PORTA: return n < 0.005f ? 0.0f : expr(n, 0.002f, 1000.0f);
    case P_TEMPO: return 40.0f + 200.0f * n;
    case P_DFB: return 1.1f * n;
    case P_WIDTH: return 2.0f * n;
    case P_RDEC: return 0.3f * fm_exp2f(n * 7.0f);            /* RT60 s */
    case P_DOWN: return 1.0f + 31.0f * n * n;                 /* sample-and-hold factor */
    default: return n;
    }
}

/* divisions, in beats */
static const float DIV_SYNC_GRAIN[10] = {4.0f, 2.0f, 1.0f, 0.5f, 1.0f / 3, 0.25f, 1.0f / 6, 0.125f, 1.0f / 12, 0.0625f};
static const float DIV_LFO[11] = {32.0f, 16.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.5f, 1.0f / 3, 0.25f, 1.0f / 6, 0.125f};
static const float DIV_DELAY[13] = {1.0f / 32, 1.0f / 16, 0.125f, 1.0f / 6, 0.25f, 1.0f / 3, 0.375f, 0.5f, 2.0f / 3,
                                    0.75f, 1.0f, 4.0f / 3, 1.5f};
static const float DIV_ARP[6] = {1.0f, 0.5f, 1.0f / 3, 0.25f, 1.0f / 6, 0.125f};

/* ------------------------------------------------------------------------------------------------- events --- */
#define QN 64u
static struct { uint8_t t; int32_t a, b; } evq[QN];
static volatile uint32_t evw, evr;
void eng_event(uint8_t type, int32_t a, int32_t b)
{
    uint32_t w = evw;
    if (w - evr >= QN)
        return;                                           /* full: dropped (64 in 1.5 ms: never in practice) */
    evq[w % QN].t = type;
    evq[w % QN].a = a;
    evq[w % QN].b = b;
    evw = w + 1u;
}

/* -------------------------------------------------------------------------------------------- the window --- */
static float win[WIN_N + 1];
static float win_base[4][WIN_N + 1];                     /* gauss, hann, tukey, rect: made once */
static float win_shape = -1.0f, win_skew = -9.0f;
static float shape_base(int k, float t)
{
    float x = t * 2.0f - 1.0f, e = fm_fabsf(x);
    switch (k) {
    case 0: return fm_expf(-x * x * 9.0f);
    case 1: return 0.5f - 0.5f * fm_cosf(2.0f * PI_F * t);
    case 2: return e < 0.5f ? 1.0f : 0.5f + 0.5f * fm_cosf(PI_F * (e - 0.5f) * 2.0f);
    default: return e < 0.94f ? 1.0f : 0.5f + 0.5f * fm_cosf(PI_F * (e - 0.94f) / 0.06f);
    }
}
static void win_init(void)
{
    uint32_t i;
    int k;
    for (k = 0; k < 4; k++)
        for (i = 0; i <= WIN_N; i++)
            win_base[k][i] = shape_base(k, (float)i / (float)WIN_N);
}
static inline float base_at(int k, float u)               /* a base shape at u 0..1, interpolated */
{
    float x = u * (float)WIN_N, f;
    int32_t i = (int32_t)x;
    if (i >= (int32_t)WIN_N)
        return win_base[k][WIN_N];
    f = x - (float)i;
    return win_base[k][i] + (win_base[k][i + 1] - win_base[k][i]) * f;
}
/* the window: m 0..1 morphs gauss -> hann -> tukey -> rect; skew moves the peak (-1 early: percussive) */
static void win_make(float m, float skew)
{
    uint32_t i;
    int k;
    float a, pk = 0.5f + 0.47f * skew;
    if (fm_fabsf(m - win_shape) < 0.002f && fm_fabsf(skew - win_skew) < 0.002f)
        return;
    win_shape = m;
    win_skew = skew;
    m *= 3.0f;
    k = m >= 3.0f ? 2 : (int)m;
    a = m - (float)k;
    for (i = 0; i <= WIN_N; i++) {
        float t = (float)i / (float)WIN_N, u;
        u = t < pk ? 0.5f * t / pk : 0.5f + 0.5f * (t - pk) / (1.0f - pk);
        win[i] = lerpf(base_at(k, u), base_at(k + 1, u), a);
    }
    win[0] = 0.0f;
    win[WIN_N] = 0.0f;
}

/* ------------------------------------------------------------------------------------------------- grains --- */
typedef struct {
    const uint8_t *u8;
    const int16_t *s16;
    int32_t ipos, len1;                                   /* read index; the source's length - 1 */
    uint32_t frac;                                        /* the index's fraction, Q32 */
    float rate, rmul;                                     /* samples a frame (signed), its glide a chunk */
    uint32_t wph, winc;                                   /* the window's phase, Q32 */
    float amp, gl, gr;
    uint32_t delay;                                       /* frames to wait in this chunk */
    int32_t ws, wl;                                       /* the window in the source (for the screen) */
    int8_t semis, pan;
    uint8_t voice, on, rev, sizec;
} grain_t;
static grain_t gr[G_MAXG];
static uint32_t maxg = G_MAXG;                            /* lowered by an overload, then back */

/* ------------------------------------------------------------------------------------------------- voices --- */
typedef struct {
    uint8_t gate, sustained, held, slice_on;
    int32_t key;
    int32_t note;
    int32_t slice;
    float vel, pres, alea;
    uint32_t age;
    float pitch, pitch_tgt;                               /* semitones (glide) */
    float scan, dir, jump_t;                              /* the scan's offset in the window, 0..1 */
    float pos_sm;                                         /* the position, slewed */
    float next;                                           /* frames to the next grain */
    float stutter;                                        /* BEGAIE: the position held */
    uint8_t stut_on;
    int stage, mstage;                                    /* envelopes: 0 off, 1 attack, 2 decay, 3 sustain, 4 release */
    float lev, mlev;
    float ic[2][2];                                       /* the filter's state, L and R */
    float vn[NP];                                         /* the parameters for this voice, normalized */
    float pos_eff;
    int32_t ws, wl;                                       /* this chunk's window in the source */
} voice_t;
static voice_t V[G_MAXV];
static uint32_t vage;
static float vbuf[G_MAXV][2][CH];
static float gn[NP];                                       /* the parameters, normalized, with the global modulations */
static int newest = 0;
static float bend, wheel;
static uint8_t sustain;
static float shared_scan, shared_dir = 1.0f, shared_jump;

/* --------------------------------------------------------------------------------------------- the clock --- */
static float beat_ph;                                      /* within the beat, 0..1 */
static uint32_t beat_n;                                    /* whole beats */
static float free_ph;
static uint32_t free_n;                                    /* a clock that always runs (sync grains, the arp) */
static float ext_tempo;                                    /* MIDI clock: the measured tempo, 0 none */
static uint32_t ext_last, ext_ticks_ago = 0xFFFFFFFFu, sample_count;
static uint8_t was_running;

/* ---------------------------------------------------------------------------------------------------- LFO --- */
static struct { float ph, val, prev, tgt; } lfo[2];

/* ---------------------------------------------------------------------------------------------- arpeggio --- */
typedef struct { int32_t key, note, slice; float vel; } held_t;
static held_t held[16];
static int nheld;
static int arp_idx, arp_dir = 1, arp_oct;
static int32_t arp_key = -1;                               /* the voice key now sounding (300 + n) */
static float arp_off_at = -1.0f;                           /* free beat at which it ends */
static float arp_last_beat = -1.0f;

/* --------------------------------------------------------------------------------------------- the echo --- */
static int16_t dly[2][DLY_LEN] PLAT_POOL;
static uint32_t dly_w;
static float dly_cur = 0.25f * G_SR, dly_fl, dly_fr;

/* ------------------------------------------------------------------------------------------- the reverb --- */
#define RV_N 4
static const uint16_t RV_MAX[RV_N] = {3203, 3637, 4111, 4507};
static int16_t rv0[3203] PLAT_POOL, rv1[3637] PLAT_POOL, rv2[4111] PLAT_POOL, rv3[4507] PLAT_POOL;
static int16_t *const RV[RV_N] = {rv0, rv1, rv2, rv3};
static uint32_t rv_w[RV_N];
static float rv_lp[RV_N];
#define AP_N 4
static const uint16_t AP_LEN[AP_N] = {142, 379, 107, 277};
static float ap0[142], ap1[379], ap2[107], ap3[277];
static float *const AP[AP_N] = {ap0, ap1, ap2, ap3};
static uint32_t ap_w[AP_N];

/* --------------------------------------------------------------------------------------------- the rest --- */
static float crush_hold[2], crush_cnt;
static float out_peak;
static uint32_t cap_w, cap_acc_n;
static float cap_acc;
static uint8_t cap_on, cap_loop, cap_full;

/* ======================================================================================================== */
static void src_init(void)
{
    uint32_t i, k;
    for (i = 0; i < G_NBUILTIN && i < SND_COUNT; i++) {
        G_SRC[i].u8 = SND_BANK[i].data;
        G_SRC[i].s16 = 0;
        G_SRC[i].len = SND_BANK[i].len;
        G_SRC[i].rate = SND_RATE;
        for (k = 0; k < 9u && SND_BANK[i].name[k]; k++)
            G_SRC[i].name[k] = SND_BANK[i].name[k];
        G_SRC[i].name[k] = 0;
    }
    G_SRC[G_SRC_CAPT].u8 = 0;
    G_SRC[G_SRC_CAPT].s16 = G_CAP;
    G_SRC[G_SRC_CAPT].len = 0;
    G_SRC[G_SRC_CAPT].rate = G_CAP_RATE;
    {
        static const char CN[] = "CAPTURE";
        for (k = 0; CN[k]; k++)
            G_SRC[G_SRC_CAPT].name[k] = CN[k];
        G_SRC[G_SRC_CAPT].name[k] = 0;
    }
    /* user slots: the UI fills them from the flash (ui/store.c) */
}

void eng_init(void)
{
    uint32_t i;
    src_init();
    for (i = 0; i < G_MAXV; i++) {
        V[i].dir = 1.0f;
        V[i].key = -1;
    }
    for (i = 0; i < 2; i++)
        lfo[i].ph = 0.25f * (float)i;
    win_init();
    win_make(0.33f, 0.0f);
}

void eng_panic(void)
{
    uint32_t i;
    for (i = 0; i < G_MAXG; i++)
        gr[i].on = 0;
    for (i = 0; i < G_MAXV; i++) {
        V[i].stage = V[i].mstage = 0;
        V[i].lev = V[i].mlev = 0.0f;
        V[i].gate = 0;
    }
    nheld = 0;
    arp_key = -1;
}

void eng_overload(void)
{
    if (maxg > 16u)
        maxg -= 4u;
}

/* ================================================================================================ voices === */
static int nvoices(void) { return clamp(G.v[P_VOICES], 1, G_MAXV); }

static void voice_start(voice_t *v, int32_t key, int32_t note, int32_t slice, float vel, int legato)
{
    int retrig = G.v[P_RETRIG];
    float tgt;
    v->key = key;
    v->note = note;
    v->slice = slice;
    v->slice_on = slice >= 0;
    v->vel = vel;
    v->pres = 0.0f;
    v->alea = randb();
    v->age = ++vage;
    v->gate = 1;
    v->sustained = 0;
    tgt = slice >= 0 ? 0.0f : (float)(note - 60);
    v->pitch_tgt = tgt;
    if (!legato) {
        if (pval(P_PORTA, pnorm(P_PORTA)) <= 0.0f || V[newest].stage == 0)
            v->pitch = tgt;
        else
            v->pitch = V[newest].pitch;               /* glide from the last note */
        v->stage = 1;
        v->mstage = 1;
        v->mlev = 0.0f;
        v->next = 0.0f;
        if (retrig == 0 || slice >= 0) {
            v->scan = 0.0f;
            v->dir = 1.0f;
        }
        v->stut_on = 0;
        if (v->lev < 0.001f)
            v->pos_sm = -1.0f;                           /* (set to the knob at the first chunk) */
        if (G.v[P_L1MODE] == 1 || G.v[P_L2MODE] == 1) {
            int i, any = 0;
            for (i = 0; i < G_MAXV; i++)
                if (V[i].gate && &V[i] != v)
                    any = 1;
            if (!any) {
                if (G.v[P_L1MODE] == 1)
                    lfo[0].ph = 0.0f;
                if (G.v[P_L2MODE] == 1)
                    lfo[1].ph = 0.0f;
            }
        }
    }
    newest = (int)(v - V);
}

static void note_on(int32_t key, int32_t note, int32_t slice, float vel)
{
    int n = nvoices(), i, best = -1;
    uint32_t best_age = 0xFFFFFFFFu;
    if (n == 1) {                                        /* mono: legato, the newest note wins */
        voice_t *v = &V[0];
        voice_start(v, key, note, slice, vel, v->gate && v->stage && slice < 0 && !v->slice_on);
        return;
    }
    for (i = 0; i < n; i++)                              /* the same key again */
        if (V[i].key == key && V[i].stage) {
            best = i;
            break;
        }
    if (best < 0)
        for (i = 0; i < n; i++)                          /* a free voice */
            if (!V[i].stage) {
                best = i;
                break;
            }
    if (best < 0) {                                      /* a released one, the quietest */
        float lo = 9.0f;
        for (i = 0; i < n; i++)
            if (!V[i].gate && V[i].lev < lo) {
                lo = V[i].lev;
                best = i;
            }
    }
    if (best < 0)                                        /* the oldest */
        for (i = 0; i < n; i++)
            if (V[i].age < best_age) {
                best_age = V[i].age;
                best = i;
            }
    voice_start(&V[best], key, note, slice, vel, 0);
}

static void note_off(int32_t key)
{
    int i;
    for (i = 0; i < G_MAXV; i++)
        if (V[i].key == key && V[i].gate) {
            if (sustain)
                V[i].sustained = 1;
            else
                V[i].gate = 0;
            if (G.v[P_VOICES] == 1 && nheld > 0 && !G.v[P_ARP]) {   /* mono: back to a note still held */
                held_t *h = &held[nheld - 1];
                if (h->key != key)
                    voice_start(&V[i], h->key, h->note, h->slice, h->vel, h->slice < 0);
            }
        }
}

/* the keys held, in order (the arpeggio and mono's last-note priority) */
static void held_add(int32_t key, int32_t note, int32_t slice, float vel)
{
    int i;
    for (i = 0; i < nheld; i++)
        if (held[i].key == key)
            return;
    if (nheld >= 16)
        return;
    held[nheld].key = key;
    held[nheld].note = note;
    held[nheld].slice = slice;
    held[nheld].vel = vel;
    nheld++;
}
static void held_remove(int32_t key)
{
    int i, j;
    for (i = 0; i < nheld; i++)
        if (held[i].key == key) {
            for (j = i; j < nheld - 1; j++)
                held[j] = held[j + 1];
            nheld--;
            return;
        }
}

static void take_events(void)
{
    while (evr != evw) {
        uint32_t r = evr % QN;
        uint8_t t = evq[r].t;
        int32_t a = evq[r].a, b = evq[r].b;
        evr++;
        switch (t) {
        case EV_NOTE_ON: {                               /* a: key; b: note | vel << 8 | (slice + 1) << 16 */
            int32_t note = b & 0xFF, slice = ((b >> 16) & 0xFF) - 1;
            float vel = (float)((b >> 8) & 0x7F) / 127.0f;
            held_add(a, note, slice, vel);
            if (!G.v[P_ARP])
                note_on(a, note, slice, vel);
            break;
        }
        case EV_NOTE_OFF:
            held_remove(a);
            if (!G.v[P_ARP])
                note_off(a);
            break;
        case EV_ALL_OFF: {
            int i;
            nheld = 0;
            for (i = 0; i < G_MAXV; i++)
                V[i].gate = 0;
            arp_key = -1;
            break;
        }
        case EV_PRESS: {
            int i;
            for (i = 0; i < G_MAXV; i++)
                if (a < 0 || V[i].key == a)
                    V[i].pres = (float)b / 127.0f;
            break;
        }
        case EV_BEND: bend = (float)a / 8192.0f; break;
        case EV_WHEEL: wheel = (float)a / 127.0f; break;
        case EV_SUSTAIN:
            sustain = a != 0;
            if (!sustain) {
                int i;
                for (i = 0; i < G_MAXV; i++)
                    if (V[i].sustained) {
                        V[i].sustained = 0;
                        V[i].gate = 0;
                    }
            }
            break;
        case EV_CLOCK_START:
        case EV_MIDI_START:
            beat_ph = 0.0f;
            beat_n = 0;
            GP.running = 1;
            break;
        case EV_CLOCK_STOP:
        case EV_MIDI_STOP:
            GP.running = 0;
            break;
        case EV_MIDI_TICK: {
            uint32_t d = sample_count - ext_last;
            ext_last = sample_count;
            if (d > 200u && d < 4000u) {                 /* 24 a beat: 27..330 bpm */
                float bpm = 60.0f * (float)G_SR / ((float)d * 24.0f);
                ext_tempo = ext_tempo > 0.0f ? ext_tempo + (bpm - ext_tempo) * 0.08f : bpm;
            }
            ext_ticks_ago = 0;
            break;
        }
        case EV_CAPTURE:                                 /* a: 1 start, 0 stop; b: loop */
            if (a) {
                cap_on = 1;
                cap_loop = (uint8_t)b;
                cap_w = 0;
                cap_acc = 0.0f;
                cap_acc_n = 0;
                cap_full = 0;
                G_SRC[G_SRC_CAPT].len = 0;
            } else {
                cap_on = 0;
            }
            break;
        case EV_LFO_SYNC:
            lfo[0].ph = lfo[1].ph = 0.0f;
            break;
        }
    }
}

/* ================================================================================================ LFOs ===== */
static float lfo_shape(int i, int shape, float ph)
{
    switch (shape) {
    case 0: return fm_sin_turns(ph);
    case 1: return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
    case 2: return 2.0f * ph - 1.0f;
    case 3: return 1.0f - 2.0f * ph;
    case 4: return ph < 0.5f ? 1.0f : -1.0f;
    case 5: return lfo[i].tgt;                          /* sample and hold */
    default: {                                          /* drift: smooth random */
        float s = 0.5f - 0.5f * fm_sin_turns(ph + 0.25f);
        return lerpf(lfo[i].prev, lfo[i].tgt, s);
    }
    }
}

static void lfo_run(uint32_t n, float tempo, float beat)
{
    int i;
    for (i = 0; i < 2; i++) {
        int pr = i ? P_L2RATE : P_L1RATE, ps = i ? P_L2SHAPE : P_L1SHAPE, py = i ? P_L2SYNC : P_L1SYNC;
        float old = lfo[i].ph;
        if (G.v[py]) {                                  /* tempo: locked to the clock */
            int d = clamp((int32_t)(gn[pr] * 10.99f), 0, 10);
            float cyc = DIV_LFO[d], b = beat / cyc;
            lfo[i].ph = b - fm_floorf(b);
        } else {
            lfo[i].ph += pval(pr, gn[pr]) * (float)n / (float)G_SR;
            if (lfo[i].ph >= 1.0f)
                lfo[i].ph -= fm_floorf(lfo[i].ph);
        }
        if (lfo[i].ph < old) {                          /* a new cycle: the random shapes move on */
            lfo[i].prev = lfo[i].tgt;
            lfo[i].tgt = randb();
        }
        lfo[i].val = lfo_shape(i, G.v[ps], lfo[i].ph);
    }
    (void)tempo;
}

/* ========================================================================================= modulations ===== */
static void params_chunk(float beat_lane)
{
    int p, m;
    float bars = (float)(1 << clamp(G.v[P_GESTLEN], 0, 3));
    for (p = 0; p < NP; p++)
        gn[p] = pnorm(p);
    if (GP.running) {                                   /* the gestures, over the base */
        float lp = beat_lane / (bars * 4.0f);
        int step = (int)((lp - fm_floorf(lp)) * (float)G_LANE_STEPS);
        int l;
        step = clamp(step, 0, G_LANE_STEPS - 1);
        GV.lane_step = (uint16_t)step;
        for (l = 0; l < G_NLANE; l++)
            if (G_LANE[l].on && G_LANE[l].p < NP)
                gn[G_LANE[l].p] = (float)G_LANE[l].v[step] * 0.001f;
    }
    for (m = 0; m < G_NMOD; m++) {
        gmod_t *s = &G.mod[m];
        float sv;
        if (s->dst >= NP || !s->depth || MS_PER_VOICE(s->src))
            continue;
        sv = s->src == MS_LFO1 ? lfo[0].val : s->src == MS_LFO2 ? lfo[1].val : wheel;
        gn[s->dst] += (float)s->depth * 0.01f * sv;
    }
}

static float voice_src(const voice_t *v, int src)
{
    switch (src) {
    case MS_ENV: return v->mlev;
    case MS_VEL: return v->vel;
    case MS_PRES: return v->pres;
    case MS_ALEA: return v->alea;
    default: return 0.0f;
    }
}

static void voice_params(voice_t *v)
{
    int p, m;
    for (p = 0; p < NP; p++)
        v->vn[p] = gn[p];
    for (m = 0; m < G_NMOD; m++) {
        gmod_t *s = &G.mod[m];
        if (s->dst >= NP || !s->depth || !MS_PER_VOICE(s->src))
            continue;
        v->vn[s->dst] += (float)s->depth * 0.01f * voice_src(v, s->src);
    }
}

/* ======================================================================================= envelopes ======== */
static void env_step(int *stage, float *lev, int gate, float a, float d, float s, float r, uint32_t n)
{
    float fn = (float)n;
    if (*stage == 0)
        return;
    if (!gate && *stage < 4)
        *stage = 4;
    switch (*stage) {
    case 1:
        *lev += fn / (a * (float)G_SR);
        if (*lev >= 1.0f) {
            *lev = 1.0f;
            *stage = 2;
        }
        break;
    case 2:
        *lev = s + (*lev - s) * fm_expf(-4.6f * fn / (d * (float)G_SR));
        if (*lev - s < 0.001f)
            *stage = 3;
        break;
    case 3:
        *lev = s;
        break;
    case 4:
        *lev *= fm_expf(-6.9f * fn / (r * (float)G_SR));
        if (*lev < 0.0003f) {
            *lev = 0.0f;
            *stage = 0;
        }
        break;
    }
}

/* ===================================================================================== scale, quantize ===== */
static int scale_snap(int note, int scale, int tonic)
{
    uint16_t mask = G_SCALE_MASK[clamp(scale, 0, G_NSCALE - 1)];
    int k;
    for (k = 0; k < 12; k++) {                          /* the nearest note of the scale, down first */
        int d = (k + 1) / 2 * ((k & 1) ? -1 : 1);
        int deg = ((note + d - tonic) % 12 + 12) % 12;
        if (mask >> deg & 1u)
            return note + d;
    }
    return note;
}

static float spread_quant(float r, int q, int base)
{
    static const int8_t MAJ[] = {-24, -20, -17, -12, -8, -5, 0, 4, 7, 12, 16, 19, 24};
    static const int8_t MIN[] = {-24, -21, -17, -12, -9, -5, 0, 3, 7, 12, 15, 19, 24};
    static const int8_t FIF[] = {-24, -17, -12, -5, 0, 7, 12, 19, 24};
    const int8_t *t = 0;
    int n = 0, i, best = 0;
    float bd = 99.0f;
    switch (q) {
    case 0: return r;
    case 1: return (float)(int)(r + (r < 0 ? -0.5f : 0.5f));
    case 2: return 12.0f * (float)(int)(r / 12.0f + (r < 0 ? -0.5f : 0.5f));
    case 3: t = FIF; n = sizeof FIF; break;
    case 4: t = MAJ; n = sizeof MAJ; break;
    case 5: t = MIN; n = sizeof MIN; break;
    default: {
        int ri = (int)(r + (r < 0 ? -0.5f : 0.5f));
        return (float)(scale_snap(base + ri, G.v[P_SCALE], G.v[P_TONIC]) - base);
    }
    }
    for (i = 0; i < n; i++) {
        float d = fm_fabsf((float)t[i] - r);
        if (d < bd) {
            bd = d;
            best = t[i];
        }
    }
    return (float)best;
}

/* ======================================================================================== grain spawn ====== */
static void grain_spawn(voice_t *v, uint32_t offset, float pitch_base, const gsrc_t *s)
{
    grain_t *g = 0;
    uint32_t i, lim = maxg;
    float size_s, ratio, idx, semis, spray, p, amp, c, th, dens, overlap, glide;
    int rev;
    uint32_t pads = GP.pads;
    for (i = 0; i < lim; i++)
        if (!gr[i].on) {
            g = &gr[i];
            break;
        }
    if (!g) {
        if (GV.grains_dropped < 255u)
            GV.grains_dropped++;
        return;
    }
    size_s = pval(P_SIZE, v->vn[P_SIZE]);
    if (pads & 1u << PAD_POUSS)
        size_s *= 0.25f;
    if (v->stut_on)
        size_s = fm_minf(size_s, 0.12f);
    spray = clamp01(v->vn[P_SPRAY] + ((pads & 1u << PAD_DERIVE) ? 0.45f : 0.0f));
    if (v->stut_on)
        p = v->stutter;
    else
        p = v->pos_eff + spray * (g_randf() - 0.5f);
    p -= fm_floorf(p);
    idx = (float)v->ws + p * (float)v->wl;
    semis = pitch_base + spread_quant(randb() * pval(P_SPREAD, v->vn[P_SPREAD]), G.v[P_QUANT], v->note);
    if (g_randf() < v->vn[P_SHIMMER] * 0.6f)
        semis += g_randf() < 0.25f ? 19.0f : 12.0f;
    ratio = (float)s->rate / (float)G_SR * semis_ratio(semis);
    rev = g_randf() < v->vn[P_REV] || (pads & 1u << PAD_ENVERS);
    dens = pval(P_DENS, v->vn[P_DENS]);
    overlap = dens * size_s;
    amp = overlap > 1.0f ? fm_rsqrtf(overlap) : 1.0f;
    amp *= 1.0f - v->vn[P_JITTER] * g_randf();
    c = (g_randf() - 0.5f) * (clamp01(v->vn[P_STEREO]) + ((pads & 1u << PAD_DERIVE) ? 1.0f : 0.0f));
    c = fm_clampf(c, -0.5f, 0.5f);
    th = (0.5f + c) * (PI_F * 0.5f);
    glide = pval(P_GLIDE, v->vn[P_GLIDE]);
    {
        float chunks = size_s * (float)G_SR / 16.0f;
        g->rmul = glide != 0.0f && chunks > 1.0f ? semis_ratio(glide / chunks) : 1.0f;
    }
    g->u8 = s->u8;
    g->s16 = s->s16;
    g->len1 = (int32_t)s->len - 1;
    g->ipos = (int32_t)idx;
    if (g->ipos > g->len1 - 1)
        g->ipos = g->len1 - 1;
    g->frac = (uint32_t)((idx - (float)(int32_t)idx) * 4294967040.0f);
    g->rate = rev ? -ratio : ratio;
    {
        float frames = size_s * (float)G_SR;
        if (frames < 32.0f)
            frames = 32.0f;
        g->winc = (uint32_t)(4294967296.0f / frames);
    }
    g->wph = 0;
    g->amp = amp;
    g->gl = fm_cosf(th);
    g->gr = fm_sinf(th);
    g->delay = offset;
    g->voice = (uint8_t)(v - V);
    g->rev = (uint8_t)rev;
    g->pan = (int8_t)clamp((int32_t)(c * 128.0f), -64, 64);
    g->ws = v->ws;
    g->wl = v->wl;
    g->semis = (int8_t)clamp((int32_t)(semis - pitch_base + (semis > pitch_base ? 0.5f : -0.5f)), -60, 60);
    g->sizec = (uint8_t)clamp((int32_t)(fm_log2f(size_s * 200.0f) * 28.0f), 0, 255);
    g->on = 1;
}

/* the grain's frames into its voice's buffer */
static void grain_run(grain_t *g, uint32_t n)
{
    float *ol = vbuf[g->voice][0], *or_ = vbuf[g->voice][1];
    uint32_t i = g->delay, wph = g->wph, winc = g->winc, frac = g->frac;
    int32_t ipos = g->ipos, len1 = g->len1;
    const uint8_t *u8 = g->u8;
    const int16_t *s16 = g->s16;
    float gl = g->amp * g->gl, grr = g->amp * g->gr;
    if (i >= n) {
        g->delay -= n;
        return;
    }
    g->delay = 0;
    while (i < n) {
        uint32_t end = i + 16u, finc;
        int32_t iinc;
        float rate = g->rate, fi;
        if (end > n)
            end = n;
        /* the rate as a whole and a Q32 fraction (signed: the whole part floors) */
        fi = fm_floorf(rate);
        iinc = (int32_t)fi;
        finc = (uint32_t)((rate - fi) * 4294967040.0f);
        for (; i < end; i++) {
            uint32_t wi = wph >> 23, nf;
            float wf = (float)(wph & 0x7FFFFFu) * (1.0f / 8388608.0f);
            float w = win[wi] + (win[wi + 1] - win[wi]) * wf;
            float s0, s1, x, f = (float)(frac >> 8) * (1.0f / 16777216.0f);
            int32_t i1 = ipos + 1;
            if (i1 > len1)
                i1 = 0;
            if (u8) {
                s0 = SND_ULAW[u8[ipos]];
                s1 = SND_ULAW[u8[i1]];
            } else {
                s0 = (float)s16[ipos] * (1.0f / 32768.0f);
                s1 = (float)s16[i1] * (1.0f / 32768.0f);
            }
            x = (s0 + (s1 - s0) * f) * w;
            ol[i] += x * gl;
            or_[i] += x * grr;
            nf = frac + finc;
            ipos += iinc + (nf < frac ? 1 : 0);
            frac = nf;
            if (ipos >= len1)
                ipos -= len1;
            else if (ipos < 0)
                ipos += len1;
            if (wph + winc < wph) {                      /* the window is over */
                g->on = 0;
                return;
            }
            wph += winc;
        }
        g->rate = rate * g->rmul;
    }
    g->wph = wph;
    g->frac = frac;
    g->ipos = ipos;
}

/* ===================================================================================== voice per chunk ===== */
static float tempo_now(void)
{
    if (ext_tempo > 0.0f && ext_ticks_ago < (uint32_t)G_SR)
        return ext_tempo;
    return pval(P_TEMPO, pnorm(P_TEMPO));
}

static void voice_scan(voice_t *v, const gsrc_t *s, uint32_t n, int shared)
{
    float sp = pval(P_SPEED, v->vn[P_SPEED]);
    float *scan = shared ? &shared_scan : &v->scan, *dir = shared ? &shared_dir : &v->dir;
    float *jump = shared ? &shared_jump : &v->jump_t;
    float d;
    int mode = G.v[P_SCAN];
    if (GP.freeze || (GP.pads & 1u << PAD_GEL) || v->wl < 2)
        return;
    d = sp * (float)n * ((float)s->rate / (float)G_SR) / (float)v->wl;
    switch (mode) {
    case 0:                                             /* loop */
        *scan += d;
        *scan -= fm_floorf(*scan);
        break;
    case 1:                                             /* back and forth */
        *scan += d * *dir;
        if (*scan > 1.0f) {
            *scan = 2.0f - *scan;
            *dir = -*dir;
        } else if (*scan < 0.0f) {
            *scan = -*scan;
            *dir = -*dir;
        }
        break;
    case 2:                                             /* once */
        *scan = clamp01(*scan + d);
        break;
    case 3:                                             /* drunk */
        *scan += fm_fabsf(sp) * (float)n / (float)G_SR * randb() * 1.5f;
        *scan -= fm_floorf(*scan);
        break;
    default:                                            /* jumps */
        *jump -= (float)n / (float)G_SR;
        if (*jump <= 0.0f) {
            *scan = g_randf();
            *jump = 1.0f / fm_maxf(0.08f, fm_fabsf(sp) * 2.0f);
        }
        break;
    }
}

static void voice_chunk(voice_t *v, uint32_t n, float beat0, float bps)
{
    const gsrc_t *s;
    int si, shared = G.v[P_RETRIG] == 2 && !v->slice_on;
    float pos, start, len, pitch_base, slew, porta;
    uint32_t pads = GP.pads;
    voice_params(v);
    si = clamp(G.v[P_SAMPLE], 0, G_NSRC - 1);
    s = &G_SRC[si];
    if (s->len < 64u)
        return;
    start = clamp01(v->vn[P_START]);
    len = fm_maxf(0.01f, clamp01(v->vn[P_LEN]));
    v->ws = (int32_t)(start * (float)s->len);
    v->wl = (int32_t)(len * (float)s->len);
    if (v->ws > (int32_t)s->len - 64)
        v->ws = (int32_t)s->len - 64;
    if (v->ws + v->wl > (int32_t)s->len)
        v->wl = (int32_t)s->len - v->ws;
    if (v->wl < 32)
        v->wl = 32;
    voice_scan(v, s, n, shared);
    /* the position: the knob (slewed), or the TAPIS slice, then the scan and the key */
    pos = v->slice_on ? ((float)v->slice + 0.02f) / 16.0f : clamp01(v->vn[P_POS]);
    slew = v->vn[P_SLEW];
    if (slew < 0.01f || v->slice_on || v->pos_sm < 0.0f)
        v->pos_sm = pos;
    else
        v->pos_sm += (pos - v->pos_sm) * fm_minf(1.0f, (float)n / (slew * slew * 2.0f * (float)G_SR));
    pos = v->pos_sm + (shared ? shared_scan : v->scan);
    if (!v->slice_on)
        pos += pval(P_KEYPOS, v->vn[P_KEYPOS]) * (float)(v->note - 60) / 24.0f;
    v->pos_eff = pos - fm_floorf(pos);
    /* BEGAIE: the position held, the grains on the sixteenths */
    if (pads & 1u << PAD_BEGAIE) {
        if (!v->stut_on) {
            v->stut_on = 1;
            v->stutter = v->pos_eff;
        }
    } else {
        v->stut_on = 0;
    }
    /* the pitch: the note (glide), the knobs, the pads, the bend */
    porta = pval(P_PORTA, gn[P_PORTA]);
    if (porta <= 0.0f)
        v->pitch = v->pitch_tgt;
    else
        v->pitch += (v->pitch_tgt - v->pitch) * fm_minf(1.0f, (float)n / (porta * (float)G_SR));
    pitch_base = v->pitch + 12.0f * (float)GP.octave + pval(P_PITCH, v->vn[P_PITCH]) +
                 pval(P_FINE, v->vn[P_FINE]) * 0.01f + bend * 2.0f;
    if (pads & 1u << PAD_OCTDN)
        pitch_base -= 12.0f;
    if (pads & 1u << PAD_OCTUP)
        pitch_base += 12.0f;
    if (pads & 1u << PAD_QUINTE)
        pitch_base += 7.0f;
    /* grains: when, then each */
    if (v->stage || v->gate) {
        int timing = G.v[P_TIMING];
        float dens = pval(P_DENS, v->vn[P_DENS]);
        if (pads & 1u << PAD_NUEE)
            dens *= 4.0f;
        if (timing == 2 || v->stut_on) {                  /* on the clock's divisions */
            int di = clamp((int32_t)(v->vn[P_DENS] * 9.99f), 0, 9);
            float div = v->stut_on ? 0.25f : DIV_SYNC_GRAIN[di], b0 = beat0 / div, b1 = (beat0 + bps * (float)n) / div;
            float k = fm_floorf(b0) + 1.0f;
            if (v->next <= 0.0f && v->stage == 1 && v->lev == 0.0f) {
                grain_spawn(v, 0, pitch_base, s);         /* a note starts with a grain */
                v->next = 1.0f;
            }
            while (k <= b1) {
                float off = (k * div - beat0) / bps;
                grain_spawn(v, (uint32_t)clamp((int32_t)off, 0, (int32_t)n - 1), pitch_base, s);
                k += 1.0f;
            }
        } else {
            float iv = (float)G_SR / dens;
            while (v->next < (float)n) {
                grain_spawn(v, (uint32_t)(v->next > 0.0f ? v->next : 0.0f), pitch_base, s);
                if (timing == 1) {                       /* random: exponential gaps, bounded */
                    float u = fm_maxf(0.02f, g_randf());
                    v->next += iv * fm_clampf(-fm_logf(u), 0.08f, 4.0f);
                } else {
                    v->next += iv;
                }
            }
            v->next -= (float)n;
        }
    }
}

/* the voice's buffer: the filter, the envelope, into the mix */
static void voice_out(voice_t *v, uint32_t n, float *ml, float *mr)
{
    float *bl = vbuf[v - V][0], *br = vbuf[v - V][1];
    float a = pval(P_AATK, v->vn[P_AATK]), d = pval(P_ADEC, v->vn[P_ADEC]), su = clamp01(v->vn[P_ASUS]);
    float r = pval(P_AREL, v->vn[P_AREL]);
    float l0 = v->lev, l1, g0, g1, gstep, velg;
    float cut = pval(P_CUT, v->vn[P_CUT]), res = clamp01(v->vn[P_RES]), ft = clamp01(v->vn[P_FTYPE]);
    float drive = clamp01(v->vn[P_DRIVE]);
    uint32_t i;
    env_step(&v->stage, &v->lev, v->gate, a, d, su, r, n);
    env_step(&v->mstage, &v->mlev, v->gate, pval(P_MATK, v->vn[P_MATK]), pval(P_MDEC, v->vn[P_MDEC]),
             clamp01(v->vn[P_MSUS]), pval(P_MREL, v->vn[P_MREL]), n);
    if (v->mstage == 0 && v->stage != 0 && v->gate == 0)
        v->mlev = 0.0f;
    l1 = v->lev;
    velg = 1.0f - pnorm(P_VELO) + pnorm(P_VELO) * v->vel;
    g0 = l0 * velg;
    g1 = l1 * velg;
    gstep = (g1 - g0) / (float)n;
    if (GP.pads & 1u << PAD_SOMBRE)
        cut *= 0.12f;
    if (cut < 17000.0f || ft > 0.01f || drive > 0.01f) {
        /* a state-variable filter (topology-preserving), LP -> BP -> HP */
        float gg = fm_tanf(PI_F * fm_minf(cut, 19000.0f) / (float)G_SR), k = 2.0f - 1.96f * res;
        float a1 = 1.0f / (1.0f + gg * (gg + k)), a2 = gg * a1, a3 = gg * a2;
        float mlp, mbp, mhp, dg = 1.0f + 7.0f * drive * drive, dn = 1.0f / (1.0f + 1.5f * drive);
        int c;
        if (ft < 0.5f) {
            mlp = 1.0f - 2.0f * ft;
            mbp = 2.0f * ft;
            mhp = 0.0f;
        } else {
            mlp = 0.0f;
            mbp = 2.0f - 2.0f * ft;
            mhp = 2.0f * ft - 1.0f;
        }
        for (c = 0; c < 2; c++) {
            float *b = c ? br : bl, ic1 = v->ic[c][0], ic2 = v->ic[c][1];
            for (i = 0; i < n; i++) {
                float x = b[i], v1, v2, v3;
                if (drive > 0.01f)
                    x = fm_tanhf(x * dg) * dn;
                v3 = x - ic2;
                v1 = a1 * ic1 + a2 * v3;
                v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;
                b[i] = mlp * v2 + mbp * v1 + mhp * (x - k * v1 - v2);
            }
            v->ic[c][0] = fm_flush(ic1);
            v->ic[c][1] = fm_flush(ic2);
        }
    }
    for (i = 0; i < n; i++) {
        ml[i] += bl[i] * g0;
        mr[i] += br[i] * g0;
        g0 += gstep;
    }
}

/* ============================================================================================= the arp ===== */
static void arp_tick(float beat_free, float bps)
{
    int mode = G.v[P_ARP], octs = clamp(G.v[P_ARPOCT], 1, 4), total, i, pick;
    float div = DIV_ARP[clamp(G.v[P_ARPDIV], 0, 5)], b;
    if (!mode) {
        if (arp_key >= 0) {
            note_off(arp_key);
            arp_key = -1;
        }
        return;
    }
    if (arp_key >= 0 && arp_off_at >= 0.0f && beat_free >= arp_off_at) {
        note_off(arp_key);
        arp_key = -1;
        arp_off_at = -1.0f;
    }
    b = fm_floorf(beat_free / div);
    if (b == arp_last_beat)
        return;
    arp_last_beat = b;
    if (nheld == 0) {
        arp_idx = 0;
        arp_dir = 1;
        return;
    }
    total = nheld * octs;
    switch (mode) {
    case 1: pick = arp_idx % total; arp_idx++; break;
    case 2: pick = total - 1 - arp_idx % total; arp_idx++; break;
    case 3:
        if (total == 1) {
            pick = 0;
            break;
        }
        pick = arp_idx;
        arp_idx += arp_dir;
        if (arp_idx >= total - 1 || arp_idx <= 0)
            arp_dir = -arp_dir;
        arp_idx = clamp(arp_idx, 0, total - 1);
        break;
    case 4: pick = (int)(g_rand() % (uint32_t)total); break;
    default: pick = arp_idx % total; arp_idx++; break;   /* as played: the held order */
    }
    {
        /* sorted by pitch for up / down (as played for ORDRE) */
        held_t h[16];
        int a, c;
        for (i = 0; i < nheld; i++)
            h[i] = held[i];
        if (mode != 5)
            for (a = 1; a < nheld; a++)
                for (c = a; c > 0 && (h[c].note + h[c].slice * 100) < (h[c - 1].note + h[c - 1].slice * 100); c--) {
                    held_t t = h[c];
                    h[c] = h[c - 1];
                    h[c - 1] = t;
                }
        arp_oct = pick / nheld;
        i = pick % nheld;
        if (arp_key >= 0)
            note_off(arp_key);
        arp_key = 300 + pick;
        note_on(arp_key, h[i].note + 12 * arp_oct, h[i].slice, h[i].vel);
        if (h[i].slice >= 0)
            V[newest].pitch_tgt = V[newest].pitch = 12.0f * (float)arp_oct;
        arp_off_at = b * div + div * (float)clamp(G.v[P_ARPGATE], 5, 100) * 0.01f;
    }
    (void)bps;
}

/* ============================================================================================ effects ====== */
static inline float softclip(float x)                     /* clean below 0.7, then a knee to 1 */
{
    float a = fm_fabsf(x);
    if (a < 0.7f)
        return x;
    a = 0.7f + 0.3f * fm_tanhf((a - 0.7f) * 3.3333f);
    return x < 0.0f ? -a : a;
}

static void fx_chunk(float *l, float *r, uint32_t n, float tempo)
{
    uint32_t i;
    float bits = gn[P_BITS], down = pval(P_DOWN, gn[P_DOWN]);
    float dmix = clamp01(gn[P_DMIX]), dfb = pval(P_DFB, gn[P_DFB]), dtone = clamp01(gn[P_DTONE]);
    float rmix = clamp01(gn[P_RMIX]), rsize = clamp01(gn[P_RSIZE]), rt = pval(P_RDEC, gn[P_RDEC]);
    float rdamp = clamp01(gn[P_RDAMP]), width = pval(P_WIDTH, gn[P_WIDTH]), level = clamp01(gn[P_LEVEL]) * 2.0f;
    float dtgt, tcoef, rg[RV_N], dcoef;
    uint32_t rl[RV_N], k;
    int di = clamp(G.v[P_DTIME], 0, 12);
    if (GP.pads & 1u << PAD_ECHO) {
        dmix = fm_maxf(dmix, 0.75f);
        dfb = fm_maxf(dfb, 0.82f);
    }
    dtgt = DIV_DELAY[di] * 60.0f / tempo * (float)G_SR;
    dtgt = fm_clampf(dtgt, 16.0f, (float)(DLY_LEN - 4u));
    tcoef = 0.08f + 0.9f * dtone * dtone;
    dcoef = 0.15f + 0.8f * (1.0f - rdamp);
    for (k = 0; k < RV_N; k++) {
        float len = (float)RV_MAX[k] * (0.28f + 0.72f * rsize);
        rl[k] = (uint32_t)len;
        rg[k] = rt > 30.0f ? 0.9995f : fm_exp2f(-3.3219f * 3.0f * len / (rt * (float)G_SR));   /* 10^(-3 len / RT60) */
    }
    for (i = 0; i < n; i++) {
        float xl = l[i], xr = r[i], dl, dr, wl, wr, rin, y[RV_N], h[RV_N], ol, orr;
        uint32_t rp, j;
        /* matter: bits and sample-and-hold */
        if (down > 1.01f) {
            crush_cnt += 1.0f;
            if (crush_cnt >= down) {
                crush_cnt -= down;
                crush_hold[0] = xl;
                crush_hold[1] = xr;
            }
            xl = crush_hold[0];
            xr = crush_hold[1];
        }
        if (bits > 0.005f) {
            float q = fm_exp2f(15.0f - 13.5f * bits);
            xl = fm_floorf(xl * q + 0.5f) / q;
            xr = fm_floorf(xr * q + 0.5f) / q;
        }
        /* the echo, ping-pong, its time gliding like tape */
        dly_cur += (dtgt - dly_cur) * 0.0004f;
        {
            float rpf = (float)dly_w - dly_cur;
            int32_t ri;
            float fr;
            if (rpf < 0.0f)
                rpf += (float)DLY_LEN;
            ri = (int32_t)rpf;
            fr = rpf - (float)ri;
            rp = (uint32_t)ri;
            j = rp + 1u >= DLY_LEN ? 0u : rp + 1u;
            dl = ((float)dly[0][rp] + ((float)dly[0][j] - (float)dly[0][rp]) * fr) * (1.0f / 16384.0f);
            dr = ((float)dly[1][rp] + ((float)dly[1][j] - (float)dly[1][rp]) * fr) * (1.0f / 16384.0f);
        }
        dly_fl += (dl - dly_fl) * tcoef;
        dly_fr += (dr - dly_fr) * tcoef;
        wl = softclip(0.5f * (xl + xr) + dfb * dly_fr);
        wr = softclip(dfb * dly_fl);
        dly[0][dly_w] = (int16_t)(wl * 16384.0f);
        dly[1][dly_w] = (int16_t)(wr * 16384.0f);
        if (++dly_w >= DLY_LEN)
            dly_w = 0;
        /* the reverb: diffusion, then a feedback delay network of four */
        rin = 0.5f * (xl + xr) + 0.4f * (dl + dr) * dmix;
        for (j = 0; j < 2u; j++) {
            float *b = AP[j], bo = b[ap_w[j]], v = rin + 0.6f * bo;
            b[ap_w[j]] = v;
            rin = bo - 0.6f * v;
            if (++ap_w[j] >= AP_LEN[j])
                ap_w[j] = 0;
        }
        for (j = 0; j < RV_N; j++) {
            uint32_t at = rv_w[j] >= rl[j] ? rv_w[j] - rl[j] : rv_w[j] + RV_MAX[j] - rl[j];
            y[j] = (float)RV[j][at] * (1.0f / 8192.0f);
        }
        h[0] = 0.5f * (y[0] + y[1] + y[2] + y[3]);
        h[1] = 0.5f * (y[0] - y[1] + y[2] - y[3]);
        h[2] = 0.5f * (y[0] + y[1] - y[2] - y[3]);
        h[3] = 0.5f * (y[0] - y[1] - y[2] + y[3]);
        for (j = 0; j < RV_N; j++) {
            float wv;
            rv_lp[j] += (h[j] - rv_lp[j]) * dcoef;
            rv_lp[j] = fm_flush(rv_lp[j]);
            wv = fm_clampf(rin * (j & 1u ? -0.5f : 0.5f) + rg[j] * rv_lp[j], -3.9f, 3.9f);
            RV[j][rv_w[j]] = (int16_t)(wv * 8192.0f);
            if (++rv_w[j] >= RV_MAX[j])
                rv_w[j] = 0;
        }
        ol = y[0] + 0.6f * y[2];
        orr = y[1] + 0.6f * y[3];
        for (j = 2; j < 4u; j++) {                       /* a little diffusion on each side */
            float *b = AP[j], bo = b[ap_w[j]], in = j == 2u ? ol : orr, v = in + 0.5f * bo;
            b[ap_w[j]] = v;
            if (j == 2u)
                ol = bo - 0.5f * v;
            else
                orr = bo - 0.5f * v;
            if (++ap_w[j] >= AP_LEN[j])
                ap_w[j] = 0;
        }
        xl = xl + dl * dmix + ol * rmix * 0.7f;
        xr = xr + dr * dmix + orr * rmix * 0.7f;
        {
            float m = 0.5f * (xl + xr), s = 0.5f * (xl - xr) * width;
            l[i] = (m + s) * level;
            r[i] = (m - s) * level;
        }
    }
}

/* ======================================================================================== the capture ===== */
static void capture_chunk(const float *l, const float *r, uint32_t n)
{
    uint32_t i;
    if (!cap_on)
        return;
    for (i = 0; i < n; i++) {
        cap_acc += 0.5f * (l[i] + r[i]);
        if (++cap_acc_n >= 2u) {
            float v = fm_clampf(cap_acc * 0.5f, -1.0f, 1.0f);
            G_CAP[cap_w] = (int16_t)(v * 32767.0f);
            cap_acc = 0.0f;
            cap_acc_n = 0;
            if (++cap_w >= G_CAP_LEN) {
                cap_full = 1;
                if (cap_loop) {
                    cap_w = 0;
                } else {
                    cap_on = 0;
                    break;
                }
            }
        }
    }
    G_SRC[G_SRC_CAPT].len = cap_full ? G_CAP_LEN : cap_w;
}

/* =========================================================================================== the render ===== */
static float mixl[CH], mixr[CH];
static float dc_x[2], dc_y[2];

static void view_update(void)
{
    uint32_t i, k = 0;
    int p, m;
    for (i = 0; i < G_MAXG; i++) {
        grain_t *g = &gr[i];
        float x;
        if (!g->on || g->delay)
            continue;
        x = (float)(g->ipos - g->ws) / (float)(g->wl > 0 ? g->wl : 1);
        if (x < 0.0f || x > 1.0f)
            x -= fm_floorf(x);
        GV.g[k].x = (uint16_t)(x * 65535.0f);
        GV.g[k].semis = g->semis;
        GV.g[k].size = g->sizec;
        GV.g[k].amp = (uint8_t)(win[g->wph >> 23] * 255.0f);
        GV.g[k].voice = g->voice;
        GV.g[k].rev = g->rev;
        GV.g[k].pan = g->pan;
        k++;
    }
    GV.n = (uint8_t)k;
    for (i = 0; i < G_MAXV; i++) {
        GV.vpos[i] = (uint16_t)(V[i].pos_eff * 65535.0f);
        GV.vlev[i] = V[i].stage ? (uint8_t)clamp((int32_t)(V[i].lev * 255.0f) + 1, 1, 255) : 0;
        GV.vnote[i] = (uint8_t)V[i].note;
    }
    for (p = 0; p < NP; p++)
        GV.pmod[p] = -1.0f;
    for (m = 0; m < G_NMOD; m++) {
        gmod_t *s = &G.mod[m];
        if (s->dst < NP && s->depth)
            GV.pmod[s->dst] = clamp01(V[newest].stage ? V[newest].vn[s->dst] : gn[s->dst]);
    }
    for (i = 0; i < G_NLANE; i++)
        if (G_LANE[i].on && G_LANE[i].p < NP && GP.running)
            GV.pmod[G_LANE[i].p] = clamp01(gn[G_LANE[i].p]);
    GV.lfo[0] = lfo[0].val;
    GV.lfo[1] = lfo[1].val;
    GV.beat_q16 = (uint32_t)(((float)beat_n + beat_ph) * 65536.0f);
    GV.cap_on = cap_on;
    GV.cap_len = G_SRC[G_SRC_CAPT].len;
    GV.seq++;
}

void eng_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t done = 0;
    float gain = (float)master_q12 * (1.0f / 4096.0f) * 4000000.0f;
    take_events();
    while (done < n) {
        uint32_t c = n - done > CH ? CH : n - done, i, j;
        float tempo = tempo_now(), bps = tempo / 60.0f / (float)G_SR;
        float beat0 = (float)beat_n + beat_ph, fbeat0 = (float)free_n + free_ph;
        float clock_beat = GP.running ? beat0 : fbeat0;
        if (GP.running && !was_running) {
            beat_ph = 0.0f;
            beat_n = 0;
            beat0 = 0.0f;
            clock_beat = 0.0f;
        }
        was_running = GP.running;
        lfo_run(c, tempo, clock_beat);
        params_chunk(beat0);
        {                                               /* the patch's window shape may be modulated */
            float wm = clamp01(gn[P_WIN]), sk = pval(P_SKEW, gn[P_SKEW]);
            win_make(wm, sk);
        }
        arp_tick(clock_beat, bps);
        for (j = 0; j < G_MAXV; j++)
            for (i = 0; i < c; i++)
                vbuf[j][0][i] = vbuf[j][1][i] = 0.0f;
        for (j = 0; j < G_MAXV; j++)
            if (V[j].stage || V[j].gate)
                voice_chunk(&V[j], c, clock_beat, bps);
        for (j = 0; j < G_MAXG; j++)
            if (gr[j].on)
                grain_run(&gr[j], c);
        for (i = 0; i < c; i++)
            mixl[i] = mixr[i] = 0.0f;
        for (j = 0; j < G_MAXV; j++)
            if (V[j].stage)
                voice_out(&V[j], c, mixl, mixr);
        for (i = 0; i < c; i++) {                        /* the voices' sum up to playing level (+7 dB) */
            mixl[i] *= 2.2f;
            mixr[i] *= 2.2f;
        }
        fx_chunk(mixl, mixr, c, tempo);
        capture_chunk(mixl, mixr, c);
        for (i = 0; i < c; i++) {                        /* no DC: a 7 Hz high-pass */
            float yl = mixl[i] - dc_x[0] + 0.999f * dc_y[0], yr = mixr[i] - dc_x[1] + 0.999f * dc_y[1];
            dc_x[0] = mixl[i];
            dc_x[1] = mixr[i];
            dc_y[0] = mixl[i] = fm_flush(yl);
            dc_y[1] = mixr[i] = fm_flush(yr);
        }
        for (i = 0; i < c; i++) {
            float l = softclip(mixl[i]), r = softclip(mixr[i]), a = fm_fabsf(l) > fm_fabsf(r) ? fm_fabsf(l) : fm_fabsf(r);
            if (a > out_peak)
                out_peak = a;
            lr[2u * (done + i)] = (int32_t)(l * gain);
            lr[2u * (done + i) + 1u] = (int32_t)(r * gain);
        }
        /* the clocks move on */
        beat_ph += bps * (float)c;
        while (beat_ph >= 1.0f) {
            beat_ph -= 1.0f;
            beat_n++;
        }
        free_ph += bps * (float)c;
        while (free_ph >= 1.0f) {
            free_ph -= 1.0f;
            free_n++;
        }
        sample_count += c;
        if (ext_ticks_ago != 0xFFFFFFFFu)
            ext_ticks_ago += c;
        done += c;
    }
    GV.tempo = tempo_now();
    GV.ext_clock = ext_tempo > 0.0f && ext_ticks_ago < (uint32_t)G_SR;
    GV.peak = (uint8_t)clamp((int32_t)(out_peak * 255.0f), 0, 255);
    if ((sample_count & 0x7FFFu) < n && GV.grains_dropped)
        GV.grains_dropped = 0;                           /* the screen shows drops of the last 0.7 s */
    out_peak *= 0.9f;
    if (maxg < G_MAXG && (sample_count & 0x3FFFu) < n)
        maxg++;                                          /* the overload's cut grows back slowly */
    view_update();
}
