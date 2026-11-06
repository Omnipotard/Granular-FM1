/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 GRAINE contributors */
/* What GRAINE keeps: the settings, a bank of 16 patches (the factory's until the player saves over one), and
 * four user sources in the flash's data area (captures kept). Part of the unity build (app.c). */

#define SET_MAGIC 0x31544553u                    /* "SET1" */
#define BANK_MAGIC 0x324B4E42u                   /* "BNK2": in two halves (OBJ_BANK, OBJ_BANK2) */
#define SLOT_MAGIC 0x31535247u                   /* "GRS1" */
#define SLOT_BASE 0x97000u
#define SLOT_SIZE 0x10000u
#define SLOT_HEAD 32u

typedef struct {
    char name[10];
    uint8_t used, rsv;
    int16_t v[NP];
    gmod_t mod[G_NMOD];
} patch_rec_t;
typedef struct {
    uint32_t magic;
    uint16_t ver, np;
    patch_rec_t p[G_NPATCH / 2];
} bank_half_t;
_Static_assert(sizeof(bank_half_t) <= PLAT_STORE_MAX, "half the bank fits a store object");
typedef struct {
    bank_half_t h[2];
} bank_t;
#define BANKP(i) (bank.h[(i) / (G_NPATCH / 2)].p[(i) % (G_NPATCH / 2)])
typedef struct {
    uint32_t magic;
    uint16_t np;
    uint8_t preset, rsv;
    int16_t g[NP];
} settings_t;
typedef struct {
    uint32_t magic, len, rate;
    char name[12];
    uint32_t rsv[2];
} slot_head_t;
_Static_assert(sizeof(slot_head_t) == SLOT_HEAD, "a slot's header");

static bank_t bank;
static settings_t set;
static char patch_name[10];
static uint8_t cur_preset;

/* ---------------------------------------------------------------------------------- the factory's sounds --- */
typedef struct {
    uint8_t p;
    int16_t v;
} pset_t;
typedef struct {
    const char *name;
    pset_t s[24];
    gmod_t m[4];
} fpreset_t;
#define END {0xFF, 0}
#define MNONE {0, 0xFF, 0, 0}
static const fpreset_t FACTORY[G_NPATCH] = {
    {"BRUME", {{P_SAMPLE, 0}, {P_POS, 300}, {P_SIZE, 131}, {P_DENS, 126}, {P_SPRAY, 20}, {P_STEREO, 70},
               {P_SPEED, 15}, {P_AATK, 69}, {P_AREL, 78}, {P_RMIX, 45}, {P_RSIZE, 80}, {P_RDEC, 70}, {P_CUT, 170},
               {P_DETUNE, 45}, END},
     {{MS_LFO2, P_POS, 15, 0}, {MS_LFO1, P_SIZE, 12, 0}, MNONE, MNONE}},
    {"CHORALE", {{P_SAMPLE, 0}, {P_POS, 420}, {P_SIZE, 140}, {P_DENS, 140}, {P_SPRAY, 12}, {P_SPREAD, 71},
                 {P_QUANT, 4}, {P_SHIMMER, 20}, {P_SPEED, 30}, {P_STEREO, 85}, {P_AATK, 59}, {P_AREL, 70},
                 {P_RMIX, 35}, END},
     {{MS_LFO1, P_SPRAY, 20, 0}, MNONE, MNONE, MNONE}},
    {"VERRIERE", {{P_SAMPLE, 1}, {P_POS, 200}, {P_SIZE, 83}, {P_DENS, 154}, {P_SPRAY, 35}, {P_REV, 30},
                  {P_SHIMMER, 30}, {P_STEREO, 90}, {P_DTIME, 9}, {P_DMIX, 30}, {P_DFB, 55}, {P_RMIX, 30}, END},
     {{MS_ALEA, P_POS, 25, 0}, MNONE, MNONE, MNONE}},
    {"GIVRE", {{P_SAMPLE, 1}, {P_POS, 550}, {P_SIZE, 146}, {P_DENS, 144}, {P_SPRAY, 6}, {P_SPREAD, 14},
               {P_WIN, 0}, {P_AATK, 77}, {P_AREL, 87}, {P_CUT, 166}, {P_RDEC, 90}, {P_RMIX, 50}, {P_RSIZE, 90},
               {P_SHIM, 35}, {P_RESO, 25}, END},
     {{MS_LFO1, P_POS, 6, 0}, {MS_ENV, P_CUT, 20, 0}, MNONE, MNONE}},
    {"HARPE", {{P_SAMPLE, 2}, {P_POS, 0}, {P_SIZE, 106}, {P_DENS, 137}, {P_SPRAY, 2}, {P_SPEED, 100},
               {P_AATK, 18}, {P_ADEC, 59}, {P_ASUS, 70}, {P_AREL, 59}, {P_RMIX, 22}, {P_LEVEL, 60}, END},
     {MNONE, MNONE, MNONE, MNONE}},
    {"ETIREMENT", {{P_SAMPLE, 2}, {P_POS, 0}, {P_SIZE, 120}, {P_DENS, 133}, {P_SPRAY, 3}, {P_SPEED, 25},
                   {P_WIN, 66}, {P_TIMING, 1}, {P_AATK, 33}, {P_AREL, 70}, {P_RMIX, 30}, {P_LEVEL, 42}, END},
     {MNONE, MNONE, MNONE, MNONE}},
    {"SOUFFLE", {{P_SAMPLE, 3}, {P_POS, 400}, {P_SIZE, 69}, {P_DENS, 168}, {P_SPRAY, 50}, {P_SPREAD, 54},
                 {P_STEREO, 100}, {P_FTYPE, 50}, {P_CUT, 131}, {P_RES, 60}, {P_AATK, 59}, {P_AREL, 78},
                 {P_L1RATE, 31}, {P_LEVEL, 92}, END},
     {{MS_LFO1, P_CUT, 40, 0}, {MS_LFO2, P_POS, 30, 0}, MNONE, MNONE}},
    {"MACHINE", {{P_SAMPLE, 4}, {P_POS, 0}, {P_SIZE, 96}, {P_DENS, 110}, {P_TIMING, 2}, {P_SPRAY, 0},
                 {P_SPEED, 100}, {P_BITS, 30}, {P_DTIME, 4}, {P_DMIX, 20}, {P_DFB, 30}, {P_AATK, 8},
                 {P_AREL, 47}, {P_RMIX, 12}, {P_STEREO, 30}, {P_LEVEL, 62}, {P_GATE, 2}, {P_SWING, 30}, END},
     {MNONE, MNONE, MNONE, MNONE}},
    {"GLITCH", {{P_SAMPLE, 4}, {P_SCAN, 4}, {P_SPEED, 150}, {P_SIZE, 77}, {P_DENS, 110}, {P_TIMING, 2},
                {P_REV, 40}, {P_SPREAD, 71}, {P_QUANT, 2}, {P_BITS, 20}, {P_DOWN, 20}, {P_AATK, 8}, {P_RMIX, 10},
                {P_CHAOS, 30}, {P_CHOP, 5}, {P_CHDIV, 2}, {P_CHPROB, 40}, END},
     {{MS_ALEA, P_SIZE, 30, 0}, MNONE, MNONE, MNONE}},
    {"ORGANUM", {{P_SAMPLE, 5}, {P_POS, 300}, {P_SIZE, 137}, {P_DENS, 111}, {P_SPRAY, 10}, {P_SPEED, 10},
                    {P_CUT, 144}, {P_STEREO, 75}, {P_AATK, 59}, {P_AREL, 70}, {P_RMIX, 32}, {P_PORTA, 40}, END},
     {{MS_LFO1, P_POS, 8, 0}, {MS_PRES, P_CUT, 35, 0}, MNONE, MNONE}},
    {"AVERSE", {{P_SAMPLE, 6}, {P_POS, 500}, {P_SIZE, 60}, {P_DENS, 178}, {P_SPRAY, 100}, {P_PITCH, 12},
                    {P_SHIMMER, 50}, {P_STEREO, 100}, {P_DMIX, 30}, {P_DFB, 60}, {P_DTIME, 7}, {P_RMIX, 55},
                    {P_RDEC, 62}, {P_LEVEL, 82}, {P_RAIN, 45}, {P_RLEN, 40}, END},
     {MNONE, MNONE, MNONE, MNONE}},
    {"SPECTRE", {{P_SAMPLE, 7}, {P_SCAN, 3}, {P_SPEED, 50}, {P_SIZE, 154}, {P_DENS, 97}, {P_SPREAD, 71},
                 {P_QUANT, 3}, {P_SKEW, -60}, {P_AATK, 59}, {P_AREL, 87}, {P_RMIX, 50}, {P_RDEC, 72}, {P_LEVEL, 34}, END},
     {{MS_LFO2, P_SKEW, 40, 0}, MNONE, MNONE, MNONE}},
    {"TROU NOIR", {{P_SAMPLE, 7}, {P_PITCH, -12}, {P_SIZE, 183}, {P_DENS, 87}, {P_GLIDE, -120}, {P_REV, 50},
                   {P_CUT, 122}, {P_DRIVE, 40}, {P_RDEC, 87}, {P_RMIX, 60}, {P_RSIZE, 100}, {P_AATK, 77},
                   {P_AREL, 92}, {P_LEVEL, 34}, {P_STRETCH, 55}, {P_SHIM, 30}, {P_SHINT, 3}, END},
     {{MS_LFO1, P_GLIDE, 20, 0}, MNONE, MNONE, MNONE}},
    {"POUSSIERE", {{P_SAMPLE, 1}, {P_POS, 650}, {P_SIZE, 16}, {P_DENS, 200}, {P_JITTER, 80}, {P_SPRAY, 60},
                   {P_FTYPE, 100}, {P_CUT, 140}, {P_STEREO, 100}, {P_TIMING, 1}, {P_RMIX, 35}, {P_LEVEL, 100}, END},
     {{MS_LFO1, P_POS, 30, 0}, {MS_LFO2, P_DENS, 25, 0}, MNONE, MNONE}},
    {"ARPEGIO", {{P_SAMPLE, 2}, {P_POS, 120}, {P_SIZE, 96}, {P_DENS, 144}, {P_SPRAY, 4}, {P_ARP, 1},
                 {P_ARPOCT, 2}, {P_ARPDIV, 3}, {P_ARPGATE, 70}, {P_DTIME, 9}, {P_DMIX, 30}, {P_DFB, 45},
                 {P_AATK, 8}, {P_ADEC, 47}, {P_ASUS, 40}, {P_AREL, 47}, {P_LEVEL, 70}, END},
     {{MS_VEL, P_SIZE, 20, 0}, MNONE, MNONE, MNONE}},
    {"RECOLTE", {{P_SAMPLE, G_SRC_CAPT}, {P_POS, 200}, {P_SIZE, 123}, {P_DENS, 129}, {P_SPRAY, 15},
                 {P_SPEED, 50}, {P_STEREO, 70}, {P_RMIX, 30}, {P_AATK, 33}, {P_AREL, 70}, END},
     {{MS_LFO2, P_POS, 20, 0}, MNONE, MNONE, MNONE}},
};

static void params_default(int globals_too)
{
    int p;
    for (p = 0; p < NP; p++)
        if (globals_too || !(PDEF[p].flags & PF_GLOBAL))
            G.v[p] = PDEF[p].def;
    for (p = 0; p < G_NMOD; p++) {
        G.mod[p].dst = 0xFF;
        G.mod[p].depth = 0;
        G.mod[p].src = 0;
    }
}

static void factory_to(patch_rec_t *r, int i)
{
    const fpreset_t *f = &FACTORY[i];
    int p, k;
    for (p = 0; p < NP; p++)
        r->v[p] = PDEF[p].def;
    for (k = 0; k < 24 && f->s[k].p != 0xFF; k++)
        r->v[f->s[k].p] = f->s[k].v;
    for (k = 0; k < G_NMOD; k++) {
        r->mod[k].dst = 0xFF;
        r->mod[k].depth = 0;
        r->mod[k].src = 0;
    }
    for (k = 0; k < 4; k++)
        if (f->m[k].dst != 0xFF)
            r->mod[k] = f->m[k];
    for (k = 0; k < 9 && f->name[k]; k++)
        r->name[k] = f->name[k];
    for (; k < 10; k++)
        r->name[k] = 0;
    r->used = 1;
}

static void bank_factory(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        bank.h[i].magic = BANK_MAGIC;
        bank.h[i].ver = 2;
        bank.h[i].np = NP;
    }
    for (i = 0; i < G_NPATCH; i++)
        factory_to(&BANKP(i), i);
}

/* a patch into the live state (the globals stay) */
static void patch_load(int i)
{
    const patch_rec_t *r = &BANKP(clamp(i, 0, G_NPATCH - 1));
    int p;
    for (p = 0; p < NP; p++)
        if (!(PDEF[p].flags & PF_GLOBAL))
            G.v[p] = (int16_t)clamp(r->v[p], PDEF[p].min, PDEF[p].max);
    for (p = 0; p < G_NMOD; p++)
        G.mod[p] = r->mod[p].dst < NP ? r->mod[p] : (gmod_t){0, 0xFF, 0, 0};
    for (p = 0; p < 10; p++)
        patch_name[p] = r->name[p];
    patch_name[9] = 0;
    cur_preset = (uint8_t)i;
}

static void patch_store(int i)
{
    patch_rec_t *r = &BANKP(clamp(i, 0, G_NPATCH - 1));
    int p;
    for (p = 0; p < NP; p++)
        r->v[p] = G.v[p];
    for (p = 0; p < G_NMOD; p++)
        r->mod[p] = G.mod[p];
    for (p = 0; p < 10; p++)
        r->name[p] = patch_name[p];
    r->used = 1;
}

static int bank_save(void)
{
    return plat_store_save(OBJ_BANK, &bank.h[0], sizeof bank.h[0]) | plat_store_save(OBJ_BANK2, &bank.h[1], sizeof bank.h[1]);
}

static int settings_save(void)
{
    int p;
    set.magic = SET_MAGIC;
    set.np = NP;
    set.preset = cur_preset;
    for (p = 0; p < NP; p++)
        set.g[p] = G.v[p];
    return plat_store_save(OBJ_SET, &set, sizeof set);
}

/* ------------------------------------------------------------------------------------------ user slots --- */
static void slots_scan(void)
{
    int i;
    for (i = 0; i < G_NUSER; i++) {
        uint32_t off = SLOT_BASE + (uint32_t)i * SLOT_SIZE;
        const slot_head_t *h = (const slot_head_t *)plat_data(off);
        gsrc_t *s = &G_SRC[G_SRC_CAPT + 1 + i];
        int k;
        s->u8 = 0;
        s->s16 = 0;
        s->len = 0;
        s->rate = G_CAP_RATE;
        s->name[0] = 'U';
        s->name[1] = (char)('1' + i);
        s->name[2] = 0;
        if (!h || h->magic != SLOT_MAGIC || h->len < 64u || h->len > SLOT_SIZE - SLOT_HEAD || h->rate < 8000u ||
            h->rate > 48000u)
            continue;
        s->u8 = plat_data(off + SLOT_HEAD);
        s->len = h->len;
        s->rate = h->rate;
        for (k = 0; k < 9 && h->name[k] >= ' ' && h->name[k] < 127; k++)
            s->name[k] = h->name[k];
        s->name[k] = 0;
        if (!k) {
            s->name[0] = 'U';
            s->name[1] = (char)('1' + i);
            s->name[2] = 0;
        }
    }
}

static uint8_t ulaw_enc(float x)                /* the inverse of the table: G.711-style mu-law, 255 levels */
{
    float y = fm_logf(1.0f + 255.0f * fm_fabsf(x)) * (1.0f / 5.5451774f);   /* ln(256) */
    int32_t q;
    if (x < 0.0f)
        y = -y;
    q = (int32_t)((y + 1.0f) * 127.5f + 0.5f);
    return (uint8_t)clamp(q, 0, 255);
}

/* the capture into user slot i (0..3): erase, header, mu-law. Main loop; the sound stops meanwhile */
static int slot_keep(int i, const char *name)
{
    static uint8_t buf[256];
    uint32_t off = SLOT_BASE + (uint32_t)i * SLOT_SIZE, len = G_SRC[G_SRC_CAPT].len, k, n;
    slot_head_t h;
    if (len < 64u || i < 0 || i >= G_NUSER)
        return -1;
    if (len > SLOT_SIZE - SLOT_HEAD)
        len = SLOT_SIZE - SLOT_HEAD;
    for (k = 0; k < SLOT_SIZE; k += 4096u)
        if (plat_data_erase(off + k))
            return -2;
    for (k = 0; k < len; k += n) {
        uint32_t j;
        n = len - k > sizeof buf ? sizeof buf : len - k;
        for (j = 0; j < n; j++)
            buf[j] = ulaw_enc((float)G_CAP[k + j] * (1.0f / 32768.0f));
        if (plat_data_write(off + SLOT_HEAD + k, buf, n))
            return -3;
    }
    h.magic = SLOT_MAGIC;
    h.len = len;
    h.rate = G_CAP_RATE;
    for (k = 0; k < 12u; k++)
        h.name[k] = 0;
    for (k = 0; k < 9u && name[k]; k++)
        h.name[k] = name[k];
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    if (plat_data_write(off, &h, sizeof h))           /* the header last: a slot is whole or not there */
        return -4;
    slots_scan();
    return 0;
}

/* ----------------------------------------------------------------------------------------------- boot --- */
static void store_boot(void)
{
    int p, ok_set;
    params_default(1);
    if (plat_store_load(OBJ_BANK, &bank.h[0], sizeof bank.h[0]) != (int)sizeof bank.h[0] ||
        plat_store_load(OBJ_BANK2, &bank.h[1], sizeof bank.h[1]) != (int)sizeof bank.h[1] ||
        bank.h[0].magic != BANK_MAGIC || bank.h[1].magic != BANK_MAGIC || bank.h[0].np != NP || bank.h[1].np != NP)
        bank_factory();                              /* (an older bank, BNK1 in one object, is the factory's again) */
    ok_set = plat_store_load(OBJ_SET, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC && set.np == NP;
    if (ok_set)
        for (p = 0; p < NP; p++)
            if (PDEF[p].flags & PF_GLOBAL)
                G.v[p] = (int16_t)clamp(set.g[p], PDEF[p].min, PDEF[p].max);
    G.v[P_DICE] = 0;
    slots_scan();
    patch_load(ok_set ? set.preset % G_NPATCH : 0);
}
