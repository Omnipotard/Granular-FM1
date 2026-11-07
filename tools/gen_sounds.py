#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 GRAINE contributors
"""GRAINE's built-in sources, synthesized from scratch (no recorded material, no third-party samples).

Writes app/sounds/bank.h: thirteen sources as 8-bit mu-law, each at its own rate (16,000 Hz, or 11,025 Hz for the
low ones: the FM-1's flash holds them all), synthesized at 22,050 Hz then resampled; with --wav DIR, a WAV of each.
The last five evolve: an arpeggio through a progression, a bass line that changes, chords that move, a pad that
opens, a rhythmic sequence. Played slowly (VITESSE), each takes many seconds to go through.

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


# --- 9..13: the evolving ones ------------------------------------------------------------------------------------
def midi_hz(n):
    return 440.0 * 2 ** ((n - 69) / 12)


def saw(ph):
    return 2 * (ph - np.floor(ph + 0.5))


def svf_lp(x, fc, q=0.7):
    """a time-varying low-pass (fc an array, Hz): Chamberlin state-variable"""
    lo = bp = 0.0
    out = np.empty(len(x))
    damp = 1 / q
    for i in range(len(x)):
        f = 2 * math.sin(math.pi * min(fc[i], SR * 0.2) / SR)
        lo += f * bp
        hi = x[i] - lo - damp * bp
        bp += f * hi
        out[i] = lo
    return out


def arpege(sec=2.2):
    """a plucked arpeggio walking through Am - F - C - G, sixteenths, its filter opening as it goes"""
    n = int(sec * SR)
    out = np.zeros(n)
    chords = [(57, 60, 64, 69), (53, 57, 60, 65), (48, 52, 55, 60), (55, 59, 62, 67)]
    steps = 32
    st = n // steps
    pattern = (0, 1, 2, 3, 2, 1, 3, 2)
    for k in range(steps):
        ch = chords[k * 4 // steps]
        note = ch[pattern[k % 8]] + (12 if k % 16 >= 12 else 0)
        f = midi_hz(note)
        m = min(int(st * 1.8), n - k * st)
        tt = np.arange(m) / SR
        bright = 0.25 + 0.75 * k / steps
        ph = f * tt
        v = 0.6 * saw(ph) + 0.4 * np.sign(np.sin(2 * math.pi * ph * 1.002)) * bright
        v *= np.exp(-tt * (18 - 10 * bright))
        out[k * st:k * st + m] += v * (0.8 + 0.2 * (k % 4 == 0))
    fc = 600 + 4200 * np.linspace(0, 1, n) ** 1.5
    return fade(norm(svf_lp(out, fc, 1.6)), 0.005, 0.05)


def basse(sec=2.6):
    """a bass line that changes: A1 A1 C2 E2 | F1 F1 G1 B1, a sub, a saw that growls more each bar"""
    n = int(sec * SR)
    line = (33, 33, 45, 36, 40, 33, 29, 29, 41, 31, 35, 31)
    st = n // len(line)
    f = np.zeros(n)
    for k, nt in enumerate(line):
        f[k * st:(k + 1) * st] = midi_hz(nt)
    f[len(line) * st:] = midi_hz(line[-1])
    f = onepole_lp(f, 40)                                       # a little glide between notes
    ph = np.cumsum(f) / SR
    pos = np.arange(n) % st / SR
    env = np.exp(-pos * 7) * 0.7 + 0.3
    grow = np.linspace(0, 1, n)
    x = 0.8 * np.sin(2 * math.pi * ph) + (0.3 + 0.6 * grow) * saw(ph) + 0.25 * grow * saw(ph * 2.01)
    fc = 120 + (300 + 1400 * grow) * np.exp(-pos * 9)
    y = svf_lp(x * env, fc, 2.2)
    return fade(norm(np.tanh(1.8 * y)), 0.005, 0.06)


def accords(sec=2.8):
    """chords that move: Dm9 - Bbmaj7 - Fmaj7/A - C6sus, detuned saws crossfading, the voicing drifting up"""
    n = int(sec * SR)
    t = np.arange(n) / SR
    chords = [(50, 57, 60, 64, 65), (46, 53, 57, 60, 62), (45, 53, 57, 60, 64), (48, 55, 57, 62, 67)]
    seg = n / len(chords)
    out = np.zeros(n)
    for k, ch in enumerate(chords):
        c = (k + 0.5) * seg
        w = np.clip(1 - np.abs(np.arange(n) - c) / (seg * 0.75), 0, 1) ** 1.2   # each chord swells in, then out
        for j, nt in enumerate(ch):
            for d in (-0.07, 0.0, 0.08):                        # band-limited saws: their harmonics below 5 kHz
                f = midi_hz(nt) * 2 ** (d / 12)
                ph = RNG.random()
                for h in range(1, int(5000 / f) + 1):
                    out += w * np.sin(2 * math.pi * (h * f * t + h * ph)) * ((0.9 - 0.08 * j) / h)
    fc = 900 + 900 * np.sin(2 * math.pi * 0.35 * t) ** 2 + 1200 * t / sec
    y = svf_lp(out, fc, 0.9)
    trem = 1 + 0.1 * np.sin(2 * math.pi * 3.1 * t)
    return fade(norm(y * trem), 0.08, 0.15)


def nappe(sec=3.0):
    """a pad that opens: the partials of A1 rising one after another, each with its own slow beat"""
    t = t_axis(sec)
    f0 = midi_hz(33)
    out = np.zeros(len(t))
    for h in range(1, 24):
        onset = (h - 1) / 23 * sec * 0.8
        env = np.clip((t - onset) / 0.4, 0, 1) ** 2
        detune = 1 + 0.0015 * math.sin(h * 1.7)
        out += env * np.sin(2 * math.pi * f0 * h * detune * t + RNG.random() * 6) * (1 / h ** 0.7) * \
            (1 + 0.3 * np.sin(2 * math.pi * (0.2 + 0.07 * h) * t))
    return fade(norm(out), 0.3, 0.3)


def sequence(sec=1.6):
    """sixteen steps that never quite repeat: FM blips, a clap of noise, a pitched tom, a bit of glitch"""
    n = int(sec * SR)
    out = np.zeros(n)
    st = n // 16
    for k in range(16):
        m = min(st * 2, n - k * st)
        tt = np.arange(m) / SR
        kind = (k * 7 + k // 4) % 5
        if kind == 0:
            f = midi_hz(48 + (k * 5) % 24)
            v = np.sin(2 * math.pi * f * tt + (2 + k * 0.4) * np.sin(2 * math.pi * f * 3.5 * tt)) * np.exp(-tt * 30)
        elif kind == 1:
            v = RNG.normal(0, 1, m) * np.exp(-tt * 45)
            v = bandpass(v, 1800 + 200 * k, 3)
        elif kind == 2:
            f = 180 * np.exp(-tt * 12) + 60
            v = np.sin(2 * math.pi * np.cumsum(f) / SR) * np.exp(-tt * 14)
        elif kind == 3:
            hold = max(1, 3 + k)
            v = np.repeat(RNG.uniform(-1, 1, m // hold + 1), hold)[:m] * np.exp(-tt * 25)
        else:
            f = midi_hz(72 + (k * 3) % 12)
            v = np.sign(np.sin(2 * math.pi * f * tt)) * np.exp(-tt * 60) * 0.6
        out[k * st:k * st + m] += v * (1.0 if k % 4 == 0 else 0.7)
    return fade(norm(out), 0.002, 0.03)


SOURCES = [("VOIX", voix, 16000), ("VERRE", verre, 16000), ("CORDE", corde, 16000), ("SOUFFLE", souffle, 16000),
           ("TAMBOUR", tambour, 16000), ("ORGUE", orgue, 16000), ("PLUIE", pluie, 16000), ("SPECTRE", spectre, 16000),
           ("ARPEGE", arpege, 16000), ("BASSE", basse, 11025), ("ACCORDS", accords, 16000), ("NAPPE", nappe, 11025),
           ("SEQUENCE", sequence, 16000)]


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
             "#pragma once", "#include <stdint.h>", f"#define SND_COUNT {len(SOURCES)}u", ""]
    total = 0
    names = []
    for name, fn, rate in SOURCES:
        x = fn()
        if rate != SR:                                          # anti-aliased, then peak-normalized again
            g = math.gcd(rate, SR)
            x = norm(signal.resample_poly(x, rate // g, SR // g))
        q = mulaw_encode(x)
        total += len(q)
        names.append((name, len(q), rate))
        body = ",".join(str(int(v)) for v in q)
        lines.append(f"static const uint8_t SND_{name}[{len(q)}] = {{{body}}};")
        if a.wav:
            d = Path(a.wav)
            d.mkdir(parents=True, exist_ok=True)
            back = mulaw_table()[q]
            with wave.open(str(d / f"{name.lower()}.wav"), "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(rate)
                w.writeframes(struct.pack(f"<{len(back)}h", *np.round(back * 32767).astype(np.int16)))
        print(f"{name:8s} {len(q) / rate:5.2f} s  {rate:5d} Hz  {len(q):6d} B")
    tab = ",".join(f"{v:.7f}f" for v in mulaw_table())
    lines.append(f"static const float SND_ULAW[256] = {{{tab}}};")
    lines.append("static const struct { const char *name; const uint8_t *data; uint32_t len, rate; } "
                 "SND_BANK[SND_COUNT] = {")
    for name, n, rate in names:
        lines.append(f'    {{"{name}", SND_{name}, {n}u, {rate}u}},')
    lines.append("};")
    out = ROOT / "app" / "sounds" / "bank.h"
    out.write_text("\n".join(lines) + "\n")
    print(f"total    {total} B -> {out}")


if __name__ == "__main__":
    main()
