/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 GRAINE contributors */
/* The panel: what each button, knob and key does, and the lights. Part of the unity build (app.c).
 *
 *   tap a page button      its page; again: its next page          HOME held      GEL (the scan stops)
 *   LFO / ENV held + knob  that knob's modulation from the LFO / the envelope (OCT-/+: the other sources)
 *   REC tap                capture the output (a new source)       REC held + knob   record the knob's gesture
 *   REC + OCT-             forget the gestures                     REC + SAVE     keep the capture in the flash
 *   SEQ tap / held         the TAPIS: white keys are positions, black keys are pads (held: only while held)
 *   PLAY                   the clock (gestures, arpeggio, sync)    SAVE           save the patch (OCT+ yes)
 *   SELECT                 the source                              PRESETS        the patches
 *   ALGORITHM              the scan's speed (0: frozen)            OCT- / OCT+    octave (both: 0) */

enum { OV_NONE, OV_SAVE, OV_KEEP, OV_WRITING };
#define TAP_MS 450u
#define HOLD_MS 300u

static struct {
    uint8_t group, sub[NGR];
    uint32_t btn, keys;
    uint32_t down_at[NB];
    uint8_t acted[NB];
    int8_t mod_src;                                  /* while LFO / ENV is held: the source the knobs assign */
    uint8_t mod_lfo, mod_env;                        /* the last chosen ones */
    int16_t toast_p;                                 /* a parameter shown big, -1: the texts below, -2 none */
    uint32_t toast_until;
    char toast_a[28], toast_b[24];
    rgb_t toast_c;
    uint8_t ov;
    int8_t ov_sel;
    uint8_t dirty, set_dirty;
    uint32_t set_changed, preset_warn;
    uint32_t enc_t[NE];
    uint8_t tapis_latch;
    int8_t rec_lane[4];
    uint8_t rec_any;
    uint16_t rec_step;
    uint8_t vel[NKEYS];
    uint8_t prs[NKEYS];
    uint32_t boot;
    uint8_t strip_dirty, head_dirty;
    uint8_t seq_held_tapis;
    int8_t key_note[NKEYS];                          /* the note each held key started (scales, octave) */
} ui;

static gstate_t dice_hist[8];
static int dice_n;

static const uint8_t WHITE[16] = PLAT_WHITE_KEYS, BLACK[11] = PLAT_BLACK_KEYS;

static int cur_page(void) { return GROUP_FIRST[ui.group] + ui.sub[ui.group] % GROUP_N[ui.group]; }

/* ------------------------------------------------------------------------------------------- messages --- */
static void toast_param(int p)
{
    ui.toast_p = (int16_t)p;
    ui.toast_until = plat_ms() + 900u;
}
static void toast(const char *a, const char *b, rgb_t c)
{
    int i;
    for (i = 0; i < 27 && a && a[i]; i++)
        ui.toast_a[i] = a[i];
    ui.toast_a[i] = 0;
    for (i = 0; i < 23 && b && b[i]; i++)
        ui.toast_b[i] = b[i];
    ui.toast_b[i] = 0;
    ui.toast_c = c;
    ui.toast_p = -1;
    ui.toast_until = plat_ms() + 1200u;
}

void app_say(const char *a, const char *b)
{
    char t[56];
    toast(a, b, K_TEXT);
    if (ann_on()) {
        t[0] = 0;
        s_cat(t, a ? a : "");
        if (b && *b) {
            s_cat(t, " ");
            s_cat(t, b);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void mark_dirty(int p)
{
    if (PDEF[p].flags & PF_GLOBAL) {
        ui.set_dirty = 1;
        ui.set_changed = plat_ms();
    } else {
        ui.dirty = 1;
    }
    ui.strip_dirty = 1;
}

/* ------------------------------------------------------------------------------------------ parameters --- */
static int32_t param_step(int p, int32_t det, uint32_t dt)
{
    const pdef_t *d = &PDEF[p];
    int32_t range = d->max - d->min, step;
    if (d->flags & PF_ENUM || range <= 16)
        return det;
    step = range / 200;
    if (step < 1)
        step = 1;
    if (p == P_SPEED)
        step = 5;
    if (dt < 30u)
        step *= 6;
    else if (dt < 70u)
        step *= 2;
    return det * step;
}

/* a lane playing p: the knob moves the whole gesture */
static int lane_of(int p)
{
    int l;
    for (l = 0; l < G_NLANE; l++)
        if (G_LANE[l].on && G_LANE[l].p == p)
            return l;
    return -1;
}

/* ------------------------------------------------------------------------------------------- scenes ---- */
static int scene_p(int p)                        /* what a scene holds: the patch's sound, not the scene knobs */
{
    return !(PDEF[p].flags & (PF_GLOBAL | PF_ACTION)) && p != P_MORPH && p != P_AUTOM;
}
static void scene_take(int32_t det)
{
    int p;
    if (det < 0 && G_SCENEB_OK) {                /* back: A and B change places */
        for (p = 0; p < NP; p++)
            if (scene_p(p)) {
                int16_t t = G.v[p];
                G.v[p] = G_SCENEB[p];
                G_SCENEB[p] = t;
            }
        ui.dirty = 1;
        toast(TR("SCENES", "SCENES"), TR("A <> B", "A <> B"), KNOB_RGB[1]);
    } else {
        for (p = 0; p < NP; p++)
            G_SCENEB[p] = G.v[p];
        G_SCENEB_OK = 1;
        toast(TR("SCENE B", "SCENE B"), TR("PRISE: CHANGE A", "TAKEN: NOW EDIT A"), KNOB_RGB[1]);
    }
    ui.strip_dirty = ui.head_dirty = 1;
}
static void scene_mutate(int32_t det)
{
    float dose = (float)G.v[P_DOSE] * 0.01f * (det < 0 ? 0.5f : 1.0f);
    int p;
    if (!G_SCENEB_OK)
        for (p = 0; p < NP; p++)
            G_SCENEB[p] = G.v[p];
    for (p = 0; p < NP; p++)
        if (scene_p(p) && !(PDEF[p].flags & PF_ENUM) && p != P_SAMPLE && p != P_SRC2 && p != P_LEVEL) {
            const pdef_t *d = &PDEF[p];
            float dv = (g_randf() * 2.0f - 1.0f) * dose * (float)(d->max - d->min) * 0.5f;
            G_SCENEB[p] = (int16_t)clamp(G_SCENEB[p] + (int32_t)dv, d->min, d->max);
        }
    G_SCENEB_OK = 1;
    toast(TR("SCENE B", "SCENE B"), TR("MUTEE: TOURNE MORPH", "MUTATED: TURN MORPH"), KNOB_RGB[2]);
    ui.strip_dirty = ui.head_dirty = 1;
}

static void param_turn(int p, int32_t det, uint32_t dt)
{
    const pdef_t *d = &PDEF[p];
    int32_t old = G.v[p], nv;
    int l;
    if (d->flags & PF_ACTION) {
        if (p == P_DICE)
            dice_roll(det);
        else if (p == P_TAKEB)
            scene_take(det);
        else if (p == P_MUTB)
            scene_mutate(det);
        return;
    }
    nv = clamp(old + param_step(p, det, dt), d->min, d->max);
    if (p == P_SPEED && old != 0 && ((old > 0) != (nv > 0) || nv == 0))
        nv = 0;                                      /* crossing zero stops there: frozen is easy to find */
    if (nv == old)
        return;
    G.v[p] = (int16_t)nv;
    if ((l = lane_of(p)) >= 0 && !GP.gest_rec) {
        int s, dv = (int32_t)((pnorm_raw(p, nv) - pnorm_raw(p, old)) * 1000.0f);
        for (s = 0; s < G_LANE_STEPS; s++)
            G_LANE[l].v[s] = (int16_t)clamp(G_LANE[l].v[s] + dv, 0, 1000);
    }
    mark_dirty(p);
    toast_param(p);
    if (p == P_LANG || p == P_SAMPLE)
        ui.head_dirty = 1;
    if (p == P_LATCH && nv == 0)
        eng_event(EV_ALL_OFF, 0, 0);                 /* TENUE off: the keys held let go */
}

/* ----------------------------------------------------------------------------------------- modulation --- */
static int mod_find(int src, int dst, int make)
{
    int m;
    for (m = 0; m < G_NMOD; m++)
        if (G.mod[m].dst == dst && G.mod[m].src == src)
            return m;
    if (!make)
        return -1;
    for (m = 0; m < G_NMOD; m++)
        if (G.mod[m].dst >= NP || !G.mod[m].depth) {
            G.mod[m].src = (uint8_t)src;
            G.mod[m].dst = (uint8_t)dst;
            G.mod[m].depth = 0;
            return m;
        }
    return -1;
}
static int mod_depth(int src, int dst)
{
    int m = mod_find(src, dst, 0);
    return m < 0 ? 0 : G.mod[m].depth;
}
static void mod_turn(int p, int32_t det, uint32_t dt)
{
    int m, dv;
    if (PDEF[p].flags & PF_NOMOD) {
        toast(TR("PAS DE MODULATION", "NOT MODULATABLE"), PDEF[p].lbl[LG], K_DIM);
        return;
    }
    m = mod_find(ui.mod_src, p, 1);
    if (m < 0) {
        toast(TR("MODULATIONS", "MODULATIONS"), TR("PLEIN (16)", "FULL (16)"), K_REC);
        return;
    }
    dv = (int)det * (dt < 40u ? 6 : 2);
    G.mod[m].depth = (int8_t)clamp(G.mod[m].depth + dv, -100, 100);
    if (!G.mod[m].depth)
        G.mod[m].dst = 0xFF;
    ui.dirty = 1;
    ui.strip_dirty = 1;
    ui.toast_p = (int16_t)(1000 + p);                /* the toast shows the depth */
    ui.toast_until = plat_ms() + 900u;
}

static const char *const SRC_NAME[NMS][2] = {{"LFO 1", "LFO 1"}, {"LFO 2", "LFO 2"}, {"ENV MOD", "MOD ENV"},
                                              {"VELOCITE", "VELOCITY"}, {"PRESSION", "PRESSURE"},
                                              {"ALEA", "RANDOM"}, {"MOLETTE", "MOD WHEEL"}};
static const uint8_t ENV_SRCS[5] = {MS_ENV, MS_VEL, MS_PRES, MS_ALEA, MS_WHEEL};

/* ------------------------------------------------------------------------------------------- gestures --- */
static void gest_begin(int k, int p)
{
    int l = lane_of(p), s;
    if (l < 0)
        for (l = 0; l < G_NLANE; l++)
            if (!G_LANE[l].on)
                break;
    if (l >= G_NLANE) {                              /* all four in use: the first goes */
        for (l = 0; l < G_NLANE - 1; l++)
            G_LANE[l] = G_LANE[l + 1];
        l = G_NLANE - 1;
    }
    G_LANE[l].p = (uint8_t)p;
    for (s = 0; s < G_LANE_STEPS; s++)
        G_LANE[l].v[s] = (int16_t)(pnorm(p) * 1000.0f);
    G_LANE[l].on = 1;
    ui.rec_lane[k] = (int8_t)l;
    ui.rec_any = 1;
    GP.gest_rec = 1;
    ui.rec_step = GV.lane_step;
    if (!GP.running)
        eng_event(EV_CLOCK_START, 0, 0);
}
static void gest_write(void)                         /* every frame while REC is held: the knobs into their lanes */
{
    int k;
    uint16_t now = GV.lane_step, s;
    if (!GP.gest_rec)
        return;
    for (k = 0; k < 4; k++) {
        int l = ui.rec_lane[k];
        int16_t v;
        if (l < 0 || l >= G_NLANE || !G_LANE[l].on)
            continue;
        v = (int16_t)(pnorm(G_LANE[l].p) * 1000.0f);
        for (s = ui.rec_step;; s = (uint16_t)((s + 1u) % G_LANE_STEPS)) {
            G_LANE[l].v[s] = v;
            if (s == now)
                break;
        }
    }
    ui.rec_step = now;
}
static void gest_clear(void)
{
    int l;
    for (l = 0; l < G_NLANE; l++)
        G_LANE[l].on = 0;
    toast(TR("GESTES", "GESTURES"), TR("EFFACES", "CLEARED"), K_SOFT);
    ui.strip_dirty = 1;
}

/* --------------------------------------------------------------------------------------------- dice ---- */
static const uint8_t DICE_NUAGE[] = {P_POS, P_SIZE, P_DENS, P_SPRAY, P_SPREAD, P_REV, P_STEREO, P_SPEED};
static const uint8_t DICE_GRAIN[] = {P_WIN, P_SKEW, P_JITTER, P_SHIMMER, P_GLIDE, P_SPREAD, P_SIZE};
static const uint8_t DICE_SON[] = {P_FTYPE, P_CUT, P_RES, P_DRIVE, P_DFB, P_DMIX, P_RMIX, P_RSIZE, P_RDEC, P_BITS,
                                   P_DOWN};
static void dice_one(int p, float dose)
{
    const pdef_t *d = &PDEF[p];
    float range = (float)(d->max - d->min), dv = (g_randf() * 2.0f - 1.0f) * dose * range * 0.5f;
    if (p == P_BITS || p == P_DOWN || p == P_GLIDE)
        dv *= 0.5f;
    G.v[p] = (int16_t)clamp(G.v[p] + (int32_t)dv, d->min, d->max);
}
static void dice_roll(int32_t det)
{
    float dose = (float)G.v[P_DOSE] * 0.01f;
    int t = G.v[P_TARGET], i;
    if (det < 0) {                                    /* back: the state before the last roll */
        if (dice_n > 0) {
            G = dice_hist[--dice_n];
            toast(TR("HASARD", "CHANCE"), TR("RETOUR", "UNDONE"), K_SOFT);
        } else {
            toast(TR("HASARD", "CHANCE"), TR("RIEN A DEFAIRE", "NOTHING TO UNDO"), K_DIM);
        }
        ui.strip_dirty = ui.head_dirty = 1;
        return;
    }
    if (dice_n == 8) {
        for (i = 0; i < 7; i++)
            dice_hist[i] = dice_hist[i + 1];
        dice_n = 7;
    }
    dice_hist[dice_n++] = G;
    if (t == 0 || t == 3)
        for (i = 0; i < (int)sizeof DICE_NUAGE; i++)
            dice_one(DICE_NUAGE[i], dose);
    if (t == 1 || t == 3)
        for (i = 0; i < (int)sizeof DICE_GRAIN; i++)
            dice_one(DICE_GRAIN[i], dose);
    if (t == 2 || t == 3)
        for (i = 0; i < (int)sizeof DICE_SON; i++)
            dice_one(DICE_SON[i], dose);
    if (t == 3 && g_randf() < dose) {
        G.v[P_QUANT] = (int16_t)(g_rand() % 7u);
        G.v[P_TIMING] = (int16_t)(g_rand() % 3u);
        G.v[P_SCAN] = (int16_t)(g_rand() % 5u);
    }
    G.v[P_DICE] = (int16_t)((G.v[P_DICE] + 1) % 1000);
    ui.dirty = 1;
    ui.strip_dirty = ui.head_dirty = 1;
    {
        char b[16];
        s_int(dice_n, b, 0);
        toast(TR("LANCER", "ROLL"), b, KNOB_RGB[0]);
    }
}

/* ------------------------------------------------------------------------------------------ presets ---- */
static void preset_step(int32_t det)
{
    int n = ((int)cur_preset + (int)det % G_NPATCH + G_NPATCH) % G_NPATCH;
    uint32_t now = plat_ms();
    char b[16];
    if (ui.dirty && now - ui.preset_warn > 2500u) {
        ui.preset_warn = now;
        toast(TR("SON MODIFIE", "PATCH CHANGED"), TR("TOURNE ENCORE", "TURN AGAIN"), K_REC);
        return;
    }
    ui.preset_warn = now;
    patch_load(n);
    ui.dirty = 0;
    ui.set_dirty = 1;
    ui.set_changed = now;
    ui.strip_dirty = ui.head_dirty = 1;
    s_int(n + 1, b, 0);
    s_cat(b, " ");
    s_cat(b, patch_name);
    toast(TR("SON", "PATCH"), b, KNOB_RGB[0]);
}

/* ------------------------------------------------------------------------------------------- overlays --- */
static void ov_open(int which)
{
    int i;
    ui.ov = (uint8_t)which;
    if (which == OV_SAVE)
        ui.ov_sel = (int8_t)cur_preset;
    if (which == OV_KEEP) {
        ui.ov_sel = 0;
        for (i = 0; i < G_NUSER; i++)
            if (G_SRC[G_SRC_CAPT + 1 + i].len == 0) {
                ui.ov_sel = (int8_t)i;
                break;
            }
    }
}
static void ov_turn(int32_t det)
{
    int n = ui.ov == OV_SAVE ? G_NPATCH : G_NUSER;
    ui.ov_sel = (int8_t)(((int)ui.ov_sel + (int)det % n + n) % n);
}
static void ov_yes(void)
{
    if (ui.ov == OV_SAVE) {
        char b[16];
        patch_store(ui.ov_sel);
        cur_preset = (uint8_t)ui.ov_sel;
        if (bank_save() == 0) {
            ui.dirty = 0;
            s_int(cur_preset + 1, b, 0);
            s_cat(b, " ");
            s_cat(b, patch_name);
            toast(TR("SAUVE", "SAVED"), b, KNOB_RGB[2]);
        } else {
            toast(TR("SAUVEGARDE", "SAVE"), TR("ECHEC", "FAILED"), K_REC);
        }
        settings_save();
        ui.ov = OV_NONE;
    } else if (ui.ov == OV_KEEP) {
        if (G_SRC[G_SRC_CAPT].len < 64u) {
            toast(TR("CAPTURE VIDE", "NOTHING CAPTURED"), TR("REC D'ABORD", "TAP REC FIRST"), K_REC);
            ui.ov = OV_NONE;
        } else {
            ui.ov = OV_WRITING;                      /* drawn first, written at the next frame */
        }
    }
    ui.strip_dirty = ui.head_dirty = 1;
}
static void ov_write_now(void)
{
    char name[10] = "RECOLTE1";
    int rc;
    name[7] = (char)('1' + ui.ov_sel);
    eng_event(EV_CAPTURE, 0, 0);
    rc = slot_keep(ui.ov_sel, name);
    ui.ov = OV_NONE;
    if (rc == 0) {
        char b[16] = "U";
        b[1] = (char)('1' + ui.ov_sel);
        b[2] = 0;
        G.v[P_SAMPLE] = (int16_t)(G_SRC_CAPT + 1 + ui.ov_sel);
        ui.dirty = 1;
        toast(TR("GARDE DANS", "KEPT IN"), b, KNOB_RGB[2]);
    } else {
        toast(TR("ECRITURE", "WRITE"), TR("ECHEC", "FAILED"), K_REC);
    }
    ui.strip_dirty = ui.head_dirty = 1;
}

/* --------------------------------------------------------------------------------------------- keys ---- */
static int key_note(int k)
{
    int note = 53 + k;                               /* key 0 is F3 */
    if (G.v[P_SCALE])
        note = ui_scale_snap(note);
    return note;
}
static int ui_scale_snap(int note)
{
    uint16_t mask = G_SCALE_MASK[clamp(G.v[P_SCALE], 0, G_NSCALE - 1)];
    int k, tonic = G.v[P_TONIC];
    for (k = 0; k < 12; k++) {
        int d = (k + 1) / 2 * ((k & 1) ? -1 : 1);
        int deg = ((note + d - tonic) % 12 + 12) % 12;
        if (mask >> deg & 1u)
            return note + d;
    }
    return note;
}
static int white_index(int k)
{
    int i;
    for (i = 0; i < 16; i++)
        if (WHITE[i] == k)
            return i;
    return -1;
}
static int black_index(int k)
{
    int i;
    for (i = 0; i < 11; i++)
        if (BLACK[i] == k)
            return i;
    return -1;
}
static int tapis_on(void) { return ui.tapis_latch || ui.seq_held_tapis; }

static void key_down(int k, int vel)
{
    if (tapis_on()) {
        int w = white_index(k), b;
        if (w >= 0) {
            eng_event(EV_NOTE_ON, k, 60 | vel << 8 | (w + 1) << 16);
        } else if ((b = black_index(k)) >= 0) {
            GP.pads |= 1u << b;
        }
        ui.key_note[k] = -1;
        return;
    }
    ui.key_note[k] = (int8_t)key_note(k);
    eng_event(EV_NOTE_ON, k, ui.key_note[k] | vel << 8);
}
static void key_up(int k)
{
    int b = black_index(k);
    if (b >= 0)
        GP.pads &= ~(1u << b);
    eng_event(EV_NOTE_OFF, k, 0);
}

/* --------------------------------------------------------------------------------------------- MIDI ---- */
static void midi_input(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu, ty = st & 0xF0u;
        if (st == 0xF8u)
            eng_event(EV_MIDI_TICK, 0, 0);
        else if (st == 0xFAu || st == 0xFBu)
            eng_event(EV_MIDI_START, 0, 0);
        else if (st == 0xFCu)
            eng_event(EV_MIDI_STOP, 0, 0);
        else if (ty == 0x90u && d2)
            eng_event(EV_NOTE_ON, KEY_MIDI((int)d1), (int32_t)(d1 | d2 << 8));
        else if (ty == 0x80u || ty == 0x90u)
            eng_event(EV_NOTE_OFF, KEY_MIDI((int)d1), 0);
        else if (ty == 0xE0u)
            eng_event(EV_BEND, (int32_t)(d1 | d2 << 7) - 8192, 0);
        else if (ty == 0xD0u)
            eng_event(EV_PRESS, -1, (int32_t)d1);
        else if (ty == 0xA0u)
            eng_event(EV_PRESS, KEY_MIDI((int)d1), (int32_t)d2);
        else if (ty == 0xB0u) {
            static const uint8_t CC_P[8] = {P_POS, P_SIZE, P_DENS, P_SPRAY, P_PITCH, P_SPREAD, P_REV, P_STEREO};
            if (d1 == 1u)
                eng_event(EV_WHEEL, (int32_t)d2, 0);
            else if (d1 == 64u)
                eng_event(EV_SUSTAIN, d2 >= 64u, 0);
            else if (d1 == 123u || d1 == 120u)
                eng_event(EV_ALL_OFF, 0, 0);
            else if (d1 >= 14u && d1 < 22u) {         /* CC 14..21: the cloud and its play */
                int p = CC_P[d1 - 14u];
                G.v[p] = (int16_t)(PDEF[p].min + (int32_t)d2 * (PDEF[p].max - PDEF[p].min) / 127);
                ui.strip_dirty = 1;
            } else if (d1 == 74u || d1 == 71u || d1 == 91u || d1 == 94u) {
                int p = d1 == 74u ? P_CUT : d1 == 71u ? P_RES : d1 == 91u ? P_RMIX : P_DMIX;
                G.v[p] = (int16_t)(PDEF[p].min + (int32_t)d2 * (PDEF[p].max - PDEF[p].min) / 127);
                ui.strip_dirty = 1;
            }
        }
    }
}

/* ------------------------------------------------------------------------------------------- buttons ---- */
static int held(int b) { return (int)(ui.btn >> b & 1u); }

static void page_tap(int grp)
{
    if (ui.group == grp)
        ui.sub[grp] = (uint8_t)((ui.sub[grp] + 1u) % GROUP_N[grp]);
    else
        ui.group = (uint8_t)grp;
    if (grp == GR_ARP && G.v[P_ARP] == 0 && ui.sub[grp] == 0 && ui.group == grp) {
        /* (ARP opens its page; the arpeggio starts from its MODE knob) */
    }
    ui.strip_dirty = ui.head_dirty = 1;
    ui.toast_until = 0;
}

static void button_press(int b, uint32_t now)
{
    ui.down_at[b] = now;
    ui.acted[b] = 0;
    if (ui.ov != OV_NONE) {
        if (b == B_OCTUP || (b == B_SAVE && ui.ov == OV_SAVE))
            ov_yes();
        else if (b == B_OCTDN) {
            ui.ov = OV_NONE;
            toast(TR("ANNULE", "CANCELLED"), "", K_DIM);
        }
        ui.acted[b] = 1;
        ui.strip_dirty = ui.head_dirty = 1;
        return;
    }
    /* combinations: the held button acted, so its release is not a tap */
    if (held(B_REC) && b == B_OCTDN) {
        gest_clear();
        ui.acted[B_REC] = ui.acted[b] = 1;
        return;
    }
    if (held(B_REC) && b == B_SAVE) {
        ov_open(OV_KEEP);
        ui.acted[B_REC] = ui.acted[b] = 1;
        return;
    }
    if ((held(B_LFO) || held(B_ENV)) && (b == B_OCTDN || b == B_OCTUP)) {
        int d = b == B_OCTUP ? 1 : -1;
        if (held(B_LFO)) {
            ui.mod_lfo ^= 1u;
            ui.mod_src = (int8_t)(MS_LFO1 + ui.mod_lfo);
        } else {
            int i, at = 0;
            for (i = 0; i < 5; i++)
                if (ENV_SRCS[i] == ui.mod_src)
                    at = i;
            at = (at + d + 5) % 5;
            ui.mod_env = (uint8_t)at;
            ui.mod_src = (int8_t)ENV_SRCS[at];
        }
        ui.acted[b] = 1;
        ui.acted[held(B_LFO) ? B_LFO : B_ENV] = 1;
        ui.strip_dirty = ui.head_dirty = 1;
        return;
    }
    if ((b == B_OCTDN && held(B_OCTUP)) || (b == B_OCTUP && held(B_OCTDN))) {
        GP.octave = 0;
        ui.acted[B_OCTDN] = ui.acted[B_OCTUP] = 1;
        toast("OCTAVE", "0", K_TEXT);
        return;
    }
    switch (b) {
    case B_OCTDN: case B_OCTUP: {
        char t[8];
        GP.octave = (int8_t)clamp(GP.octave + (b == B_OCTUP ? 1 : -1), -3, 3);
        s_int(GP.octave, t, 1);
        toast("OCTAVE", t, K_TEXT);
        ui.acted[b] = 1;
        ui.head_dirty = 1;
        break;
    }
    case B_PLAY:
        eng_event(GP.running ? EV_CLOCK_STOP : EV_CLOCK_START, 0, 0);
        GP.running = (uint8_t)!GP.running;           /* (the render agrees at its next block) */
        ui.acted[b] = 1;
        ui.head_dirty = 1;
        break;
    case B_LFO:
        ui.mod_src = (int8_t)(MS_LFO1 + ui.mod_lfo);
        break;
    case B_ENV:
        ui.mod_src = (int8_t)ENV_SRCS[ui.mod_env % 5u];
        break;
    case B_REC:
        ui.rec_any = 0;
        ui.rec_lane[0] = ui.rec_lane[1] = ui.rec_lane[2] = ui.rec_lane[3] = -1;
        break;
    default:
        break;
    }
}

static void button_release(int b, uint32_t now)
{
    int tap = !ui.acted[b] && now - ui.down_at[b] < TAP_MS;
    switch (b) {
    case B_HOME:
        if (GP.freeze) {
            GP.freeze = 0;
            ui.head_dirty = 1;
        } else if (tap)
            page_tap(GR_HOME);
        break;
    case B_SEL: if (tap) page_tap(GR_SEL); break;
    case B_EDIT: if (tap) page_tap(GR_EDIT); break;
    case B_FX: if (tap) page_tap(GR_FX); break;
    case B_GLO: if (tap) page_tap(GR_GLO); break;
    case B_ARP:
        if (tap) {
            if (ui.group == GR_ARP && G.v[P_ARP]) {  /* on its page: ARP stops it */
                G.v[P_ARP] = 0;
                eng_event(EV_ALL_OFF, 0, 0);
                ui.dirty = 1;
                toast(TR("ARPEGE", "ARP"), "OFF", K_DIM);
            } else if (ui.group == GR_ARP) {
                G.v[P_ARP] = 1;
                ui.dirty = 1;
                toast(TR("ARPEGE", "ARP"), TR("MONTE", "UP"), KNOB_RGB[0]);
            }
            page_tap(GR_ARP);
        }
        break;
    case B_LFO: case B_ENV:
        if (tap)
            page_tap(b == B_LFO ? GR_LFO : GR_ENV);
        ui.mod_src = -1;
        ui.strip_dirty = ui.head_dirty = 1;
        break;
    case B_SEQ:
        if (ui.seq_held_tapis) {
            ui.seq_held_tapis = 0;
            if (!ui.tapis_latch)
                eng_event(EV_ALL_OFF, 0, 0);
        } else if (tap) {
            ui.tapis_latch ^= 1u;
            eng_event(EV_ALL_OFF, 0, 0);
            GP.pads = 0;
            toast("TAPIS", ui.tapis_latch ? "ON" : "OFF", ui.tapis_latch ? KNOB_RGB[1] : K_DIM);
        }
        GP.tapis = (uint8_t)tapis_on();
        ui.head_dirty = 1;
        break;
    case B_REC:
        if (ui.rec_any) {                            /* a gesture was recorded */
            GP.gest_rec = 0;
            ui.rec_any = 0;
            toast(TR("GESTE", "GESTURE"), TR("EN BOUCLE", "LOOPING"), SRC_RGB[MS_ENV]);
        } else if (tap) {
            int on = !GV.cap_on;
            eng_event(EV_CAPTURE, on, G.v[P_RECMODE]);
            if (on) {
                toast("CAPTURE", TR("ENREGISTRE...", "RECORDING..."), K_REC);
            } else {
                G.v[P_SAMPLE] = G_SRC_CAPT;
                ui.dirty = 1;
                toast("CAPTURE", TR("PRETE: SOURCE", "READY: SOURCE"), KNOB_RGB[2]);
            }
        }
        ui.head_dirty = ui.strip_dirty = 1;
        break;
    case B_SAVE:
        if (tap)
            ov_open(OV_SAVE);
        break;
    default:
        break;
    }
}

static void buttons(uint32_t btn, uint32_t now)
{
    int b;
    for (b = 0; b < NB; b++) {
        uint32_t bit = 1u << b;
        if ((btn & bit) && !(ui.btn & bit)) {
            ui.btn |= bit;
            button_press(b, now);
        } else if (!(btn & bit) && (ui.btn & bit)) {
            ui.btn &= ~bit;
            button_release(b, now);
        }
    }
    ui.btn = btn;
    /* holds */
    if (held(B_HOME) && !ui.acted[B_HOME] && now - ui.down_at[B_HOME] >= HOLD_MS) {
        ui.acted[B_HOME] = 1;
        GP.freeze = 1;
        toast(TR("GEL", "FREEZE"), TR("LACHE HOME", "RELEASE HOME"), K_ICE);
        ui.head_dirty = 1;
    }
    if (held(B_SEQ) && !ui.acted[B_SEQ] && now - ui.down_at[B_SEQ] >= HOLD_MS && !ui.tapis_latch) {
        ui.acted[B_SEQ] = 1;
        ui.seq_held_tapis = 1;
        GP.tapis = 1;
        eng_event(EV_ALL_OFF, 0, 0);
        ui.head_dirty = 1;
    }
}

/* ------------------------------------------------------------------------------------------- encoders --- */
static void encoders(uint32_t now)
{
    int k;
    for (k = 0; k < NE; k++) {
        int32_t e = plat_enc(k);
        uint32_t dt;
        if (!e)
            continue;
        dt = now - ui.enc_t[k];
        ui.enc_t[k] = now;
        if (ui.ov == OV_SAVE || ui.ov == OV_KEEP) {
            ov_turn(e);
            ui.head_dirty = 1;
            continue;
        }
        if (k >= EN_K1) {
            int p = PAGES[cur_page()].p[k - EN_K1];
            if (ui.mod_src >= 0 && (held(B_LFO) || held(B_ENV))) {
                ui.acted[held(B_LFO) ? B_LFO : B_ENV] = 1;
                mod_turn(p, e, dt);
            } else if (held(B_REC)) {
                if (ui.rec_lane[k - EN_K1] < 0)
                    gest_begin(k - EN_K1, p);
                ui.acted[B_REC] = 1;
                param_turn(p, e, dt);
            } else {
                param_turn(p, e, dt);
            }
        } else if (k == EN_SELECT) {                 /* the sources, the empty user slots skipped */
            int d = e > 0 ? 1 : -1, i = G.v[P_SAMPLE], n;
            for (n = 0; n < G_NSRC; n++) {
                i = (i + d + G_NSRC) % G_NSRC;
                if (i <= G_SRC_CAPT || G_SRC[i].len >= 64u)
                    break;
            }
            if (i != G.v[P_SAMPLE]) {
                G.v[P_SAMPLE] = (int16_t)i;
                mark_dirty(P_SAMPLE);
                toast_param(P_SAMPLE);
                ui.head_dirty = 1;
            }
        } else if (k == EN_PRESET) {
            preset_step(e > 0 ? 1 : -1);
        } else if (k == EN_ALGO) {
            param_turn(P_SPEED, e, dt);
        }
    }
}

/* ---------------------------------------------------------------------------------------------- lights --- */
static void lights(uint32_t now)
{
    uint32_t lit = 1u << GROUP_BTN[ui.group], dim = 0, keys = 0, dimk = 0;
    int i;
    if (tapis_on())
        lit |= 1u << B_SEQ;
    if (G.v[P_ARP])
        lit |= 1u << B_ARP;
    if (GP.running)
        lit |= LED_PLAY_GREEN;
    if ((GV.cap_on && (now / 180u) & 1u) || GP.gest_rec)
        lit |= 1u << B_REC;
    if (ui.dirty)
        lit |= 1u << B_SAVE;
    if (GP.freeze)
        lit |= 1u << B_HOME;
    if (ui.mod_src >= 0)
        lit |= (now / 120u & 1u) ? 1u << (held(B_LFO) ? B_LFO : B_ENV) : 0u;
    for (i = 0; i < NB; i++)
        if (!(lit >> i & 1u))
            dim |= 1u << i;
    keys = ui.keys;
    if (tapis_on()) {
        int v = -1, j;
        uint32_t best = 0;
        for (j = 0; j < G_MAXV; j++)                 /* the slice the newest voice plays */
            if (GV.vlev[j] && GV.vlev[j] > best) {
                best = GV.vlev[j];
                v = j;
            }
        for (i = 0; i < 16; i++)
            dimk |= 1u << WHITE[i];
        for (i = 0; i < 11; i++)
            if (GP.pads >> i & 1u)
                keys |= 1u << BLACK[i];
            else
                dimk |= 1u << BLACK[i];
        if (v >= 0)
            keys |= 1u << WHITE[clamp(GV.vpos[v] * 16 / 65536, 0, 15)];
    } else if (G.v[P_SCALE]) {
        for (i = 0; i < NKEYS; i++)
            if (ui_scale_snap(53 + i) == 53 + i)
                dimk |= 1u << i;
    }
    plat_lights(lit, keys, dim & ~lit, dimk & ~keys, 1);
}

/* ----------------------------------------------------------------------------------------------- input --- */
static void input(void)
{
    uint32_t keys, k, now = plat_ms();
    touch_poll();
    keys = touch_keys();
    midi_input();
    buttons(plat_buttons(), now);
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            uint32_t v = touch_vel(k);
            if (!v)
                v = 100;
            ui.vel[k] = (uint8_t)v;
            key_down((int)k, (int)v);
        } else if (!(keys & bit) && (ui.keys & bit)) {
            key_up((int)k);
        } else if ((keys & bit) && touch_pressure(k) != ui.prs[k]) {
            ui.prs[k] = touch_pressure(k);
            eng_event(EV_PRESS, (int32_t)k, ui.prs[k]);
        }
    }
    ui.keys = keys;
    encoders(now);
    lights(now);
}

void app_input_only(void) { input(); }

static void ui_boot(void)
{
    int i;
    ui.group = GR_HOME;
    ui.mod_src = -1;
    ui.toast_p = -2;
    for (i = 0; i < 4; i++)
        ui.rec_lane[i] = -1;
    ui.boot = plat_ms();
    ui.strip_dirty = ui.head_dirty = 1;
}

/* the settings are written a while after the last change (the flash wears) */
static void settings_idle(uint32_t now)
{
    if (ui.set_dirty && now - ui.set_changed > 3000u) {
        ui.set_dirty = 0;
        settings_save();
    }
}
