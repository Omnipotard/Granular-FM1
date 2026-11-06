#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 GRAINE contributors
"""GRAINE's emulator page, from fm1-base's browser build (./build.sh --web, which runs this after it):

  app/build/emu/site/            index.html, worklet.js, app.wasm: to host (GitHub Pages, any static host)
  app/build/emu/graine-emulateur.html   the same in one file (the wasm and the worklet inlined): open it from disk

The worklet is fm1-base's (web/emu/worklet.js) with the emulator's parts it was built with (resample, data, midi)
and GRAINE's own part: a sound dropped on the page written into a user slot of the data area."""
import base64
import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EMU = ROOT / "app" / "build" / "emu"
PAGE = ROOT / "app" / "web" / "emu" / "graine.html"

GRAINE_PART = r"""
// GRAINE's part: a sound from the page into user slot m.slot (U1..U4) of the data area, then the source
fm1Part({
  name: "graine",
  message(fm1, m) {
    const ex = fm1.ex;
    if (!ex.web_data || m.slot === undefined) return;
    const area = new Uint8Array(ex.memory.buffer, ex.web_data(), ex.web_data_len());
    const off = 0x97000 + m.slot * 0x10000 - ex.web_data_base();
    if (off < 0 || off + 0x10000 > area.length) return;
    area.fill(0xff, off, off + 0x10000);
    area.set(new Uint8Array(m.body), off + 32);
    area.set(new Uint8Array(m.head), off);      // (the header last, as the firmware writes it)
    if (ex.graine_rescan) ex.graine_rescan();
    if (ex.graine_source) ex.graine_source(9 + m.slot);
    fm1.part("graine", { ok: m.slot });
  },
});
"""


def strip_modules(src):
    return "\n".join(l for l in src.splitlines() if not re.match(r"\s*(import|export)\s", l))


def main():
    parts = [EMU / "parts" / f for f in ("parts_worklet.js", "resample_worklet.js", "data_worklet.js",
                                         "midi_worklet.js")]
    worklet = "\n".join(strip_modules(p.read_text()) for p in parts if p.exists())
    worklet += GRAINE_PART + "\n" + strip_modules((EMU / "worklet.js").read_text())
    page = PAGE.read_text()
    site = EMU / "site"
    site.mkdir(parents=True, exist_ok=True)
    (site / "index.html").write_text(page.replace("<!--@@INLINE@@-->", ""))
    (site / "worklet.js").write_text(worklet)
    shutil.copy(EMU / "app.wasm", site / "app.wasm")
    wasm = base64.b64encode((EMU / "app.wasm").read_bytes()).decode()
    inline = ("<script>window.GRAINE_WORKLET = " + json.dumps(worklet).replace("</", "<\\/") + ";\n"
              "window.GRAINE_WASM = \"" + wasm + "\";</script>")
    one = EMU / "graine-emulateur.html"
    one.write_text(page.replace("<!--@@INLINE@@-->", inline))
    print(f"emu page {site}/ (index.html, worklet.js, app.wasm) and {one} ({one.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
