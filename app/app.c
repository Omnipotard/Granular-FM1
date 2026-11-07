/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 GRAINE contributors */
/* GRAINE on fm1-base: the app's unity build, compiled into fm1-base's after the platform (gfx.c, announce.c and
 * lib/touch.c in scope). The sound is its own unit (dsp/engine.c, fm1.toml o2). */
#include "fastmath.h"
#include "graine.h"

static void dice_roll(int32_t det);
static int ui_scale_snap(int note);
static int lane_of(int p);
static int held(int b);
static int tapis_on(void);
static int mod_depth(int src, int dst);

#include "ui/params.c"
#include "ui/store.c"
#include "ui/draw.c"
#include "ui/ui.c"
#include "ui/screen.c"

void app_init(void)
{
    eng_init();
    eng_cores(plat_cpu2_set(1) == 0);             /* the second core: half of the grains */
    store_boot();
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12) { eng_render(lr, n, master_q12); }

void app_panic(void) { eng_panic(); }

void app_overload(void) { eng_overload(); }

void app_ui_init(void)
{
    touch_on(1);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui_boot();
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    input();
    if (ui.ov == OV_WRITING && now - ui.boot > 100u) {
        static uint32_t shown;
        if (!shown) {
            shown = now;                             /* "writing..." on the screen first */
        } else if (now - shown > 60u) {
            shown = 0;
            ov_write_now();
        }
    }
    if (GP.gest_rec)
        gest_write();
    settings_idle(now);
    screen_frame();
    if (ann_on() && ann_where())
        app_say(PAGES[cur_page()].name[LG], patch_name);
    ann_tick(now);
}

/* a sound from the computer into a user slot (store.c slot_import): the frame's body is the slot (0..3), the rate,
 * the length and the CRC-32 of the mu-law bytes (little-endian), and the name (10 bytes). Main loop: the sound stops
 * while the flash is written; the FM-1 reports at the end by reading 0xC0000000 (kept) or 0xCF000000 | code */
static uint32_t rd32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
void app_usb_cmd(const uint8_t *d, uint32_t n)
{
    uint8_t t[8];
    char name[10];
    int slot, rc, k;
    uint32_t len;
    if (n != 30u || d[2] != IMP_CMD)
        return;
    slot = d[6];
    len = rd32(d + 11);
    for (k = 0; k < 9; k++)
        name[k] = (char)d[19 + k];
    name[9] = 0;
    eng_event(EV_ALL_OFF, 0, 0);
    toast(TR("IMPORT USB", "USB IMPORT"), name, KNOB_RGB[2]);
    ui.head_dirty = 1;
    screen_frame();                              /* (the main loop waits for the transfer: say it first) */
    plat_host_patience(2000u, 120000u);
    rc = slot_import(slot, rd32(d + 7), len, rd32(d + 15), name, plat_host_read);
    plat_host_read(rc ? 0xCF000000u | (uint32_t)rc : 0xC0000000u, t, sizeof t);
    if (!rc && len) {
        G.v[P_SAMPLE] = (int16_t)(G_SRC_CAPT + 1 + slot);
        ui.dirty = 1;
        toast(TR("IMPORTE", "IMPORTED"), G_SRC[G_SRC_CAPT + 1 + slot].name, KNOB_RGB[2]);
    } else {
        toast(TR("IMPORT USB", "USB IMPORT"), rc ? TR("RATE", "FAILED") : TR("U EFFACE", "SLOT CLEARED"), rc ? K_REC : K_SOFT);
    }
    ui.strip_dirty = ui.head_dirty = 1;
}

#ifdef PLAT_HOST
#include "host.c"
#endif

#ifdef PLAT_WEB
/* the emulator page's own calls (app/web/emu): a sound dropped into a user slot of the data area, then chosen */
__attribute((used, visibility("default"))) void graine_rescan(void) { slots_scan(); }
__attribute((used, visibility("default"))) void graine_source(int i);
__attribute((used, visibility("default"))) void graine_user(int slot)   /* user slot 0..3 chosen as the source */
{
    graine_source(G_SRC_CAPT + 1 + clamp(slot, 0, G_NUSER - 1));
}
__attribute((used, visibility("default"))) void graine_source(int i)
{
    G.v[P_SAMPLE] = (int16_t)clamp(i, 0, G_NSRC - 1);
    ui.dirty = 1;
    ui.strip_dirty = ui.head_dirty = 1;
    toast_param(P_SAMPLE);
}
#endif
