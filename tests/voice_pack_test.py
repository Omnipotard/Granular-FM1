#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 GRAINE contributors
"""The voice pack (app/sounds/voix, tools/gen_voices.py) as the importer takes it: each WAV listed, read by
tools/graine_import.py, kept whole (nothing cut) at the rate written in index.json, a name the FM-1 shows."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import graine_import as gi                      # noqa: E402

VOIX = ROOT / "app" / "sounds" / "voix"
fails = 0


def check(what, ok):
    global fails
    print(f"{what}: {'ok' if ok else 'FAIL'}")
    fails += not ok


index = json.loads((VOIX / "index.json").read_text())
check("the pack lists its sounds", len(index) >= 4)
for v in index:
    mono, rate_in = gi.read_wav(str(VOIX / v["file"]))
    body, rate = gi.convert(mono, rate_in)
    kept = len(body) / rate
    check(f"{v['name']}: the rate the importer picks is the pack's ({rate} Hz)", rate == v["rate"] == rate_in)
    check(f"{v['name']}: whole in a slot ({len(body)} of {gi.SLOT_BYTES} bytes, {kept:.2f} of {v['seconds']} s)",
          len(body) <= gi.SLOT_BYTES and kept >= v["seconds"] - 0.05)
    check(f"{v['name']}: a name of 9 characters or fewer, in ASCII", len(v["name"]) <= 9 and v["name"].isascii())
print("all ok" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
