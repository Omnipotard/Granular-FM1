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

#ifdef PLAT_HOST
#include "host.c"
#endif

#ifdef PLAT_WEB
/* the emulator page's own calls (app/web/emu): a sound dropped into a user slot of the data area, then chosen */
__attribute((used, visibility("default"))) void graine_rescan(void) { slots_scan(); }
__attribute((used, visibility("default"))) void graine_source(int i)
{
    G.v[P_SAMPLE] = (int16_t)clamp(i, 0, G_NSRC - 1);
    ui.dirty = 1;
    ui.strip_dirty = ui.head_dirty = 1;
    toast_param(P_SAMPLE);
}
#endif
