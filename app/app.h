/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 GRAINE contributors */
/* GRAINE: the app's half of the contract with fm1-base (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "GRAINE"
#define APP_TAGLINE "GRANULAR"
#define APP_SPLASH_RGB RGB(255, 186, 72)
#define APP_BLOCK 128u                   /* frames per audio block: 2.9 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */
#define APP_CANVAS_PX (240u * 80u)       /* the screen drawn in three bands of 80 rows (ui/draw.c) */
#define APP_OVERLOAD                     /* the render over 90 % of its time: fewer grains for a moment */

/* the store: the settings, the patches, the panel's calibration (the platform's); the user sources are in the
 * data area from 0x97000 (ui/store.c), beside them */
enum { OBJ_SET, OBJ_BANK, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xD8000u, 0xDC000u}

/* the contract: the platform calls these */
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
void app_overload(void);

/* the host simulator's checks (host/plat_host.c "expect") */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
