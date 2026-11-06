/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 GRAINE contributors */
/* The host simulator's checks: "expect WHAT VALUE" in the app's tests (host/plat_host.c). */
int app_expect(const char *what, int *got)
{
    int i;
    if (!strcmp(what, "page"))
        *got = cur_page();
    else if (!strcmp(what, "preset"))
        *got = cur_preset;
    else if (!strcmp(what, "dirty"))
        *got = ui.dirty;
    else if (!strcmp(what, "voices")) {               /* sounding voices */
        *got = 0;
        for (i = 0; i < G_MAXV; i++)
            *got += GV.vlev[i] != 0;
    } else if (!strcmp(what, "grains"))
        *got = GV.n;
    else if (!strcmp(what, "dropped"))
        *got = GV.grains_dropped;
    else if (!strcmp(what, "tapis"))
        *got = tapis_on();
    else if (!strcmp(what, "freeze"))
        *got = GP.freeze;
    else if (!strcmp(what, "pads"))
        *got = (int)GP.pads;
    else if (!strcmp(what, "running"))
        *got = GP.running;
    else if (!strcmp(what, "capturing"))
        *got = GV.cap_on;
    else if (!strcmp(what, "cap_len"))
        *got = (int)GV.cap_len;
    else if (!strcmp(what, "octave"))
        *got = GP.octave;
    else if (!strcmp(what, "lanes")) {
        *got = 0;
        for (i = 0; i < G_NLANE; i++)
            *got += G_LANE[i].on;
    } else if (!strcmp(what, "mods")) {
        *got = 0;
        for (i = 0; i < G_NMOD; i++)
            *got += G.mod[i].dst < NP && G.mod[i].depth;
    } else if (!strcmp(what, "overlay"))
        *got = ui.ov;
    else if (!strcmp(what, "peak"))
        *got = GV.peak;
    else if (!strcmp(what, "user_len0"))
        *got = (int)G_SRC[G_SRC_CAPT + 1].len;
    else if (!strncmp(what, "p.", 2)) {               /* p.N: parameter N's raw value */
        int p = atoi(what + 2);
        if (p < 0 || p >= NP)
            return 0;
        *got = G.v[p];
    } else if (!strncmp(what, "mod.", 4)) {           /* mod.SRC.P: a modulation's depth */
        int s = atoi(what + 4), p;
        const char *d = strchr(what + 4, '.');
        if (!d)
            return 0;
        p = atoi(d + 1);
        *got = mod_depth(s, p);
    } else
        return 0;
    return 1;
}
