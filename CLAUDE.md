# GRAINE : notes pour Claude Code

Firmware granulaire pour le M-VAVE FM-1 (JieLi AC79, 360 MHz, écran 240×240, 7 encodeurs, 27 touches,
12 boutons + OCT−/+), bâti sur fm1-base (`base/`, sous-module git, MPL-2.0 ; felucca-bsp dans `base/bsp/`).
Le code de GRAINE est sous MIT. Le README décrit l'instrument côté musicien ; ce fichier décrit le code.

## État

- Tout compile et tourne dans le **simulateur** (`./build.sh --host`) et l'**émulateur web** (`./build.sh --web`).
  `sh tests/run_tests.sh` : 58 vérifications (`features.txt`, `drone_glitch.txt`), toutes vertes.
- **Jamais compilé ni essayé sur un vrai FM-1** : la toolchain JieLi n'était pas accessible pendant l'écriture.
  Priorité 1 : un build device (CI GitHub, ou WSL avec la toolchain) puis un essai sur la machine.
- Charge mesurée en pire cas (64 grains, 6 voix, effets) : 1,7 % d'un cœur Xeon ; estimation 25–45 % d'un cœur AC79.

## Carte du code

| Fichier | Rôle |
|---|---|
| `app/graine.h` | ce que moteur et interface partagent : paramètres (`P_*`), sources, événements, snapshot `GV`, état de jeu `GP` |
| `app/dsp/engine.c` | le son, unité séparée compilée en -O2 (`fm1.toml o2`), appelée par l'interruption audio. Grains, voix, balayage, fenêtres, modulations, filtre SVF, écho ping-pong, réverb FDN, matière, capture, arpège, horloge, gestes |
| `app/ui/params.c` | table `PDEF` (libellés FR/EN, plages, unités), `PAGES`, formatage des valeurs (`pfmt`) |
| `app/ui/store.c` | réglages, banque de 16 patchs en deux objets de 8 (`OBJ_BANK` 0xD8000, `OBJ_BANK2` 0xDA000 : un objet fait 3 840 o au plus), 16 presets usine (`FACTORY`), emplacements U1–U4 dans la zone data (0x97000 + i×0x10000, en-tête 32 o + µ-law) |
| `app/ui/draw.c` | rendu par bandes de 80 lignes sur le canvas de fm1-base : mélange, lueurs additives, anneaux AA, texte |
| `app/ui/screen.c` | l'écran : en-tête, nuage (forme d'onde + grains), aperçu, TAPIS, toasts, overlays, bande des 4 knobs, splash |
| `app/ui/viz.c` | le dessin de chaque page en surimpression sur le nuage (enveloppes, LFO, filtre, écho, espace, grain, balayage, matière, arpège, gamme, horloge, scènes, drone, chaos, halo, hachoir, pluie), choisi par le 1er knob de la page ; inclus par screen.c |
| `app/ui/ui.c` | le panneau : boutons (tap / maintien / combos), encodeurs, touches, MIDI, LEDs, modulation par geste, gestes, dés |
| `app/app.c` | build unity côté interface + exports web (`graine_rescan`, `graine_source`) |
| `app/host.c` | les `expect` du simulateur |
| `app/tests/features.txt` | test scripté de toutes les fonctions |
| `app/sounds/bank.h` | généré par `tools/gen_sounds.py` (8 sources µ-law 22 050 Hz, ~300 Ko) |
| `app/web/emu/graine.html` + `tools/emu_page.py` | la page de l'émulateur (le worklet de fm1-base + ses parts + la part `graine`) |
| `tools/emcc-wasi/` | un `emcc` de remplacement : clang + sysroot WASI (sans Emscripten) |

## Règles du FM-1 (à respecter, le build device les vérifie)

- **Pas de `double`**, pas de libc côté device (ni `<math.h>` ni `<string.h>`) : `fastmath.h` (`fm_expf`,
  `fm_exp2f`, `fm_tanhf`, `fm_sin_turns`…). Littéraux flottants avec `f`.
- **Pas de multiplications 64 bits** (bug CPU documenté dans `base/docs/CPU.md`) ; pas de division 64 bits.
- Le moteur tourne dans l'interruption audio : pas d'allocation, pas d'appel bloquant. L'interface lui parle par
  `eng_event()` (file SPSC) et par les mots de `G` / `GP` (écritures atomiques 32 bits).
- RAM : `ble = false`, capture 128 Ko + écho 106 Ko + réverb 30 Ko dans `PLAT_POOL`, canvas 240×80.
- Flash : écrire seulement depuis la boucle principale (`plat_data_*`, `plat_store_save`), le son s'arrête pendant.
- Vérifier le moteur sur une cible nue :
  `clang --target=riscv32-unknown-elf -march=rv32imafc -mabi=ilp32f -ffreestanding -nostdlibinc -isystem $(clang -print-resource-dir)/include -O2 -Wdouble-promotion -Ibase/include -Iapp -c app/dsp/engine.c`
  → seul symbole externe attendu : `PDEF`.

## Pages pour drones et glitchs

24 réglages sur 6 pages (fin de l'enum `P_*`) ; le moteur les lit dans `gn[]` / `vn[]` comme les autres :

- **SCENES** (HOME, 3e) : `P_MORPH` mélange chaque réglage du patch vers `G_SCENEB` (RAM, non sauvé) dans
  `params_chunk` ; `P_TAKEB` (action : prend B, à l'envers échange A et B), `P_MUTB` (action : B muté, dose de
  HASARD), `P_AUTOM` (balaye A→B tout seul).
- **DRONE** (EDIT, 3e) : `P_STRETCH` (grains ×1..×8, 16 s au plus), `P_DETUNE` (voix désaccordées et écartées),
  `P_SRC2` + `P_XFADE` (une part des grains lit la source B au même endroit).
- **CHAOS** (EDIT, 4e) : `P_CHAOS` (par grain : envers, ±octave, clic, saut, glissando, trou) ; `P_CHOP` / `P_CHDIV`
  / `P_CHPROB` : `chop_tick`, à chaque pas de l'horloge, par chance, bégaie / roll / frein / envol (`tape`).
- **HALO** (FX, 4e) : `P_SHIM` + `P_SHINT` (pitch-shifter 2 têtes réinjecté dans l'espace, `sh_buf` 4 Ko),
  `P_RESO` (3 peignes accordés sur la note, sa quinte, la tierce de la gamme, `rs_buf` 9 Ko), `P_INFINI`.
- **HACHOIR** (FX, 6e) : `P_GATE` (8 motifs de 16 pas, `gate_tick`), `P_SWING`, `P_RING`, `P_FOLD`.
- **PLUIE** (ARP, 2e) : `P_RAIN` / `P_RRANGE` / `P_RLEN` (`rain_tick` : notes de la gamme au hasard, clés 400+),
  `P_LATCH` (une touche reste, se relâche à la frappe suivante).
- Ajouts : LFO `SYNC` = LENT (÷20, cycles de plusieurs minutes) ; capture `SEUIL` (démarre au premier son).

Pool ajouté : ~13 Ko (`sh_buf`, `rs_buf`). À vérifier sur la machine : la charge CPU avec HALO + HACHOIR actifs.

## Commandes

```sh
git submodule update --init --recursive
./build.sh --host && sh tests/run_tests.sh
app/build/host/graine_host app/tests/features.txt /tmp/out   # PNG + WAV dans /tmp/out
tools/emcc-wasi/get_wasi.sh && PATH="$PWD/tools/emcc-wasi:$PATH" ./build.sh --web
python3 -m http.server -d app/build/emu/site 8000           # puis http://localhost:8000
```

Langage des scripts de test : voir l'en-tête de `base/host/plat_host.c` (`key`, `press`, `tap`, `turn`, `spin`,
`wait`, `shot`, `wav`, `expect`, `power`…). Sous Windows, le simulateur et la CI passent par WSL.

## Pistes d'amélioration (par ordre de valeur)

1. **Essai sur le vrai FM-1** : CPU réel (`plat_cpu_pct`), lisibilité (les textes de 10 px — infos, pads, chips —
   sont probablement trop petits sur 1,54″), vélocité des touches silicone, cadence d'affichage.
2. **Envoi de samples par USB** sans émulateur : une commande `APP_USB_CMD` + une page WebMIDI/SysEx qui écrit U1–U4.
3. **Deuxième cœur** (felucca-bsp `cpu2`) pour les grains : 128 grains ou une interpolation Hermite.
4. Scènes A/B avec morphing, noms de patchs éditables, sortie MIDI (horloge, notes de l'arpège).
5. Capture stéréo / plus longue si la RAM le permet ; seuil de déclenchement de la capture.
6. Grains : filtre par grain ou décalage de formant ; mode « spectral freeze ».
