/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 GRAINE contributors */
/* GRAINE: a granular sampler for the M-VAVE FM-1. What the engine (dsp/engine.c, its own unit at -O2, the audio
 * interrupt) and the UI (ui/, the main loop) share. The UI writes parameters and pushes events; the engine reads
 * the parameters once a block and leaves a snapshot of what it plays for the screen. */
#pragma once
#include <stdint.h>

#define G_SR 44100
#define G_MAXV 6                          /* voices */
#define G_MAXG 64                         /* grains, shared by the voices */
#define G_NBUILTIN 8                      /* sources in the firmware (sounds/bank.h) */
#define G_NUSER 4                         /* user slots in the flash (captures kept) */
#define G_SRC_CAPT G_NBUILTIN             /* the capture buffer, in RAM */
#define G_NSRC (G_NBUILTIN + 1 + G_NUSER)
#define G_CAP_RATE 22050u
#define G_CAP_LEN 65472u                  /* samples: 2.97 s at 22,050 Hz; a user slot holds one */
#define G_NPATCH 16
#define G_NMOD 16                         /* modulation slots in a patch */
#define G_NLANE 4                         /* gesture lanes */
#define G_LANE_STEPS 128

/* ---------------------------------------------------------------------------------------------- parameters --- */
enum {
    /* HOME: NUAGE, JEU */
    P_POS, P_SIZE, P_DENS, P_SPRAY,
    P_PITCH, P_SPREAD, P_REV, P_STEREO,
    /* SEL: SOURCE, BALAYAGE */
    P_SAMPLE, P_START, P_LEN, P_FINE,
    P_SCAN, P_SPEED, P_RETRIG, P_KEYPOS,
    /* EDIT: GRAIN, TEXTURE */
    P_WIN, P_SKEW, P_QUANT, P_TIMING,
    P_JITTER, P_SHIMMER, P_GLIDE, P_SLEW,
    /* ENV: AMPLI, MOD */
    P_AATK, P_ADEC, P_ASUS, P_AREL,
    P_MATK, P_MDEC, P_MSUS, P_MREL,
    /* LFO: LFO 1, LFO 2 */
    P_L1RATE, P_L1SHAPE, P_L1SYNC, P_L1MODE,
    P_L2RATE, P_L2SHAPE, P_L2SYNC, P_L2MODE,
    /* FX: FILTRE, ECHO, ESPACE, MATIERE */
    P_FTYPE, P_CUT, P_RES, P_DRIVE,
    P_DTIME, P_DFB, P_DTONE, P_DMIX,
    P_RSIZE, P_RDEC, P_RDAMP, P_RMIX,
    P_BITS, P_DOWN, P_WIDTH, P_LEVEL,
    /* ARP */
    P_ARP, P_ARPDIV, P_ARPOCT, P_ARPGATE,
    /* GLO: GLOBAL, JEU, HASARD */
    P_VOICES, P_PORTA, P_SCALE, P_TONIC,
    P_TEMPO, P_VELO, P_RECMODE, P_GESTLEN,
    P_DICE, P_DOSE, P_TARGET, P_LANG,
    /* the pages for drones and glitches (each in a button's cycle: ui/params.c PAGES) */
    P_STRETCH, P_DETUNE, P_SRC2, P_XFADE,  /* EDIT: DRONE */
    P_CHAOS, P_CHOP, P_CHDIV, P_CHPROB,    /* EDIT: CHAOS */
    P_SHIM, P_SHINT, P_RESO, P_INFINI,     /* FX: HALO */
    P_GATE, P_SWING, P_RING, P_FOLD,       /* FX: HACHOIR */
    P_MORPH, P_TAKEB, P_MUTB, P_AUTOM,     /* HOME: SCENES */
    P_RAIN, P_RRANGE, P_RLEN, P_LATCH,     /* ARP: PLUIE */
    NP
};

/* units and kinds, for the screen (ui/params.c formats them) */
enum { U_PCT, U_PERMIL, U_MS, U_HZ, U_ST, U_CENT, U_X, U_ENUM, U_INT, U_BPM, U_DB, U_TIME, U_SIGNED, U_RATE, U_DIV };
#define PF_ENUM 1u                         /* a list of names */
#define PF_GLOBAL 2u                       /* kept in the settings, not in a patch */
#define PF_NOMOD 4u                        /* no modulation (lists, the clock, actions) */
#define PF_ACTION 8u                       /* a knob that does something on each detent (DICE) */
typedef struct {
    const char *lbl[2];                    /* the knob's label, FR / EN (7 characters at most) */
    const char *name[2];                   /* the long name (the toast) */
    int16_t min, max, def;
    uint8_t unit, flags;
    const char *const *enums;              /* PF_ENUM: max - min + 1 names (one language: words of both) */
} pdef_t;
extern const pdef_t PDEF[NP];

/* modulation sources */
enum { MS_LFO1, MS_LFO2, MS_ENV, MS_VEL, MS_PRES, MS_ALEA, MS_WHEEL, NMS };
#define MS_PER_VOICE(s) ((s) >= MS_ENV && (s) <= MS_ALEA)
typedef struct {
    uint8_t src, dst;                      /* MS_*, P_* (dst 0xFF: free) */
    int8_t depth;                          /* -100 .. 100 % of the parameter's range */
    uint8_t rsv;
} gmod_t;

/* the live state the UI edits: every parameter's raw value and the patch's modulations */
typedef struct {
    int16_t v[NP];
    gmod_t mod[G_NMOD];
} gstate_t;
extern gstate_t G;

/* scene B: the patch the MORPH knob leads to (the UI takes it, the engine blends toward it; not saved) */
extern int16_t G_SCENEB[NP];
extern volatile uint8_t G_SCENEB_OK;

/* gesture lanes: a parameter's motion over the loop (normalized 0..1000) */
typedef struct {
    uint8_t on, p;                         /* playing; the parameter */
    int16_t v[G_LANE_STEPS];
} glane_t;
extern glane_t G_LANE[G_NLANE];

/* sources: built-in (flash, mu-law), the capture (RAM, 16-bit), user slots (flash, mu-law) */
typedef struct {
    const uint8_t *u8;                     /* mu-law, or 0 */
    const int16_t *s16;                    /* 16-bit, or 0 */
    uint32_t len;                          /* samples */
    uint32_t rate;                         /* Hz */
    char name[10];
} gsrc_t;
extern gsrc_t G_SRC[G_NSRC];
extern const float *const g_ulaw;            /* mu-law to float, 256 entries */

/* ------------------------------------------------------------------------------------------- events in --- */
enum { EV_NOTE_ON, EV_NOTE_OFF, EV_ALL_OFF, EV_PRESS, EV_BEND, EV_WHEEL, EV_SUSTAIN, EV_CLOCK_START, EV_CLOCK_STOP,
       EV_MIDI_TICK, EV_MIDI_START, EV_MIDI_STOP, EV_CAPTURE, EV_LFO_SYNC };
void eng_event(uint8_t type, int32_t a, int32_t b);   /* main loop: queued, taken at the next block */
void eng_init(void);
void eng_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt */
void eng_panic(void);
void eng_overload(void);

/* the keys' owner: a key of the panel (0..26), a MIDI note (100 + note), an arpeggio's note (300 + note) */
#define KEY_MIDI(n) (100 + (n))

/* the performance pads (the black keys in TAPIS), held: bits */
enum { PAD_GEL, PAD_ENVERS, PAD_OCTDN, PAD_OCTUP, PAD_QUINTE, PAD_NUEE, PAD_POUSS, PAD_BEGAIE, PAD_DERIVE, PAD_SOMBRE,
       PAD_ECHO, NPADS };

/* the play state the UI sets directly (single words: the render reads them as they are) */
typedef struct {
    volatile uint32_t pads;                /* PAD_* bits held */
    volatile uint8_t tapis;                /* the keys are the TAPIS: positions, and pads */
    volatile uint8_t freeze;               /* HOME held */
    volatile int8_t octave;                /* -3 .. 3 */
    volatile uint8_t running;              /* the clock runs (PLAY) */
    volatile uint8_t gest_rec;             /* REC held: a lane records (the UI writes it) */
} gplay_t;
extern gplay_t GP;

/* ------------------------------------------------------------------------------------------ the snapshot --- */
/* what the engine plays, for the screen: the grains (position in the window 0..1, pitch, size, level now), the
 * voices (their position, level), the modulated value of each parameter (0..1, the newest voice's), the clock */
typedef struct {
    uint16_t x;                            /* position in the window, 0..65535 */
    int8_t semis;                          /* pitch, semitones from the voice's note */
    uint8_t size;                          /* 0..255 (log of the length) */
    uint8_t amp;                           /* window now 0..255 */
    uint8_t voice, rev;
    int8_t pan;                            /* -64..64 */
} gsnap_t;
typedef struct {
    uint32_t seq;
    uint8_t n;                             /* grains in g[] */
    gsnap_t g[G_MAXG];
    uint16_t vpos[G_MAXV];                 /* voice positions 0..65535 */
    uint8_t vlev[G_MAXV];                  /* voice levels 0..255 (0: off) */
    uint8_t vnote[G_MAXV];
    float pmod[NP];                        /* each parameter as modulated now (0..1); -1: not modulated */
    float lfo[2];                          /* -1..1 */
    uint32_t beat_q16;                     /* the clock: beats << 16 */
    uint16_t lane_step;
    uint8_t cap_on;                        /* capturing */
    uint32_t cap_len;                      /* samples captured */
    uint8_t peak;                          /* the output's peak 0..255 */
    uint8_t grains_dropped;
    float tempo;                           /* the tempo playing (internal or MIDI) */
    uint8_t ext_clock;
    float morph;                           /* scene A -> B now, 0..1 */
    uint8_t chop;                          /* HACHE: the mode acting this step (0: none) */
    uint8_t gate_step, gate_open;          /* MASQUE: the sixteenth (0..15), open now */
    uint8_t cap_wait;                      /* the capture waits for a sound (SEUIL) */
    float tape;                            /* the speed of every grain (FREIN, ENVOL), 1: as played */
    uint8_t rain_n;                        /* PLUIE: notes falling now */
} gview_t;
extern gview_t GV;
extern int16_t G_CAP[G_CAP_LEN];

/* ------------------------------------------------------------------------------------------------ helpers --- */
static inline float g_clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
static inline float pnorm(int p)            /* a parameter's raw value as 0..1 */
{
    const pdef_t *d = &PDEF[p];
    return (float)(G.v[p] - d->min) / (float)(d->max - d->min);
}
static inline float pnorm_raw(int p, int32_t raw)
{
    const pdef_t *d = &PDEF[p];
    return (float)(raw - d->min) / (float)(d->max - d->min);
}
float pval(int p, float n);                 /* a parameter's normalized value in its unit (dsp/engine.c) */
uint32_t g_rand(void);
float g_randf(void);                        /* 0..1 */

/* the arpeggiator's and the scale's lists (shared with the screen) */
#define G_NSCALE 10
extern const uint16_t G_SCALE_MASK[G_NSCALE];
