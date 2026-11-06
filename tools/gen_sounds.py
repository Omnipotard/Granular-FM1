#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 GRAINE contributors
"""GRAINE's built-in sources, synthesized from scratch (no recorded material, no third-party samples).

Writes app/sounds/bank.h: eight sources as 8-bit mu-law at 22,050 Hz (random access: one table lookup a read),
and, with --wav DIR, a WAV of each for listening.

    python3 tools/gen_sounds.py [--wav DIR]

Every source is made to be granulated: long, evolving, with something to find wherever a grain lands."""
import argparse
import math
import struct
import wave
from pathlib import Path

import numpy as np
from scipy import signal

SR = 22050
RNG = np.random.default_rng(20261106)
ROOT = Path(__file__).resolve().parents[1]


def t_axis(sec):
    return np.arange(int(sec * SR)) / SR


def norm(x, peak=0.89):
    x = x - np.mean(x)
    return x * (peak / (np.max(np.abs(x)) + 1e-9))


def fade(x, a=0.01, r=0.04):
    n = len(x)
    na, nr = int(a * SR), int(r * SR)
    env = np.ones(n)
    env[:na] = np.linspace(0, 1, na) ** 2
    env[n - nr:] = np.linspace(1, 0, nr) ** 2
    return x * env


def bandpass(x, f, q):
    b, a = signal.iirpeak(min(f, SR * 0.45), q, fs=SR)
    return signal.lfilter(b, a, x)


def onepole_lp(x, fc):
    a = math.exp(-2 * math.pi * fc / SR)
    return signal.lfilter([1 - a], [1, -a], x)


# --- 1. VOIX: a small choir sliding through vowels -----------------------------------------------------------
VOWELS = {  # F1, F2, F3 (Hz), sung, roughly
    "a": (800, 1150, 2900), "o": (450, 800, 2830), "u": (325, 700, 2700),
    "e": (400, 1620, 2400), "i": (290, 1870, 2800)}


def voix(sec=2.4):
    t = t_axis(sec)
    n = len(t)
    out = np.zeros(n)
    for f0, pan_w in ((146.83, 1.0), (220.0, 0.8), (293.66, 0.7), (369.99, 0.55)):   # D3 A3 D4 F#4
        vib = 1 + 0.006 * np.sin(2 * math.pi * (5.1 + RNG.random()) * t + RNG.random() * 6)
        drift = 1 + 0.003 * np.cumsum(RNG.normal(0, 1, n)) / math.sqrt(n)
        ph = np.cumsum(f0 * vib * drift / SR)
        src = 2 * (ph % 1.0) - 1                              # a bright glottal-ish saw
        src = onepole_lp(src, 3800)
        breath = RNG.normal(0, 0.18, n)
        x = src + breath
        seq = ["a", "o", "u", "e", "i", "a"]
        seg = n // (len(seq) - 1)
        y = np.zeros(n)
        for k in range(len(seq) - 1):                          # morph the formants vowel to vowel
            a, b = VOWELS[seq[k]], VOWELS[seq[k + 1]]
            lo, hi = k * seg, n if k == len(seq) - 2 else (k + 1) * seg
            chunk = x[lo:hi]
            m = np.linspace(0, 1, hi - lo)
            for j, (fa, fb) in enumerate(zip(a, b)):
                g = (1.0, 0.6, 0.3)[j]
                ya = bandpass(chunk, fa, 7) * (1 - m)
                yb = bandpass(chunk, fb, 7) * m
                y[lo:hi] += g * (ya + yb)
        out += pan_w * y
    return fade(norm(out), 0.25, 0.3)


# --- 2. VERRE: struck glass and bells, inharmonic, beating ---------------------------------------------------
def verre(sec=1.8):
    t = t_axis(sec)
    out = np.zeros(len(t))
    ratios = (1.0, 2.32, 4.25, 6.63, 9.38, 12.9)
    notes = (659.25, 783.99, 987.77, 587.33, 880.0)          # E5 G5 B5 D5 A5
    for k, f in enumerate(notes):
        start = int(k * 0.32 * SR)
        tt = t[: len(t) - start]
        tone = np.zeros(len(tt))
        for j, r in enumerate(ratios):
            dec = 2.2 / (1 + 0.9 * j)
            beat = 1 + 0.0015 * (j + 1)
            amp = 1 / (1 + j) ** 1.1
            tone += amp * np.exp(-tt / dec) * (np.sin(2 * math.pi * f * r * tt) + 0.5 * np.sin(2 * math.pi * f * r * beat * tt))
        click = np.exp(-tt * 900) * RNG.normal(0, 0.3, len(tt))
        out[start:] += tone * (0.9 - 0.08 * k) + click
    return fade(norm(out), 0.002, 0.2)


# --- 3. CORDE: plucked strings (Karplus-Strong) with a wooden body -------------------------------------------
def ks(f, n, bright=0.5):
    p = int(SR / f)
    buf = RNG.uniform(-1, 1, p)
    buf = onepole_lp(buf, 1500 + 6000 * bright)
    y = np.zeros(n)
    idx = 0
    prev = 0.0
    for i in range(n):
        v = buf[idx]
        nv = 0.5 * (v + buf[(idx + 1) % p]) * 0.996
        buf[idx] = nv
        y[i] = v
        idx = (idx + 1) % p
        prev = v
    return y


def corde(sec=1.6):
    n = int(sec * SR)
    out = np.zeros(n)
    notes = (110.0, 164.81, 220.0, 261.63, 329.63, 392.0, 493.88)   # A2 E3 A3 C4 E4 G4 B4: Am9
    for k, f in enumerate(notes):
        start = int(k * 0.11 * SR)
        out[start:] += ks(f, n - start, 0.35 + 0.08 * k) * (1 - 0.06 * k)
    body = bandpass(out, 210, 3) * 0.6 + bandpass(out, 470, 4) * 0.4
    return fade(norm(out + 0.7 * body), 0.001, 0.1)


# --- 4. SOUFFLE: wind, gusts and a whistle ---------------------------------------------------------------------
def souffle(sec=1.4):
    t = t_axis(sec)
    n = len(t)
    noise = RNG.normal(0, 1, n)
    gust = 0.55 + 0.45 * np.sin(2 * math.pi * 0.9 * t + 1.0) * np.sin(2 * math.pi * 0.37 * t)
    y = np.zeros(n)
    hop = 512
    for c in (380, 900, 2100):                                  # resonances that wander
        f = c * (1 + 0.35 * np.sin(2 * math.pi * (0.6 + c / 5000) * t + c))
        for i in range(0, n, hop):
            seg = noise[i:i + hop]
            b, a = signal.iirpeak(f[i], 9, fs=SR)
            y[i:i + hop] += signal.lfilter(b, a, seg) / (1 + c / 1500)
    whistle = 0.12 * np.sin(2 * math.pi * np.cumsum(1320 * (1 + 0.02 * np.sin(2 * math.pi * 3 * t))) / SR)
    return fade(norm(y * gust + whistle * gust ** 2), 0.15, 0.25)


# --- 5. TAMBOUR: one bar of drums at 120 bpm -------------------------------------------------------------------
def tambour(sec=2.0):
    n = int(sec * SR)
    out = np.zeros(n)
    step = n // 16

    def kick(m):
        tt = np.arange(m) / SR
        f = 48 + 120 * np.exp(-tt * 28)
        return np.sin(2 * math.pi * np.cumsum(f) / SR) * np.exp(-tt * 7.5)

    def snare(m):
        tt = np.arange(m) / SR
        nz = signal.lfilter(*signal.butter(2, [900, 7000], "bandpass", fs=SR), RNG.normal(0, 1, m))
        return 0.7 * nz * np.exp(-tt * 18) + 0.5 * np.sin(2 * math.pi * 190 * tt) * np.exp(-tt * 25)

    def hat(m, open_=False):
        tt = np.arange(m) / SR
        nz = signal.lfilter(*signal.butter(2, 7000, "highpass", fs=SR), RNG.normal(0, 1, m))
        return 0.35 * nz * np.exp(-tt * (9 if open_ else 55))

    def rim(m):
        tt = np.arange(m) / SR
        return 0.5 * np.sin(2 * math.pi * 1700 * tt) * np.exp(-tt * 90)

    pat = {"k": "x.....x...x.....", "s": "....x.......x..x", "h": "x.x.x.x.x.x.xxx.", "o": "...............x",
           "r": ".......x..x....."}
    for s in range(16):
        at = s * step
        m = n - at
        if pat["k"][s] == "x":
            out[at:] += kick(m)
        if pat["s"][s] == "x":
            out[at:] += snare(m) * (0.6 if s == 15 else 1)
        if pat["h"][s] == "x":
            out[at:] += hat(m) * (1.0 if s % 4 == 0 else 0.6)
        if pat["o"][s] == "x":
            out[at:] += hat(m, True)
        if pat["r"][s] == "x":
            out[at:] += rim(m)
    return fade(norm(np.tanh(out * 1.3)), 0.0005, 0.02)


# --- 6. ORGUE: drawbars, a seventh chord, a slow rotor ----------------------------------------------------------
def orgue(sec=1.4):
    t = t_axis(sec)
    out = np.zeros(len(t))
    bars = ((0.5, 0.6), (1, 1.0), (2, 0.7), (3, 0.5), (4, 0.35), (6, 0.2), (8, 0.15))
    rot = 1 + 0.004 * np.sin(2 * math.pi * 6.3 * t)
    for f in (130.81, 196.0, 246.94, 329.63):                  # C3 G3 B3 E4: Cmaj7
        for r, a in bars:
            out += a * np.sin(2 * math.pi * np.cumsum(f * r * rot) / SR)
    am = 1 + 0.18 * np.sin(2 * math.pi * 6.3 * t + 1.3)
    return fade(norm(out * am), 0.03, 0.15)


# --- 7. PLUIE: droplets, plinks and crackle ---------------------------------------------------------------------
def pluie(sec=1.2):
    n = int(sec * SR)
    out = np.zeros(n)
    for _ in range(170):
        f = RNG.uniform(1200, 5200)
        m = int(SR * RNG.uniform(0.006, 0.05))
        at = RNG.integers(0, n - m)
        tt = np.arange(m) / SR
        fs = f * (1 + 0.6 * np.exp(-tt * 140))                  # a drop's chirp
        out[at:at + m] += RNG.uniform(0.2, 1) * np.sin(2 * math.pi * np.cumsum(fs) / SR) * np.exp(-tt * RNG.uniform(60, 220))
    crack = (RNG.random(n) > 0.997) * RNG.normal(0, 1.5, n)
    bed = onepole_lp(RNG.normal(0, 0.05, n), 5000)
    return fade(norm(out + crack + bed), 0.05, 0.1)


# --- 8. SPECTRE: an FM drone that slowly opens -------------------------------------------------------------------
def spectre(sec=1.8):
    t = t_axis(sec)
    f = 98.0                                                    # G2
    idx = 0.3 + 4.2 * (0.5 - 0.5 * np.cos(math.pi * t / sec))
    ratio = 1.41 + 0.02 * np.sin(2 * math.pi * 0.7 * t)
    mod = np.sin(2 * math.pi * np.cumsum(f * ratio) / SR)
    car = np.sin(2 * math.pi * f * t + idx * mod)
    car2 = np.sin(2 * math.pi * f * 2.005 * t + 0.6 * idx * mod)
    sub = 0.4 * np.sin(2 * math.pi * f * 0.5 * t)
    return fade(norm(car + 0.5 * car2 + sub), 0.2, 0.25)


SOURCES = [("VOIX", voix), ("VERRE", verre), ("CORDE", corde), ("SOUFFLE", souffle), ("TAMBOUR", tambour),
           ("ORGUE", orgue), ("PLUIE", pluie), ("SPECTRE", spectre)]


def mulaw_encode(x):
    """G.711-style mu-law, 8 bits (255 levels), from float -1..1"""
    mu = 255.0
    x = np.clip(x, -1, 1)
    y = np.sign(x) * np.log1p(mu * np.abs(x)) / np.log1p(mu)
    q = np.round((y + 1) * 127.5).astype(np.int32)
    return np.clip(q, 0, 255).astype(np.uint8)


def mulaw_table():
    mu = 255.0
    y = np.arange(256) / 127.5 - 1
    return np.sign(y) * (np.expm1(np.abs(y) * np.log1p(mu)) / mu)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wav", help="also write each source as a WAV here")
    a = ap.parse_args()
    lines = ["/* SPDX-License-Identifier: MIT */",
             "/* GRAINE's built-in sources: made by tools/gen_sounds.py (synthesized, no recorded material). Do not edit. */",
             "#pragma once", "#include <stdint.h>", f"#define SND_RATE {SR}u", f"#define SND_COUNT {len(SOURCES)}u", ""]
    total = 0
    names = []
    for name, fn in SOURCES:
        x = fn()
        q = mulaw_encode(x)
        total += len(q)
        names.append((name, len(q)))
        body = ",".join(str(int(v)) for v in q)
        lines.append(f"static const uint8_t SND_{name}[{len(q)}] = {{{body}}};")
        if a.wav:
            d = Path(a.wav)
            d.mkdir(parents=True, exist_ok=True)
            back = mulaw_table()[q]
            with wave.open(str(d / f"{name.lower()}.wav"), "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(SR)
                w.writeframes(struct.pack(f"<{len(back)}h", *np.round(back * 32767).astype(np.int16)))
        print(f"{name:8s} {len(q) / SR:5.2f} s  {len(q):6d} B")
    tab = ",".join(f"{v:.7f}f" for v in mulaw_table())
    lines.append(f"static const float SND_ULAW[256] = {{{tab}}};")
    lines.append("static const struct { const char *name; const uint8_t *data; uint32_t len; } SND_BANK[SND_COUNT] = {")
    for name, n in names:
        lines.append(f'    {{"{name}", SND_{name}, {n}u}},')
    lines.append("};")
    out = ROOT / "app" / "sounds" / "bank.h"
    out.write_text("\n".join(lines) + "\n")
    print(f"total    {total / SR:5.2f} s  {total} B -> {out}")


if __name__ == "__main__":
    main()
