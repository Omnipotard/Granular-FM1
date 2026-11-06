/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 GRAINE contributors */
/* Every parameter: its labels in French and English, its range, its unit; the pages; how a value reads.
 * Part of the unity build (app.c). */

/* lists: French, English in turn */
static const char *const E_SCAN[] = {"BOUCLE", "LOOP", "ALLER-R", "PINGPNG", "1 FOIS", "ONCE", "IVRE", "DRUNK",
                                     "SAUTE", "JUMP"};
static const char *const E_RETRIG[] = {"RELANCE", "RESTART", "LIBRE", "FREE", "COMMUN", "SHARED"};
static const char *const E_QUANT[] = {"LIBRE", "FREE", "DEMI-T", "SEMI", "OCTAVE", "OCTAVE", "QUINTE", "FIFTH",
                                      "MAJEUR", "MAJOR", "MINEUR", "MINOR", "GAMME", "SCALE"};
static const char *const E_TIMING[] = {"REGUL", "EVEN", "ALEA", "RANDOM", "SYNC", "SYNC"};
static const char *const E_SHAPE[] = {"SINUS", "SINE", "TRIANG", "TRI", "RAMPE+", "RAMP UP", "RAMPE-", "RAMP DN",
                                      "CARRE", "SQUARE", "HASARD", "S&H", "DERIVE", "DRIFT"};
static const char *const E_SYNC[] = {"LIBRE", "FREE", "TEMPO", "TEMPO", "LENT", "SLOW"};
static const char *const E_LMODE[] = {"LIBRE", "FREE", "NOTE", "NOTE"};
static const char *const E_DDIV[] = {"1/128", "1/128", "1/64", "1/64", "1/32", "1/32", "1/16T", "1/16T", "1/16",
                                     "1/16", "1/8T", "1/8T", "1/16.", "1/16.", "1/8", "1/8", "1/4T", "1/4T", "1/8.",
                                     "1/8.", "1/4", "1/4", "1/2T", "1/2T", "1/4.", "1/4."};
static const char *const E_ARP[] = {"OFF", "OFF", "MONTE", "UP", "DESCEND", "DOWN", "M+D", "UP+DN", "ALEA",
                                    "RANDOM", "ORDRE", "ORDER"};
static const char *const E_ARPDIV[] = {"1/4", "1/4", "1/8", "1/8", "1/8T", "1/8T", "1/16", "1/16", "1/16T",
                                       "1/16T", "1/32", "1/32"};
static const char *const E_SCALE[] = {"CHROM", "CHROM", "MAJEUR", "MAJOR", "MINEUR", "MINOR", "DORIEN", "DORIAN",
                                      "PHRYG", "PHRYG", "LYDIEN", "LYDIAN", "MIXO", "MIXO", "PENTA+", "PENTA+",
                                      "PENTA-", "PENTA-", "BLUES", "BLUES"};
static const char *const E_NOTE[] = {"C", "C", "C#", "C#", "D", "D", "D#", "D#", "E", "E", "F", "F", "F#", "F#",
                                     "G", "G", "G#", "G#", "A", "A", "A#", "A#", "B", "B"};
static const char *const E_REC[] = {"1 FOIS", "ONCE", "BOUCLE", "LOOP", "SEUIL", "THRESH"};
static const char *const E_CHOP[] = {"OFF", "OFF", "BEGAIE", "STUTTER", "ROLL", "ROLL", "FREIN", "TAPESTOP",
                                     "ENVOL", "SPEEDUP", "MELANGE", "MIXED"};
static const char *const E_CHDIV[] = {"1/4", "1/4", "1/8", "1/8", "1/16", "1/16", "1/32", "1/32"};
static const char *const E_SHINT[] = {"OCTAVE", "OCTAVE", "QUINTE", "FIFTH", "OCT+5TE", "OCT+5TH", "OCT BAS",
                                      "OCT DOWN"};
static const char *const E_GATE[] = {"OFF", "OFF", "CROCHES", "8THS", "SYNCOPE", "SYNCO", "TRESILLO", "TRESILLO",
                                     "GALOP", "GALLOP", "HACHE", "CHOPPED", "PULSE", "PULSE", "ALEA", "RANDOM"};
static const char *const E_ONOFF[] = {"OFF", "OFF", "ON", "ON"};
static const char *const E_GEST[] = {"1 MES", "1 BAR", "2 MES", "2 BARS", "4 MES", "4 BARS", "8 MES", "8 BARS"};
static const char *const E_TARGET[] = {"NUAGE", "CLOUD", "GRAIN", "GRAIN", "SON", "SOUND", "TOUT", "ALL"};
static const char *const E_LANG[] = {"FRANCAIS", "FRANCAIS", "ENGLISH", "ENGLISH"};

#define C_ PF_ENUM | PF_NOMOD
const pdef_t PDEF[NP] = {
    /* NUAGE */
    [P_POS] = {{"POS", "POS"}, {"POSITION", "POSITION"}, 0, 1000, 250, U_PERMIL, 0, 0},
    [P_SIZE] = {{"TAILLE", "SIZE"}, {"TAILLE DU GRAIN", "GRAIN SIZE"}, 0, 200, 118, U_MS, 0, 0},
    [P_DENS] = {{"DENSITE", "DENSITY"}, {"GRAINS PAR SECONDE", "GRAINS PER SECOND"}, 0, 200, 112, U_HZ, 0, 0},
    [P_SPRAY] = {{"FLOU", "SPRAY"}, {"FLOU DE POSITION", "POSITION SPRAY"}, 0, 100, 8, U_PCT, 0, 0},
    /* JEU */
    [P_PITCH] = {{"HAUTEUR", "PITCH"}, {"HAUTEUR", "PITCH"}, -24, 24, 0, U_ST, 0, 0},
    [P_SPREAD] = {{"ESSAIM", "SPREAD"}, {"ECART DE HAUTEUR", "PITCH SPREAD"}, 0, 100, 0, U_ST, 0, 0},
    [P_REV] = {{"ENVERS", "REVERSE"}, {"GRAINS A L'ENVERS", "REVERSED GRAINS"}, 0, 100, 0, U_PCT, 0, 0},
    [P_STEREO] = {{"STEREO", "STEREO"}, {"ECART STEREO", "STEREO SPREAD"}, 0, 100, 45, U_PCT, 0, 0},
    /* SOURCE */
    [P_SAMPLE] = {{"SOURCE", "SOURCE"}, {"SOURCE", "SOURCE"}, 0, G_NSRC - 1, 0, U_ENUM, PF_NOMOD, 0},
    [P_START] = {{"DEBUT", "START"}, {"DEBUT DE FENETRE", "WINDOW START"}, 0, 1000, 0, U_PERMIL, 0, 0},
    [P_LEN] = {{"FENETRE", "WINDOW"}, {"LONGUEUR DE FENETRE", "WINDOW LENGTH"}, 10, 1000, 1000, U_PERMIL, 0, 0},
    [P_FINE] = {{"ACCORD", "TUNE"}, {"ACCORD FIN", "FINE TUNE"}, -100, 100, 0, U_CENT, 0, 0},
    /* BALAYAGE */
    [P_SCAN] = {{"MODE", "MODE"}, {"MODE DE BALAYAGE", "SCAN MODE"}, 0, 4, 0, U_ENUM, C_, E_SCAN},
    [P_SPEED] = {{"VITESSE", "SPEED"}, {"VITESSE DE LECTURE", "SCAN SPEED"}, -400, 400, 0, U_X, 0, 0},
    [P_RETRIG] = {{"DEPART", "RETRIG"}, {"A CHAQUE NOTE", "ON EACH NOTE"}, 0, 2, 0, U_ENUM, C_, E_RETRIG},
    [P_KEYPOS] = {{"TOUCHE", "KEY>POS"}, {"NOTE VERS POSITION", "KEY TO POSITION"}, -100, 100, 0, U_SIGNED, 0, 0},
    /* GRAIN */
    [P_WIN] = {{"FORME", "SHAPE"}, {"FORME DU GRAIN", "GRAIN SHAPE"}, 0, 100, 33, U_PCT, 0, 0},
    [P_SKEW] = {{"PENTE", "SKEW"}, {"PENTE DU GRAIN", "GRAIN SKEW"}, -100, 100, 0, U_SIGNED, 0, 0},
    [P_QUANT] = {{"QUANT", "QUANT"}, {"QUANTIF. DE L'ESSAIM", "SPREAD QUANTIZE"}, 0, 6, 0, U_ENUM, C_, E_QUANT},
    [P_TIMING] = {{"RYTHME", "TIMING"}, {"RYTHME DES GRAINS", "GRAIN TIMING"}, 0, 2, 0, U_ENUM, C_, E_TIMING},
    /* TEXTURE */
    [P_JITTER] = {{"SCINTIL", "JITTER"}, {"SCINTILLEMENT", "AMPLITUDE JITTER"}, 0, 100, 0, U_PCT, 0, 0},
    [P_SHIMMER] = {{"ECLAT", "SHIMMER"}, {"GRAINS A L'OCTAVE", "OCTAVE GRAINS"}, 0, 100, 0, U_PCT, 0, 0},
    [P_GLIDE] = {{"GLISSE", "GLIDE"}, {"GLISSE DANS LE GRAIN", "GLIDE IN GRAIN"}, -240, 240, 0, U_ST, 0, 0},
    [P_SLEW] = {{"LISSAGE", "SLEW"}, {"LISSAGE DE POSITION", "POSITION SLEW"}, 0, 100, 25, U_PCT, 0, 0},
    /* AMPLI */
    [P_AATK] = {{"ATTAQUE", "ATTACK"}, {"ATTAQUE", "ATTACK"}, 0, 100, 14, U_TIME, 0, 0},
    [P_ADEC] = {{"DECLIN", "DECAY"}, {"DECLIN", "DECAY"}, 0, 100, 50, U_TIME, 0, 0},
    [P_ASUS] = {{"TENUE", "SUSTAIN"}, {"TENUE", "SUSTAIN"}, 0, 100, 85, U_PCT, 0, 0},
    [P_AREL] = {{"RELACHE", "RELEASE"}, {"RELACHE", "RELEASE"}, 0, 100, 55, U_TIME, 0, 0},
    /* ENV MOD */
    [P_MATK] = {{"ATTAQUE", "ATTACK"}, {"ENV MOD ATTAQUE", "MOD ENV ATTACK"}, 0, 100, 40, U_TIME, 0, 0},
    [P_MDEC] = {{"DECLIN", "DECAY"}, {"ENV MOD DECLIN", "MOD ENV DECAY"}, 0, 100, 62, U_TIME, 0, 0},
    [P_MSUS] = {{"TENUE", "SUSTAIN"}, {"ENV MOD TENUE", "MOD ENV SUSTAIN"}, 0, 100, 0, U_PCT, 0, 0},
    [P_MREL] = {{"RELACHE", "RELEASE"}, {"ENV MOD RELACHE", "MOD ENV RELEASE"}, 0, 100, 55, U_TIME, 0, 0},
    /* LFO 1 */
    [P_L1RATE] = {{"VITESSE", "RATE"}, {"LFO 1 VITESSE", "LFO 1 RATE"}, 0, 100, 42, U_RATE, 0, 0},
    [P_L1SHAPE] = {{"FORME", "SHAPE"}, {"LFO 1 FORME", "LFO 1 SHAPE"}, 0, 6, 0, U_ENUM, C_, E_SHAPE},
    [P_L1SYNC] = {{"SYNC", "SYNC"}, {"LFO 1 SYNC", "LFO 1 SYNC"}, 0, 2, 0, U_ENUM, C_, E_SYNC},
    [P_L1MODE] = {{"DEPART", "RETRIG"}, {"LFO 1 DEPART", "LFO 1 RETRIG"}, 0, 1, 0, U_ENUM, C_, E_LMODE},
    /* LFO 2 */
    [P_L2RATE] = {{"VITESSE", "RATE"}, {"LFO 2 VITESSE", "LFO 2 RATE"}, 0, 100, 22, U_RATE, 0, 0},
    [P_L2SHAPE] = {{"FORME", "SHAPE"}, {"LFO 2 FORME", "LFO 2 SHAPE"}, 0, 6, 6, U_ENUM, C_, E_SHAPE},
    [P_L2SYNC] = {{"SYNC", "SYNC"}, {"LFO 2 SYNC", "LFO 2 SYNC"}, 0, 2, 0, U_ENUM, C_, E_SYNC},
    [P_L2MODE] = {{"DEPART", "RETRIG"}, {"LFO 2 DEPART", "LFO 2 RETRIG"}, 0, 1, 0, U_ENUM, C_, E_LMODE},
    /* FILTRE */
    [P_FTYPE] = {{"TYPE", "TYPE"}, {"TYPE DE FILTRE", "FILTER TYPE"}, 0, 100, 0, U_PCT, 0, 0},
    [P_CUT] = {{"COUPURE", "CUTOFF"}, {"FREQUENCE DE COUPURE", "CUTOFF"}, 0, 200, 200, U_HZ, 0, 0},
    [P_RES] = {{"RESON", "RESO"}, {"RESONANCE", "RESONANCE"}, 0, 100, 12, U_PCT, 0, 0},
    [P_DRIVE] = {{"SATUR", "DRIVE"}, {"SATURATION", "DRIVE"}, 0, 100, 0, U_PCT, 0, 0},
    /* ECHO */
    [P_DTIME] = {{"TEMPS", "TIME"}, {"TEMPS D'ECHO", "ECHO TIME"}, 0, 12, 7, U_DIV, C_, E_DDIV},
    [P_DFB] = {{"RETOUR", "FEEDBK"}, {"REINJECTION", "FEEDBACK"}, 0, 110, 38, U_PCT, 0, 0},
    [P_DTONE] = {{"TEINTE", "TONE"}, {"TEINTE D'ECHO", "ECHO TONE"}, 0, 100, 55, U_PCT, 0, 0},
    [P_DMIX] = {{"DOSE", "MIX"}, {"DOSE D'ECHO", "ECHO MIX"}, 0, 100, 16, U_PCT, 0, 0},
    /* ESPACE */
    [P_RSIZE] = {{"TAILLE", "SIZE"}, {"TAILLE DE L'ESPACE", "SPACE SIZE"}, 0, 100, 65, U_PCT, 0, 0},
    [P_RDEC] = {{"DUREE", "DECAY"}, {"DUREE DE L'ESPACE", "SPACE DECAY"}, 0, 100, 52, U_TIME, 0, 0},
    [P_RDAMP] = {{"AMORTI", "DAMP"}, {"AMORTI", "DAMPING"}, 0, 100, 45, U_PCT, 0, 0},
    [P_RMIX] = {{"DOSE", "MIX"}, {"DOSE D'ESPACE", "SPACE MIX"}, 0, 100, 26, U_PCT, 0, 0},
    /* MATIERE */
    [P_BITS] = {{"BITS", "BITS"}, {"RESOLUTION", "BIT DEPTH"}, 0, 100, 0, U_INT, 0, 0},
    [P_DOWN] = {{"RABOT", "DOWNSMP"}, {"SOUS-ECHANTILLON", "DOWNSAMPLE"}, 0, 100, 0, U_X, 0, 0},
    [P_WIDTH] = {{"LARGEUR", "WIDTH"}, {"LARGEUR STEREO", "STEREO WIDTH"}, 0, 200, 100, U_PCT, 0, 0},
    [P_LEVEL] = {{"VOLUME", "LEVEL"}, {"VOLUME DU SON", "PATCH LEVEL"}, 0, 100, 50, U_DB, 0, 0},
    /* ARPEGE */
    [P_ARP] = {{"ARPEGE", "ARP"}, {"ARPEGE", "ARPEGGIO"}, 0, 5, 0, U_ENUM, C_, E_ARP},
    [P_ARPDIV] = {{"PAS", "RATE"}, {"PAS D'ARPEGE", "ARP RATE"}, 0, 5, 3, U_DIV, C_, E_ARPDIV},
    [P_ARPOCT] = {{"OCTAVES", "OCTAVES"}, {"OCTAVES", "OCTAVES"}, 1, 4, 1, U_INT, PF_NOMOD, 0},
    [P_ARPGATE] = {{"DUREE", "GATE"}, {"DUREE DES NOTES", "NOTE GATE"}, 5, 100, 60, U_PCT, PF_NOMOD, 0},
    /* GLOBAL */
    [P_VOICES] = {{"VOIX", "VOICES"}, {"POLYPHONIE", "POLYPHONY"}, 1, G_MAXV, 4, U_INT, PF_NOMOD, 0},
    [P_PORTA] = {{"PORTAM", "GLIDE"}, {"PORTAMENTO", "PORTAMENTO"}, 0, 100, 0, U_TIME, 0, 0},
    [P_SCALE] = {{"GAMME", "SCALE"}, {"GAMME", "SCALE"}, 0, G_NSCALE - 1, 0, U_ENUM, C_, E_SCALE},
    [P_TONIC] = {{"TONIQUE", "KEY"}, {"TONIQUE", "KEY"}, 0, 11, 0, U_ENUM, C_, E_NOTE},
    /* HORLOGE */
    [P_TEMPO] = {{"TEMPO", "TEMPO"}, {"TEMPO", "TEMPO"}, 40, 240, 110, U_BPM, PF_GLOBAL | PF_NOMOD, 0},
    [P_VELO] = {{"VELOC", "VELO"}, {"SENSIB. VELOCITE", "VELOCITY SENS"}, 0, 100, 60, U_PCT, PF_NOMOD, 0},
    [P_RECMODE] = {{"CAPTURE", "CAPTURE"}, {"MODE DE CAPTURE", "CAPTURE MODE"}, 0, 2, 0, U_ENUM, PF_GLOBAL | C_, E_REC},
    [P_GESTLEN] = {{"GESTES", "GESTURE"}, {"BOUCLE DE GESTES", "GESTURE LOOP"}, 0, 3, 1, U_ENUM, PF_GLOBAL | C_, E_GEST},
    /* HASARD */
    [P_DICE] = {{"LANCER", "ROLL"}, {"LANCER LES DES", "ROLL THE DICE"}, 0, 999, 0, U_INT,
                PF_GLOBAL | PF_NOMOD | PF_ACTION, 0},
    [P_DOSE] = {{"DOSE", "AMOUNT"}, {"DOSE DE HASARD", "CHANCE AMOUNT"}, 0, 100, 35, U_PCT, PF_GLOBAL | PF_NOMOD, 0},
    [P_TARGET] = {{"CIBLE", "TARGET"}, {"CIBLE DU HASARD", "CHANCE TARGET"}, 0, 3, 0, U_ENUM, PF_GLOBAL | C_, E_TARGET},
    [P_LANG] = {{"LANGUE", "LANGUAGE"}, {"LANGUE", "LANGUAGE"}, 0, 1, 0, U_ENUM, PF_GLOBAL | C_, E_LANG},
    /* DRONE */
    [P_STRETCH] = {{"ETIRE", "STRETCH"}, {"GRAINS ETIRES", "GRAIN STRETCH"}, 0, 100, 0, U_X, 0, 0},
    [P_DETUNE] = {{"CHOEUR", "CHORUS"}, {"VOIX DESACCORDEES", "DETUNED VOICES"}, 0, 100, 0, U_PCT, 0, 0},
    [P_SRC2] = {{"SOURC B", "SRC B"}, {"SOURCE B", "SOURCE B"}, 0, G_NSRC - 1, 1, U_ENUM, PF_NOMOD, 0},
    [P_XFADE] = {{"MELANGE", "BLEND"}, {"MELANGE A > B", "BLEND A > B"}, 0, 100, 0, U_PCT, 0, 0},
    /* CHAOS */
    [P_CHAOS] = {{"CHAOS", "CHAOS"}, {"GRAINS FOUS", "WILD GRAINS"}, 0, 100, 0, U_PCT, 0, 0},
    [P_CHOP] = {{"HACHE", "CHOP"}, {"HACHOIR AUTO", "AUTO CHOP"}, 0, 5, 0, U_ENUM, C_, E_CHOP},
    [P_CHDIV] = {{"PAS", "STEP"}, {"PAS DU HACHOIR", "CHOP STEP"}, 0, 3, 2, U_DIV, C_, E_CHDIV},
    [P_CHPROB] = {{"CHANCE", "CHANCE"}, {"CHANCE DU HACHOIR", "CHOP CHANCE"}, 0, 100, 35, U_PCT, 0, 0},
    /* HALO */
    [P_SHIM] = {{"SHIMMER", "SHIMMER"}, {"ESPACE QUI MONTE", "RISING SPACE"}, 0, 100, 0, U_PCT, 0, 0},
    [P_SHINT] = {{"INTERV", "INTERV"}, {"INTERVALLE SHIMMER", "SHIMMER INTERVAL"}, 0, 3, 0, U_ENUM, C_, E_SHINT},
    [P_RESO] = {{"RESONAT", "RESONAT"}, {"RESONATEURS ACCORDES", "TUNED RESONATORS"}, 0, 100, 0, U_PCT, 0, 0},
    [P_INFINI] = {{"INFINI", "HOLD"}, {"ESPACE INFINI", "SPACE HOLD"}, 0, 100, 0, U_PCT, 0, 0},
    /* HACHOIR */
    [P_GATE] = {{"MASQUE", "GATE"}, {"MASQUE RYTHMIQUE", "RHYTHM GATE"}, 0, 7, 0, U_ENUM, C_, E_GATE},
    [P_SWING] = {{"SWING", "SWING"}, {"SWING", "SWING"}, 0, 100, 0, U_PCT, 0, 0},
    [P_RING] = {{"ANNEAU", "RING"}, {"MODULATION EN ANNEAU", "RING MOD"}, 0, 100, 0, U_PCT, 0, 0},
    [P_FOLD] = {{"PLIAGE", "FOLD"}, {"REPLI DE L'ONDE", "WAVEFOLD"}, 0, 100, 0, U_PCT, 0, 0},
    /* SCENES */
    [P_MORPH] = {{"MORPH", "MORPH"}, {"SCENE A > B", "SCENE A > B"}, 0, 100, 0, U_PCT, 0, 0},
    [P_TAKEB] = {{"PRENDRE", "TAKE B"}, {"PRENDRE LA SCENE B", "TAKE SCENE B"}, 0, 999, 0, U_INT,
                 PF_NOMOD | PF_ACTION, 0},
    [P_MUTB] = {{"MUTER B", "MUTATE"}, {"MUTER LA SCENE B", "MUTATE SCENE B"}, 0, 999, 0, U_INT,
                PF_NOMOD | PF_ACTION, 0},
    [P_AUTOM] = {{"AUTO", "AUTO"}, {"MORPH AUTOMATIQUE", "AUTO MORPH"}, 0, 100, 0, U_PCT, 0, 0},
    /* PLUIE */
    [P_RAIN] = {{"PLUIE", "RAIN"}, {"NOTES QUI TOMBENT", "FALLING NOTES"}, 0, 100, 0, U_PCT, 0, 0},
    [P_RRANGE] = {{"ETENDUE", "RANGE"}, {"ETENDUE (OCTAVES)", "RANGE (OCTAVES)"}, 1, 4, 2, U_INT, PF_NOMOD, 0},
    [P_RLEN] = {{"DUREE", "LENGTH"}, {"DUREE DES GOUTTES", "DROP LENGTH"}, 0, 100, 50, U_TIME, 0, 0},
    [P_LATCH] = {{"TENUE", "LATCH"}, {"TOUCHES TENUES", "LATCHED KEYS"}, 0, 1, 0, U_ENUM, C_, E_ONOFF},
};

/* the pages: four knobs each; a button opens its first page, then its next ones */
enum { GR_HOME, GR_SEL, GR_EDIT, GR_ENV, GR_LFO, GR_FX, GR_ARP, GR_GLO, NGR };
typedef struct {
    const char *name[2];
    uint8_t p[4];
} gpage_t;
static const gpage_t PAGES[] = {
    {{"NUAGE", "CLOUD"}, {P_POS, P_SIZE, P_DENS, P_SPRAY}},
    {{"JEU", "PLAY"}, {P_PITCH, P_SPREAD, P_REV, P_STEREO}},
    {{"SCENES", "SCENES"}, {P_MORPH, P_TAKEB, P_MUTB, P_AUTOM}},
    {{"SOURCE", "SOURCE"}, {P_SAMPLE, P_START, P_LEN, P_FINE}},
    {{"BALAYAGE", "SCAN"}, {P_SCAN, P_SPEED, P_RETRIG, P_KEYPOS}},
    {{"GRAIN", "GRAIN"}, {P_WIN, P_SKEW, P_QUANT, P_TIMING}},
    {{"TEXTURE", "TEXTURE"}, {P_JITTER, P_SHIMMER, P_GLIDE, P_SLEW}},
    {{"DRONE", "DRONE"}, {P_STRETCH, P_DETUNE, P_SRC2, P_XFADE}},
    {{"CHAOS", "CHAOS"}, {P_CHAOS, P_CHOP, P_CHDIV, P_CHPROB}},
    {{"AMPLI", "AMP ENV"}, {P_AATK, P_ADEC, P_ASUS, P_AREL}},
    {{"ENV MOD", "MOD ENV"}, {P_MATK, P_MDEC, P_MSUS, P_MREL}},
    {{"LFO 1", "LFO 1"}, {P_L1RATE, P_L1SHAPE, P_L1SYNC, P_L1MODE}},
    {{"LFO 2", "LFO 2"}, {P_L2RATE, P_L2SHAPE, P_L2SYNC, P_L2MODE}},
    {{"FILTRE", "FILTER"}, {P_FTYPE, P_CUT, P_RES, P_DRIVE}},
    {{"ECHO", "ECHO"}, {P_DTIME, P_DFB, P_DTONE, P_DMIX}},
    {{"ESPACE", "SPACE"}, {P_RSIZE, P_RDEC, P_RDAMP, P_RMIX}},
    {{"HALO", "HALO"}, {P_SHIM, P_SHINT, P_RESO, P_INFINI}},
    {{"MATIERE", "MATTER"}, {P_BITS, P_DOWN, P_WIDTH, P_LEVEL}},
    {{"HACHOIR", "CHOPPER"}, {P_GATE, P_SWING, P_RING, P_FOLD}},
    {{"ARPEGE", "ARP"}, {P_ARP, P_ARPDIV, P_ARPOCT, P_ARPGATE}},
    {{"PLUIE", "RAIN"}, {P_RAIN, P_RRANGE, P_RLEN, P_LATCH}},
    {{"GLOBAL", "GLOBAL"}, {P_VOICES, P_PORTA, P_SCALE, P_TONIC}},
    {{"HORLOGE", "CLOCK"}, {P_TEMPO, P_VELO, P_RECMODE, P_GESTLEN}},
    {{"HASARD", "CHANCE"}, {P_DICE, P_DOSE, P_TARGET, P_LANG}},
};
#define NPAGES (sizeof PAGES / sizeof PAGES[0])
static const uint8_t GROUP_FIRST[NGR] = {0, 3, 5, 9, 11, 13, 19, 21};
static const uint8_t GROUP_N[NGR] = {3, 2, 4, 2, 2, 6, 2, 3};
static const uint8_t GROUP_BTN[NGR] = {B_HOME, B_SEL, B_EDIT, B_ENV, B_LFO, B_FX, B_ARP, B_GLO};

/* the language: 0 French, 1 English */
#define LG ((int)(G.v[P_LANG] != 0))
#define TR(fr, en) (LG ? (en) : (fr))

/* ---------------------------------------------------------------------------------------- text helpers --- */
static char *s_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
    return d;
}
static void s_int(int32_t v, char *b, int sign)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    else if (sign && v > 0)
        *b++ = '+';
    while (n)
        *b++ = t[--n];
    *b = 0;
}
/* v with d decimals (v already scaled by 10^d) */
static void s_fix(int32_t v, int d, char *b, int sign)
{
    char t[16];
    int32_t p = d == 2 ? 100 : d == 1 ? 10 : 1;
    int32_t a = v < 0 ? -v : v;
    b[0] = 0;
    if (v < 0)
        s_cat(b, "-");
    else if (sign && v > 0)
        s_cat(b, "+");
    s_int(a / p, t, 0);
    s_cat(b, t);
    if (d) {
        int32_t f = a % p;
        s_cat(b, ".");
        if (d == 2 && f < 10)
            s_cat(b, "0");
        s_int(f, t, 0);
        s_cat(b, t);
    }
}
static int32_t rnd(float x) { return (int32_t)(x + (x < 0 ? -0.5f : 0.5f)); }

static void fmt_secs(float s, char *b)
{
    if (s < 0.0995f)
        s_fix(rnd(s * 10000.0f), 1, b, 0), s_cat(b, "ms");
    else if (s < 0.9995f)
        s_int(rnd(s * 1000.0f), b, 0), s_cat(b, "ms");
    else if (s < 9.95f)
        s_fix(rnd(s * 100.0f), 2, b, 0), s_cat(b, "s");
    else
        s_fix(rnd(s * 10.0f), 1, b, 0), s_cat(b, "s");
}

static const char *src_name(int i)
{
    const gsrc_t *s = &G_SRC[clamp(i, 0, G_NSRC - 1)];
    if (s->len < 64u && i != G_SRC_CAPT)
        return TR("VIDE", "EMPTY");
    return s->name;
}

/* how parameter p's raw value reads on the screen */
static void pfmt(int p, int32_t raw, char *b)
{
    const pdef_t *d = &PDEF[p];
    float n = pnorm_raw(p, raw), v;
    b[0] = 0;
    if (p == P_SAMPLE || p == P_SRC2) {
        s_cat(b, src_name(raw));
        return;
    }
    if (d->enums) {
        s_cat(b, d->enums[2 * (raw - d->min) + LG]);
        return;
    }
    v = pval(p, n);
    switch (p) {
    case P_DENS:
        if (G.v[P_TIMING] == 2) {
            static const char *const D[10] = {"1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T",
                                              "1/64"};
            s_cat(b, D[clamp((int32_t)(n * 9.99f), 0, 9)]);
            return;
        }
        if (v < 9.95f)
            s_fix(rnd(v * 10.0f), 1, b, 0);
        else
            s_int(rnd(v), b, 0);
        s_cat(b, "/s");
        return;
    case P_SIZE: case P_AATK: case P_ADEC: case P_AREL: case P_MATK: case P_MDEC: case P_MREL:
        fmt_secs(v, b);
        return;
    case P_PORTA:
        if (v <= 0.0f)
            s_cat(b, "OFF");
        else
            fmt_secs(v, b);
        return;
    case P_RDEC:
        if (v > 30.0f)
            s_cat(b, TR("INFINI", "FREEZE"));
        else
            fmt_secs(v, b);
        return;
    case P_PITCH:
        s_int(raw, b, 1);
        s_cat(b, raw ? " ST" : " ST");
        return;
    case P_SPREAD:
        s_fix(rnd(v * 10.0f), 1, b, 0);
        s_cat(b, " ST");
        return;
    case P_GLIDE:
        s_fix(raw, 1, b, 1);
        s_cat(b, " ST");
        return;
    case P_FINE:
        s_int(raw, b, 1);
        s_cat(b, " CT");
        return;
    case P_SPEED:
        if (raw == 0) {
            s_cat(b, TR("GEL", "FROZEN"));
            return;
        }
        s_cat(b, "x");
        {
            char t[12];
            s_fix(raw, 2, t, 1);
            s_cat(b, t);
        }
        return;
    case P_KEYPOS: case P_SKEW:
        s_int(raw, b, 1);
        s_cat(b, "%");
        return;
    case P_WIN: {
        static const char *const W[4] = {"GAUSS", "HANN", "TUKEY", "RECT"};
        int k = clamp(rnd(n * 3.0f), 0, 3);
        s_cat(b, W[k]);
        return;
    }
    case P_POS: case P_START: case P_LEN:
        s_fix(raw, 1, b, 0);
        s_cat(b, "%");
        return;
    case P_L1RATE: case P_L2RATE:
        if (G.v[p == P_L1RATE ? P_L1SYNC : P_L2SYNC] == 2)
            v *= 0.05f;                              /* LENT: a twentieth */
        if (G.v[p == P_L1RATE ? P_L1SYNC : P_L2SYNC] == 1) {
            static const char *const D[11] = {"8 MES", "4 MES", "2 MES", "1 MES", "1/2", "1/4", "1/8", "1/8T",
                                              "1/16", "1/16T", "1/32"};
            s_cat(b, D[clamp((int32_t)(n * 10.99f), 0, 10)]);
            return;
        }
        if (v < 0.995f)
            s_fix(rnd(v * 100.0f), 2, b, 0);
        else if (v < 9.95f)
            s_fix(rnd(v * 10.0f), 1, b, 0);
        else
            s_int(rnd(v), b, 0);
        s_cat(b, "Hz");
        return;
    case P_CUT:
        if (v >= 999.5f) {
            s_fix(rnd(v / 100.0f), 1, b, 0);
            s_cat(b, "k");
        } else {
            s_int(rnd(v), b, 0);
        }
        s_cat(b, "Hz");
        return;
    case P_FTYPE: {
        static const char *const F[5] = {"PB", "PB>BD", "BANDE", "BD>PH", "PH"};
        static const char *const FE[5] = {"LP", "LP>BP", "BP", "BP>HP", "HP"};
        int k = clamp(rnd(n * 4.0f), 0, 4);
        s_cat(b, LG ? FE[k] : F[k]);
        return;
    }
    case P_BITS:
        if (raw == 0) {
            s_cat(b, "16 BIT");
            return;
        }
        s_fix(rnd((16.0f - 13.5f * n) * 10.0f), 1, b, 0);
        s_cat(b, " BIT");
        return;
    case P_DOWN:
        if (raw == 0) {
            s_cat(b, "OFF");
            return;
        }
        s_cat(b, "/");
        {
            char t[12];
            s_fix(rnd(v * 10.0f), 1, t, 0);
            s_cat(b, t);
        }
        return;
    case P_TEMPO:
        s_int(raw, b, 0);
        return;
    case P_LEVEL:                                    /* x2 at the top: +6 dB; 50: 0 dB */
        if (raw == 0) {
            s_cat(b, "-INF");
            return;
        }
        s_fix(rnd(20.0f * fm_log2f(n * 2.0f) * 0.30103f * 10.0f), 1, b, 1);
        s_cat(b, " DB");
        return;
    case P_DICE: case P_TAKEB: case P_MUTB:
        s_cat(b, TR("TOURNE", "TURN"));
        return;
    case P_STRETCH:
        s_cat(b, "x");
        {
            char t[12];
            s_fix(rnd(v * 10.0f), 1, t, 0);
            s_cat(b, t);
        }
        return;
    case P_RLEN:
        fmt_secs(v, b);
        return;
    case P_RAIN: case P_AUTOM:
        if (raw == 0) {
            s_cat(b, "OFF");
            return;
        }
        if (p == P_AUTOM) {                          /* a sweep's length */
            fmt_secs(1.0f / v, b);
            return;
        }
        s_fix(rnd(v * 10.0f), 1, b, 0);
        s_cat(b, "/s");
        return;
    case P_ARPOCT: case P_VOICES: case P_RRANGE:
        s_int(raw, b, 0);
        return;
    default:
        s_int(rnd(n * (p == P_DFB ? 110.0f : p == P_WIDTH ? 200.0f : 100.0f)), b, 0);
        s_cat(b, "%");
        return;
    }
}
