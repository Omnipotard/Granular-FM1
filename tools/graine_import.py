#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 GRAINE contributors
"""Send a sound into one of GRAINE's four user slots (U1..U4) over USB, without the browser.

    python3 tools/graine_import.py SOUND.wav --slot 1 [--name NAME] [--rate auto|22050|...] [--port NAME]
    python3 tools/graine_import.py --clear --slot 2
    python3 tools/graine_import.py SOUND.wav --out u1.bin      (only convert: what would be sent, no FM-1)

Needs Python 3 with mido and python-rtmidi (pip install mido python-rtmidi). The sound: a WAV file (8, 16, 24 or
32-bit PCM, or 32-bit float; any rate; mono or stereo: the channels are mixed). GRAINE keeps 65,504 bytes a slot:
the silence before the sound is cut, then the sound is normalized and kept as mu-law at the rate chosen. With
--rate auto (the default) that is 22,050 Hz for up to 2.97 s, and a lower rate for longer sounds, down to 8,000 Hz
(8.2 s); past that the end is cut. The slot becomes the source playing, and stays there across power-offs.

The FM-1 takes the command (cmd 0x47, the update's SysEx framing), pulls the bytes, checks their CRC-32 and writes the
slot's header last: a transfer cut short leaves the slot empty, never half a sound.

Exit codes: 0 sent, 1 the FM-1 refused or failed, 2 bad arguments or file, 3 FM-1 not found, 4 connection lost."""
import argparse
import math
import struct
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "base" / "tools"))

IMP_CMD = 0x47
SLOT_BYTES = 65504                                 # 64 KiB a slot, less its 32-byte header (app/ui/store.c)
RATES = (22050, 16000, 11025, 8000)
ERRORS = {1: "the request (slot, length or rate)", 2: "the transfer (a read request unanswered)",
          7: "a flash erase", 8: "a flash write", 22: "the CRC-32 (a byte changed on the way)"}


class SoundError(Exception):
    pass


def read_wav(path):
    """-> (mono samples as floats -1..1, rate). PCM 8/16/24/32-bit and float 32-bit, read by hand (no numpy)"""
    b = Path(path).read_bytes()
    if b[:4] != b"RIFF" or b[8:12] != b"WAVE":
        raise SoundError("not a WAV file (RIFF/WAVE): convert it first, e.g. ffmpeg -i in.mp3 out.wav")
    pos, fmt, data = 12, None, None
    while pos + 8 <= len(b):
        cid, size = b[pos:pos + 4], struct.unpack_from("<I", b, pos + 4)[0]
        body = b[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", body)
            if fmt[0] == 0xFFFE and len(body) >= 26:          # WAVE_FORMAT_EXTENSIBLE: its subformat
                fmt = (struct.unpack_from("<H", body, 24)[0],) + fmt[1:]
        elif cid == b"data":
            data = body
        pos += 8 + size + (size & 1)
    if not fmt or data is None:
        raise SoundError("a WAV without its fmt or data part")
    tag, ch, rate, _, _, bits = fmt
    w = bits // 8
    if ch < 1 or w < 1 or rate < 1000:
        raise SoundError("a WAV this reads not (channels, bits or rate)")
    n = len(data) // (w * ch)
    if tag == 3 and bits == 32:
        vals = struct.unpack(f"<{n * ch}f", data[:n * ch * 4])
    elif tag == 1 and bits == 8:
        vals = [(v - 128) / 128.0 for v in data[:n * ch]]
    elif tag == 1 and bits == 16:
        vals = [v / 32768.0 for v in struct.unpack(f"<{n * ch}h", data[:n * ch * 2])]
    elif tag == 1 and bits == 24:
        vals = [int.from_bytes(data[i:i + 3], "little", signed=True) / 8388608.0 for i in range(0, n * ch * 3, 3)]
    elif tag == 1 and bits == 32:
        vals = [v / 2147483648.0 for v in struct.unpack(f"<{n * ch}i", data[:n * ch * 4])]
    else:
        raise SoundError(f"a WAV of format {tag}, {bits} bits: 8/16/24/32-bit PCM or 32-bit float only")
    mono = [sum(vals[i * ch:(i + 1) * ch]) / ch for i in range(n)]
    return mono, rate


def ulaw(x):
    """GRAINE's mu-law (ui/store.c ulaw_enc, the emulator's): 0..255, 128 the middle"""
    y = math.copysign(math.log1p(255.0 * min(1.0, abs(x))) / math.log(256.0), x)
    return max(0, min(255, round((y + 1.0) * 127.5)))


def convert(mono, rate_in, rate="auto"):
    """-> (mu-law bytes, rate): the silence before cut, normalized, resampled (linear, with a low-pass first when
    going down), faded in and out, at most SLOT_BYTES"""
    start = 0
    while start < len(mono) and abs(mono[start]) < 0.003:
        start += 1
    x = mono[start:]
    if len(x) < 64:
        raise SoundError("too short, or silent")
    dur = len(x) / rate_in
    if rate == "auto":
        rate = next((r for r in RATES if dur * r <= SLOT_BYTES), RATES[-1])
    rate = int(rate)
    if not 8000 <= rate <= 48000:
        raise SoundError("the rate: 8000 .. 48000 Hz")
    if rate < rate_in:                               # a one-pole low-pass twice, near the new Nyquist
        a = math.exp(-2 * math.pi * 0.45 * rate / rate_in)
        for _ in range(2):
            y, out = 0.0, []
            for v in x:
                y = (1 - a) * v + a * y
                out.append(y)
            x = out
    n = min(SLOT_BYTES, int(len(x) * rate / rate_in))
    step = rate_in / rate
    res = []
    for i in range(n):
        p = i * step
        k = int(p)
        f = p - k
        a0 = x[k] if k < len(x) else 0.0
        a1 = x[k + 1] if k + 1 < len(x) else a0
        res.append(a0 + (a1 - a0) * f)
    peak = max(abs(v) for v in res) or 1.0
    g = 0.95 / peak
    fade_in, fade_out = min(64, n // 4), min(256, n // 4)
    q = bytearray(n)
    for i, v in enumerate(res):
        e = min(1.0, i / fade_in if fade_in else 1.0, (n - i) / fade_out if fade_out else 1.0)
        q[i] = ulaw(v * g * e)
    return bytes(q), rate


def name_of(s):
    s = "".join(c for c in s.upper() if c.isascii() and (c.isalnum() or c in " _-"))
    return (s[:9] or "SON").ljust(10, "\0").encode("ascii")


def send(slot, payload, rate, name, port=None, backend=None, out=sys.stdout):
    import fm1_backup as B                          # noqa: E402 (base/tools)
    import fm1_install as I                         # noqa: E402
    if backend is None:
        try:
            backend = I.MidoBackend()
        except ImportError:
            raise I.InstallError("usage", "needs mido and python-rtmidi (pip install mido python-rtmidi)")
    dev = B.find(backend, port, B.Kind(identity="FM-1_47", name="GRAINE", port="GRAINE"))
    print(f"device: {dev.id.text}  ({dev.name})", file=out)
    try:
        body = struct.pack("<BIII", slot - 1, rate, len(payload), zlib.crc32(payload)) + name
        dev.link.drain()
        dev.link.send(B.frame(IMP_CMD, body))
        if not payload:                             # (clearing: nothing to pull, the FM-1 reports at once)
            payload = b"\0"
        code = B.serve(dev.link, payload, out)
        print(file=out)
        return code
    finally:
        dev.link.close()


def main(argv=None, out=sys.stdout):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sound", nargs="?", help="a WAV file")
    ap.add_argument("--slot", type=int, choices=(1, 2, 3, 4), default=1, help="U1..U4 (default 1)")
    ap.add_argument("--name", help="its name on the FM-1 (9 characters; default: the file's)")
    ap.add_argument("--rate", default="auto", help="auto (default), or 8000 .. 48000 Hz")
    ap.add_argument("--clear", action="store_true", help="empty the slot instead")
    ap.add_argument("--out", metavar="FILE", help="only write what would be sent (mu-law bytes) to FILE")
    ap.add_argument("--port", metavar="NAME", help="MIDI port to use (part of its name)")
    a = ap.parse_args(argv)
    try:
        if a.clear:
            payload, rate, name = b"", 22050, name_of("")
        else:
            if not a.sound:
                ap.error("a WAV file, or --clear")
            mono, rin = read_wav(a.sound)
            payload, rate = convert(mono, rin, a.rate)
            name = name_of(a.name or Path(a.sound).stem)
            print(f"{Path(a.sound).name}: {len(payload)} bytes at {rate} Hz ({len(payload) / rate:.2f} s) "
                  f"as {name.rstrip(bytes(1)).decode()}", file=out)
        if a.out:
            Path(a.out).write_bytes(payload)
            print(f"written: {a.out}", file=out)
            return 0
    except (OSError, SoundError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2
    import fm1_install as I                         # noqa: E402
    try:
        code = send(a.slot, payload, rate, name, a.port, out=out)
    except I.InstallError as e:
        print(f"error: {e}", file=sys.stderr)
        return I.EXIT.get(e.code, 1)
    except KeyboardInterrupt:
        print("\ninterrupted: the slot is left empty (send it again)", file=sys.stderr)
        return 130
    if code:
        print(f"error: not kept: {ERRORS.get(code, 'code %d' % code)}", file=sys.stderr)
        return 1
    print(f"done: U{a.slot} {'cleared' if a.clear else 'holds it, and plays'}", file=out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
