#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 GRAINE contributors
"""GRAINE's voice pack: whispers, an Indian raga sung over a tanpura, the tanpura alone, an OM mantra and overtone
(throat) singing, synthesized from scratch (no recorded voice, no third-party sample): a glottal source, the vocal
tract as formant resonators, whispered consonants from shaped noise, gamakas and meends as pitch curves, the
tanpura's jawari as a band of harmonics that sweeps up and down each pluck.

They are not built into the firmware (its flash is full): each is a WAV that goes into a user slot U1..U4, from
the emulator's or the site's importer ("Pack voix"), or by USB with tools/graine_import.py. Lengths fit a slot at
the rate the importer picks (65,504 bytes: 2.97 s at 22,050 Hz, 4.09 s at 16,000, 5.94 s at 11,025).

    python3 tools/gen_voices.py [--out app/sounds/voix]"""
import argparse
import json
import math
import struct
import wave
from pathlib import Path

import numpy as np
from scipy import signal

FS = 32000
RNG = np.random.default_rng(20261117)
ROOT = Path(__file__).resolve().parents[1]

# formants (Hz) of sung / spoken vowels, an adult male voice (Peterson and Barney, rounded); F4, F5 fixed
VOWEL = {"a": (730, 1090, 2440), "e": (530, 1840, 2480), "i": (300, 2250, 3000), "o": (570, 840, 2410),
         "u": (320, 870, 2240), "@": (500, 1500, 2500)}
BW = (80, 90, 120, 180, 220)


def t_axis(sec):
    return np.arange(int(sec * FS)) / FS


def norm(x, peak=0.89):
    x = x - np.mean(x)
    return x * (peak / (np.max(np.abs(x)) + 1e-12))


def fades(x, a=0.02, r=0.08):
    n = len(x)
    na, nr = max(1, int(a * FS)), max(1, int(r * FS))
    e = np.ones(n)
    e[:na] = np.linspace(0, 1, na) ** 2
    e[n - nr:] = np.linspace(1, 0, nr) ** 2
    return x * e


def reson(x, f, bw):
    """a Klatt resonator: unity gain at DC, peak at f"""
    r = math.exp(-math.pi * bw / FS)
    b1 = 2 * r * math.cos(2 * math.pi * min(f, FS * 0.47) / FS)
    b2 = -r * r
    return signal.lfilter([1 - b1 - b2], [1, -b1, -b2], x)


def tract(x, formants, scale=1.0, bw_mul=1.0, gains=(1.0, 0.8, 0.5, 0.3, 0.2)):
    """the vocal tract, as parallel formant resonators (parallel: each formant's level its own)"""
    fs = list(formants) + [3300, 3750]
    y = np.zeros_like(x)
    for k, (f, g) in enumerate(zip(fs, gains)):
        y += g * reson(x, f * (scale if k < 3 else 1.0), BW[k] * bw_mul)
    return y


def tract_tv(x, track, block=64, bw_mul=1.0, gains=(1.0, 0.8, 0.5, 0.3, 0.2)):
    """the vocal tract whose formants move: track(t) -> (F1, F2, F3); resonators run in blocks, their state kept"""
    n = len(x)
    y = np.zeros(n)
    zi = [np.zeros(2) for _ in range(5)]
    for s in range(0, n, block):
        seg = x[s:s + block]
        fs = list(track(s / FS)) + [3300, 3750]
        for k in range(5):
            r = math.exp(-math.pi * BW[k] * bw_mul / FS)
            b1 = 2 * r * math.cos(2 * math.pi * min(fs[k], FS * 0.47) / FS)
            b2 = -r * r
            out, zi[k] = signal.lfilter([1 - b1 - b2], [1, -b1, -b2], seg, zi=zi[k])
            y[s:s + block] += gains[k] * out
    return y


def glottal(f0, oq=0.6, tilt=0.0, jitter=0.004, shimmer=0.03):
    """the glottal flow's derivative for a pitch curve f0 (Hz, a value a sample): a smooth open phase and a sharp
    closure, a little jitter and shimmer. tilt > 0: softer (less high end)"""
    n = len(f0)
    jit = 1 + jitter * np.cumsum(RNG.normal(0, 1, n)) / math.sqrt(FS)
    ph = np.cumsum(f0 * jit / FS)
    frac = ph % 1.0
    tp, tn = oq * 0.66, oq                          # the flow's peak, its closure
    flow = np.where(frac < tp, 0.5 - 0.5 * np.cos(np.pi * frac / tp),
                    np.where(frac < tn, np.cos(0.5 * np.pi * (frac - tp) / (tn - tp)), 0.0))
    cyc = np.floor(ph).astype(int)
    shim = 1 + shimmer * RNG.normal(0, 1, cyc.max() + 2)[cyc]
    d = np.diff(flow * shim, prepend=0.0) * FS / np.maximum(f0, 30)
    if tilt > 0:
        d = signal.lfilter([1 - tilt], [1, -tilt], d)
    return d


def pitch_curve(notes, sa, fs=FS):
    """notes: [(semitones from Sa, seconds held, glide seconds into it, ornament)] -> f0 a sample. Ornaments:
    'andolan' (a slow swing, the komal note a little low), 'kampit' (vibrato), 'gamak' (a fast shake to the note
    above), None"""
    out = []
    prev = notes[0][0]
    for semis, dur, glide, orn in notes:
        n = int(dur * fs)
        t = np.arange(n) / fs
        g = min(glide, dur * 0.8)
        s = np.full(n, float(semis))
        if g > 0:
            k = t < g
            s[k] = prev + (semis - prev) * (0.5 - 0.5 * np.cos(np.pi * t[k] / g))   # meend: a cosine glide
        if orn == "andolan":
            s += -0.18 + 0.32 * np.sin(2 * np.pi * 1.6 * t) * np.clip((t - g) / 0.15, 0, 1)
        elif orn == "kampit":
            s += 0.16 * np.sin(2 * np.pi * 5.6 * t) * np.clip((t - g - 0.1) / 0.3, 0, 1)
        elif orn == "gamak":
            s += 0.9 * np.clip(np.sin(2 * np.pi * 6.5 * t), 0, 1) * np.clip((t - g) / 0.06, 0, 1)
        out.append(s)
        prev = semis
    s = np.concatenate(out)
    return sa * 2 ** (s / 12)


# --- the tanpura: four strings, plucked in turn, each a band of harmonics that sweeps (the jawari) --------------
def tanpura_pluck(f, sec, bright=1.0):
    t = t_axis(sec)
    nh = int(min(60, (FS * 0.45) / f))
    y = np.zeros(len(t))
    c = 6 + 26 * (1 - np.exp(-t / 0.7)) - 10 * np.clip((t - 1.6) / 2.0, 0, 1)   # the swept band's centre
    for n in range(1, nh + 1):
        base = n ** -0.9
        band = np.exp(-((n - c) / 3.2) ** 2) * 1.6 * bright       # the jawari's moving buzz
        dec = np.exp(-t / (3.2 / (1 + 0.05 * n)))
        y += (base + band * base ** 0.3) * dec * np.sin(2 * np.pi * f * n * t * (1 + 0.00004 * n * n) +
                                                       RNG.random() * 6.28)
    att = np.clip(t / 0.004, 0, 1)
    return y * att


def tanpura_bed(sec, sa):
    """Pa (the fifth below), Sa, Sa, low Sa: the classic four-string cycle, ~0.5 s apart, round and round"""
    n = int(sec * FS)
    out = np.zeros(n + FS * 4)
    strings = (sa * 0.75, sa * 1.0, sa * 1.0015, sa * 0.5)            # (the two Sa a hair apart: they beat)
    at, k = 0.0, 0
    while at < sec:
        f = strings[k % 4]
        p = tanpura_pluck(f, 4.0, 0.8 + 0.4 * RNG.random()) * (0.8 if k % 4 == 3 else 1.0)
        i = int(at * FS)
        out[i:i + len(p)] += p
        at += 0.46 + 0.04 * RNG.random() + (0.25 if k % 4 == 3 else 0.0)
        k += 1
    pre = tanpura_pluck(strings[3], 4.0) * 0.7                         # the cycle already ringing at 0
    pre = pre[int(1.2 * FS):]
    out[:len(pre)] += pre
    return out[:n]


def tanpura(sec=5.8, sa=138.59):
    return fades(norm(tanpura_bed(sec, sa)), 0.03, 0.25)


# --- the raga: Bhairav, sung on "aa" over the tanpura --------------------------------------------------------
def raga(sec=4.0, sa=138.59):
    # S, a meend to G, m, P held (kampit), komal dha with andolan, back to P, G m, komal re (andolan), S
    phrase = [(0, 0.45, 0.0, None), (4, 0.38, 0.16, None), (5, 0.28, 0.08, None), (7, 0.78, 0.12, "kampit"),
              (8, 0.62, 0.14, "andolan"), (7, 0.32, 0.22, "gamak"), (4, 0.26, 0.12, None), (5, 0.18, 0.06, None),
              (1, 0.48, 0.12, "andolan"), (0, 0.6, 0.2, "kampit")]
    f0 = pitch_curve(phrase, sa * 2)          # the middle octave
    n = int(sec * FS)
    f0 = np.concatenate([f0, np.full(max(0, n - len(f0)), f0[-1])])[:n]
    src = glottal(f0, oq=0.62, tilt=0.35, jitter=0.003, shimmer=0.025)
    breath = signal.lfilter(*signal.butter(2, [600, 5000], "bandpass", fs=FS), RNG.normal(0, 1, n)) * 0.012
    t = t_axis(sec)
    vowel = np.array(VOWEL["a"], float)
    voice = tract(src + breath, vowel, scale=1.0, gains=(1.0, 0.85, 0.55, 0.35, 0.22))
    voice += 0.3 * tract(src, VOWEL["@"], scale=1.0, gains=(0.0, 0.0, 0.5, 0.6, 0.4))   # the singer's formant
    env = np.clip(t / 0.18, 0, 1) * (0.75 + 0.25 * np.clip((t - 1.0) / 0.6, 0, 1)) * np.clip((sec - t) / 0.35, 0, 1)
    voice = norm(voice * env)
    bed = norm(tanpura_bed(sec, sa)) * 0.32
    return fades(norm(voice + bed), 0.01, 0.2)


# --- whispers: three voices murmuring a made-up language -----------------------------------------------------
CONS = {   # a whispered consonant: (noise band low, high, peak, length s, level)
    "s": (4200, 10500, 6500, 0.11, 0.8), "ch": (2000, 6000, 3000, 0.11, 0.8), "f": (1200, 9000, 5000, 0.08, 0.35),
    "h": None, "t": (2500, 9000, 4500, 0.018, 1.0), "k": (1500, 4500, 2300, 0.025, 0.9),
    "p": (400, 3000, 1200, 0.015, 0.7), "m": None, "": None}


def fricative(kind, n):
    lo, hi, pk, _, lvl = CONS[kind]
    x = RNG.normal(0, 1, n)
    x = signal.lfilter(*signal.butter(2, [lo, min(hi, FS * 0.46)], "bandpass", fs=FS), x)
    x = x + 0.8 * reson(x, pk, pk * 0.25)
    return x * lvl


def whisper_voice(sec, scale, level):
    n = int(sec * FS)
    out = np.zeros(n + FS)
    at = RNG.uniform(0, 0.25)
    syll = [("s", "i"), ("ch", "a"), ("p", "u"), ("f", "e"), ("s", "o"), ("k", "i"), ("t", "a"), ("h", "u"),
            ("m", "e"), ("", "o"), ("ch", "i"), ("s", "e"), ("t", "o"), ("h", "a")]
    while at < sec:
        c, v = syll[RNG.integers(len(syll))]
        i = int(at * FS)
        piece = []
        if CONS.get(c):
            ln = int(CONS[c][3] * FS * RNG.uniform(0.8, 1.3))
            fr = fricative(c, ln)
            e = np.hanning(ln * 2)[:ln] if c in "sfch" else np.exp(-np.arange(ln) / (ln * 0.3))
            piece.append(fr * (e if c in ("s", "f", "ch") else e))
        vl = int(FS * RNG.uniform(0.09, 0.2))
        asp = RNG.normal(0, 1, vl)
        asp = signal.lfilter([1, -0.6], [1], asp)                          # aspiration, a little brighter
        vow = tract(asp, VOWEL[v], scale=scale, bw_mul=2.4, gains=(0.7, 1.0, 0.8, 0.45, 0.3))
        vow *= np.sin(np.linspace(0, np.pi, vl)) ** 0.7
        if c == "h":
            vow[: vl // 3] *= 1.3
        if c == "m":
            vow *= 0.7
        piece.append(vow * 0.9)
        if RNG.random() < 0.25:                                             # a final s / ch
            k = "s" if RNG.random() < 0.6 else "ch"
            ln = int(0.08 * FS)
            piece.append(fricative(k, ln) * np.hanning(ln * 2)[ln:])
        seg = np.concatenate(piece)
        out[i:i + len(seg)] += seg * level * RNG.uniform(0.7, 1.15)
        at += len(seg) / FS + (RNG.uniform(0.25, 0.6) if RNG.random() < 0.18 else RNG.uniform(0.01, 0.06))
    return out[:n]


def chuchote(sec=2.9):
    n = int(sec * FS)
    x = whisper_voice(sec, 1.0, 1.0) + whisper_voice(sec, 1.17, 0.8) + whisper_voice(sec, 0.92, 0.65)
    inhale = signal.lfilter(*signal.butter(2, [500, 3500], "bandpass", fs=FS), RNG.normal(0, 1, n))
    t = t_axis(sec)
    inhale *= 0.25 * np.exp(-((t - 1.55) / 0.12) ** 2)                     # a breath in, mid-way
    return fades(norm(x + inhale), 0.03, 0.12)


# --- the mantra: OM, a low group, two breaths -------------------------------------------------------------------
def om_voice(sec, f, start, length, sub=False):
    n = int(sec * FS)
    t = t_axis(sec)
    vib = 1 + 0.004 * np.sin(2 * np.pi * (4.2 + RNG.random()) * t + RNG.random() * 6)
    f0 = f * vib * (1 + 0.002 * RNG.normal())
    src = glottal(f0, oq=0.55, tilt=0.5, jitter=0.004, shimmer=0.05)
    if sub:                                                                 # period doubling: the Tibetan growl
        src *= 1 + 0.5 * np.sin(np.pi * np.cumsum(f0) / FS)
    close = start + length * 0.55                                           # "o" closes to "m" here
    m = np.clip((t - close) / 0.18, 0, 1)
    o = tract(src, VOWEL["o"], gains=(1.0, 0.7, 0.35, 0.2, 0.1))
    hum = reson(src, 260, 70) * 1.4 + reson(src, 2200, 300) * 0.08          # the nasal murmur
    hum = signal.lfilter(*signal.iirnotch(1100, 2.0, fs=FS), hum)
    y = o * (1 - m) + hum * m
    env = np.clip((t - start) / 0.25, 0, 1) * np.clip((start + length - t) / 0.3, 0, 1)
    return y * env


def mantra(sec=5.8):
    out = np.zeros(int(sec * FS))
    for start in (0.05, 3.0):                                               # two OMs, a breath between
        for f, lvl, sub in ((73.42, 1.0, True), (73.6, 0.8, False), (110.0, 0.55, False), (146.83, 0.6, False),
                            (147.2, 0.45, False)):
            s = start + RNG.uniform(0, 0.09)
            out += lvl * om_voice(sec, f, s, 2.65 + RNG.uniform(-0.05, 0.05), sub)
    t = t_axis(sec)
    breath = signal.lfilter(*signal.butter(2, [300, 2500], "bandpass", fs=FS), RNG.normal(0, 1, len(t)))
    out += 0.05 * breath * np.exp(-((t - 2.85) / 0.1) ** 2)
    return fades(norm(out), 0.02, 0.2)


# --- overtone singing: a drone, and a whistle that picks its harmonics -----------------------------------------
def gorge(sec=4.0, f=110.0):
    n = int(sec * FS)
    t = t_axis(sec)
    f0 = f * (1 + 0.003 * np.sin(2 * np.pi * 5.0 * t))
    src = glottal(f0, oq=0.42, tilt=0.0, jitter=0.002, shimmer=0.02)       # a pressed, bright voice
    melody = [(8, 0.0), (9, 0.55), (10, 1.0), (12, 1.45), (10, 2.0), (9, 2.35), (8, 2.7), (6, 3.1), (8, 3.5)]

    def h_at(tt):                                                          # the harmonic sung at tt
        for k in range(len(melody) - 1):
            a, _ = melody[k]
            b, tb = melody[k + 1]
            if tt < tb:
                if tt >= tb - 0.09:                                         # a 90 ms glide into the next
                    g = (tt - (tb - 0.09)) / 0.09
                    return a + (b - a) * (0.5 - 0.5 * math.cos(math.pi * g))
                return a
        return melody[-1][0]

    y = tract(src, (420, 1000, 2000), gains=(0.8, 0.0, 0.0, 0.15, 0.08))   # F1 and the high ones; F2 below
    sharp = np.zeros(n)                                                     # the whistle: a very narrow F2
    zi = np.zeros(2)
    for s in range(0, n, 32):
        fpk = f * h_at(s / FS)
        r = math.exp(-math.pi * 14 / FS)
        b1, b2 = 2 * r * math.cos(2 * math.pi * fpk / FS), -r * r
        out, zi = signal.lfilter([1 - b1 - b2], [1, -b1, -b2], src[s:s + 32], zi=zi)
        sharp[s:s + 32] = out
    x = norm(y) * 0.6 + norm(sharp) * 0.75
    return fades(norm(x), 0.08, 0.25)


VOICES = [  # file, name on the FM-1 (9 characters), what it is, maker
    ("chuchote", "CHUCHOTE", "chuchotements à trois voix, consonnes et souffles", chuchote),
    ("raga", "RAGA", "raga Bhairav chanté sur un tanpura (gamakas, meends)", raga),
    ("tanpura", "TANPURA", "le tanpura seul : Pa, Sa, Sa, Sa grave, et sa jawari", tanpura),
    ("mantra", "MANTRA OM", "OM à cinq voix graves, avec le grondement tibétain", mantra),
    ("gorge", "DIPHONIE", "chant diphonique : un bourdon et une mélodie d'harmoniques", gorge),
]


def trim_lead(x):
    """the near-silence before the sound, cut here as the importer would (|x| < 0.003 at a peak of 0.89), so the
    pack arrives whole; then a 2 ms fade in"""
    x = x * (0.89 / (np.max(np.abs(x)) + 1e-12))
    k = int(np.argmax(np.abs(x) >= 0.006))
    x = x[k:].copy()
    n = int(0.002 * FS)
    x[:n] *= np.linspace(0, 1, n)
    return x


SLOT_BYTES = 65504
RATES = (22050, 16000, 11025, 8000)


def slot_rate(seconds):
    """the rate the importer picks for this length (tools/graine_import.py --rate auto)"""
    for r in RATES:
        if seconds * r <= SLOT_BYTES:
            return r
    return RATES[-1]


def write_wav(path, x, rate):
    """16-bit PCM at the slot's own rate: the importer keeps it as it is (no second resampling)"""
    g = math.gcd(rate, FS)
    y = signal.resample_poly(x, rate // g, FS // g)
    y = y * (0.89 / (np.max(np.abs(y)) + 1e-12))
    q = np.round(np.clip(y, -1, 1) * 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(q.tobytes())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "app" / "sounds" / "voix"))
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    index = []
    for file, name, what, fn in VOICES:
        x = trim_lead(fn())
        sec = len(x) / FS
        rate = slot_rate(sec)
        write_wav(out / f"{file}.wav", x, rate)
        index.append({"file": f"{file}.wav", "name": name, "what": what, "seconds": round(sec, 2), "rate": rate})
        print(f"{name:10s} {sec:5.2f} s  {rate:5d} Hz  {(out / (file + '.wav')).stat().st_size // 1024:4d} KB")
    (out / "index.json").write_text(json.dumps(index, ensure_ascii=False, indent=1) + "\n")


if __name__ == "__main__":
    main()
