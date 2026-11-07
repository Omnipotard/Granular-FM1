#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 GRAINE contributors
"""tools/graine_import.py against a simulated FM-1 (fm1-base's tests/backup_tool_test.py FakeFM1: no hardware, no
mido). The fake answers GRAINE's import command (0x47) as app.c / store.c do: it pulls the bytes in 512s, checks the
CRC-32 and reports. Checked: WAVs of each kind converted (rates, lengths, the silence cut, mu-law), a sound sent and
kept byte for byte, a slot cleared, a byte changed on the way (refused), another firmware (not GRAINE: not sent).
    python3 tests/import_tool_test.py"""
import io
import math
import struct
import sys
import tempfile
import wave
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "base" / "tools"))
sys.path.insert(0, str(ROOT / "base" / "tests"))
import fm1_backup as B  # noqa: E402
import fm1_install as I  # noqa: E402
import graine_import as G  # noqa: E402
from backup_tool_test import FakeFM1, fast  # noqa: E402


class FakeGraine(FakeFM1):
    def __init__(self, identity="FM-1_4700", corrupt=False):
        super().__init__(b"GRNE" + bytes(12), identity=identity, port="GRAINE FM-1")
        self.slots, self.corrupt = {}, corrupt

    def importer(self, slot, rate, n, crc, name):
        got = bytearray()
        for k in range(0, n, 512):
            got += (yield (k, min(512, n - k)))
        if self.corrupt and got:
            got[len(got) // 2] ^= 1
        if slot > 3 or (n and (n < 64 or n > G.SLOT_BYTES or not 8000 <= rate <= 48000)):
            yield (B.FAIL_ADDR | 1, 8)
            return
        if zlib.crc32(bytes(got)) != crc:
            self.slots.pop(slot, None)
            yield (B.FAIL_ADDR | 22, 8)
            return
        if n:
            self.slots[slot] = (rate, bytes(got), name.rstrip(b"\0").decode())
        else:
            self.slots.pop(slot, None)
        yield (B.OK_ADDR, 8)

    def rx(self, link, d):
        f = B.parse_frame(d)
        if f and f[0] == G.IMP_CMD and len(f[1]) == 23 and self.identity.startswith("FM-1_47"):
            slot, rate, n, crc = struct.unpack_from("<BIII", f[1])
            self.pull = self.importer(slot, rate, n, crc, f[1][13:23])
            self.next(link, None)
            return
        super().rx(link, d)


def wav_bytes(path, rate, sec, ch=1, width=2, lead=0.0):
    n = int(rate * sec)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(ch)
        w.setsampwidth(width)
        w.setframerate(rate)
        frames = bytearray()
        for i in range(n):
            v = 0.0 if i < lead * rate else 0.5 * math.sin(2 * math.pi * 220 * i / rate)
            for _ in range(ch):
                if width == 2:
                    frames += struct.pack("<h", int(v * 32767))
                elif width == 3:
                    frames += int(v * 8388607).to_bytes(3, "little", signed=True)
                else:
                    frames += bytes([int(128 + v * 127)])
        w.writeframes(bytes(frames))


def main():
    failed = 0

    def ok(cond, what):
        nonlocal failed
        print(f"{what:<66} {'ok' if cond else 'FAIL'}")
        failed += not cond

    fast()
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        wav_bytes(td / "short.wav", 44100, 1.0, ch=2, lead=0.25)
        mono, r = G.read_wav(td / "short.wav")
        q, rate = G.convert(mono, r)
        ok(rate == 22050 and abs(len(q) - 16537) < 30, "1 s stereo 44.1 kHz: 22,050 Hz, the silence before cut")
        ok(max(q) > 240 and min(q) < 15 and abs(G.ulaw(0.0) - 128) <= 1, "normalized mu-law, 128 the middle")
        wav_bytes(td / "long.wav", 48000, 5.0, width=3)
        q, rate = G.convert(*G.read_wav(td / "long.wav"))
        ok(rate == 11025 and len(q) <= G.SLOT_BYTES, "5 s 24-bit: 11,025 Hz, it fits a slot")
        wav_bytes(td / "vlong.wav", 8000, 12.0, width=1)
        q, rate = G.convert(*G.read_wav(td / "vlong.wav"))
        ok(rate == 8000 and len(q) == G.SLOT_BYTES, "12 s 8-bit: 8,000 Hz, the end cut")
        q, rate = G.convert(*G.read_wav(td / "short.wav"), rate="16000")
        ok(rate == 16000, "--rate 16000")
        (td / "x.txt").write_text("hello")
        try:
            G.read_wav(td / "x.txt")
            ok(False, "not a WAV: refused")
        except G.SoundError:
            ok(True, "not a WAV: refused")
        ok(G.name_of("ma voix (1).wav") == b"MA VOIX 1".ljust(10, b"\0"), "the name: 9 capitals")

        dev = FakeGraine()
        out = io.StringIO()
        payload, rate = G.convert(*G.read_wav(td / "short.wav"))
        code = G.send(2, payload, rate, G.name_of("short"), backend=dev, out=out)
        ok(code == 0 and dev.slots.get(1) == (22050, payload, "SHORT"), "sent into U2, kept byte for byte")
        code = G.send(2, b"", 22050, G.name_of(""), backend=dev, out=out)
        ok(code == 0 and 1 not in dev.slots, "--clear empties U2")
        bad = FakeGraine(corrupt=True)
        code = G.send(1, payload, rate, G.name_of("x"), backend=bad, out=out)
        ok(code == 22 and not bad.slots, "a byte changed on the way: refused, nothing kept")
        other = FakeGraine(identity="FM-1_1100")
        try:
            G.send(1, payload, rate, G.name_of("x"), backend=other, out=out)
            ok(False, "another firmware: not sent")
        except I.InstallError as e:
            ok(e.code == "model" and not other.slots, "another firmware: not sent")
        rc = G.main([str(td / "short.wav"), "--out", str(td / "u.bin")], out=out)
        ok(rc == 0 and (td / "u.bin").read_bytes() == payload, "--out: only converted")
    print("FAILED" if failed else "all ok")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
