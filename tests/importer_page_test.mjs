// SPDX-License-Identifier: MIT
// Copyright (C) 2026 GRAINE contributors
// The site's sound importer (app/web/importer/index.html, the part between its "import protocol" markers), filled in
// as tools/site.py fills it, against a simulated FM-1 that answers as app/app.c app_usb_cmd and store.c slot_import:
// the command 0x47 (slot, rate, length, CRC-32, name), the bytes pulled in 512s, the CRC checked, the report. Checked:
// a sound kept byte for byte with its rate and name, a slot cleared, a byte changed on the way (refused), another
// firmware on the FM-1 (not sent), an FM-1 that stops asking; the mu-law and the rate chosen as tools/graine_import.py.
//   node tests/importer_page_test.mjs
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const root = path.join(path.dirname(fileURLToPath(import.meta.url)), "..");
const PORTS_JS = fs.readFileSync(path.join(root, "base/web/fm1ports.js"), "utf8").replace(/^export\s+/gm, "").replace(/^import .*?;\n/gm, "");
let html = fs.readFileSync(path.join(root, "app/web/importer/index.html"), "utf8");
html = html.replace(/\{\{([A-Z_]+)\}\}/g, (m, k) => ({ NAME: "GRAINE", ID: "FM-1_47", PORTS_JS })[k] ?? m);
const left = html.match(/\{\{[A-Z_]+\}\}/g);
const m = /\/\/ --- import protocol: begin[^\n]*\n([\s\S]*?)\/\/ --- import protocol: end/.exec(html);
if (!m || left) { console.log(`FAIL importer page: ${left ? left.join(" ") : "no protocol markers"}`); process.exit(1); }
const FW = { name: "GRAINE", id: "FM-1_47" };
const GI = new Function("FW", `"use strict";\n${m[1]}\nreturn GI;`)(FW);
Object.assign(GI.DELAY, { poll: 5, idle: 300, first: 300, hs: 100, hsFull: 100 });

let fails = 0;
const check = (ok, msg) => { console.log(`${ok ? "ok  " : "FAIL"} ${msg}`); if (!ok) fails++; };
const u32 = (b, o) => (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0;
const le32 = (v) => [v & 0xFF, (v >>> 8) & 0xFF, (v >>> 16) & 0xFF, (v >>> 24) & 0xFF];

class FakeGraine {
  constructor(o = {}) { Object.assign(this, { identity: "FM-1_4700", corrupt: false, stallAfter: null }, o); this.q = []; this.slots = {}; this.lost = false; this.reads = 0; }
  send(pkt) { this.rx(pkt); return true; }
  drain() { this.q.length = 0; }
  async read(ms) { if (!this.q.length) await new Promise((r) => setTimeout(r, Math.min(ms, 2))); return this.q.shift() || null; }
  close() {}
  *importer(slot, rate, n, c, name) {
    const got = [];
    for (let k = 0; k < n; k += 512) got.push(...(yield [k, Math.min(512, n - k)]));
    if (this.corrupt && got.length) got[got.length >> 1] ^= 1;
    if (slot > 3 || (n && (n < 64 || n > GI.SLOT_BYTES || rate < 8000 || rate > 48000))) { yield [(GI.FAIL_ADDR | 1) >>> 0, 8]; return; }
    const g = Uint8Array.from(got);
    if (GI.crc32(g) !== c) { delete this.slots[slot]; yield [(GI.FAIL_ADDR | 22) >>> 0, 8]; return; }
    if (n) this.slots[slot] = { rate, bytes: g, name: String.fromCharCode(...name.filter((b) => b)) };
    else delete this.slots[slot];
    yield [GI.OK_ADDR, 8];
  }
  ask(addr, n) {
    const q = [0, ...le32(addr >>> 0), n & 255, n >> 8, 0];
    this.q.push([0xF0, ...GI.pack7([0, 0x59, 0x30, 8, 0, 0, ...q, ~q.reduce((a, v) => a + v, 0) & 255]), 0xF7]);
    this.wanted = [addr >>> 0, n];
  }
  next(data) { const r = this.pull.next(data); if (r.done) { this.pull = null; return; } this.ask(...r.value); }
  rx(d) {
    const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
    if (same(d, [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7]) || same(d, [0xF0, 0x00, 0x32, 0x45, 0x08, 0x00, 0x00, 0x40, 0x00, 0x7E, 0x01, 0xF7])) {
      const t = [...Buffer.from(this.identity), ...new Array(27 - this.identity.length).fill(0)];
      this.q.push([0xF0, ...GI.pack7([0, 0x59, 0x11, 27, 0, 0, ...t, ~t.reduce((a, v) => a + v, 0) & 255]), 0xF7]);
      return;
    }
    const u = GI.unpack7(d.slice(1, -1));
    if (u.length >= 7 && u[2] === GI.IMP_CMD && (u[3] | u[4] << 8) === 23 && this.identity.startsWith("FM-1_47")) {
      const b = u.slice(6, 29);
      this.pull = this.importer(b[0], u32(b, 1), u32(b, 5), u32(b, 9), b.slice(13, 23));
      this.next(undefined);
      return;
    }
    if (this.pull && this.wanted && u.length >= 15 && u[2] === 0x30) {
      const [addr, n] = this.wanted;
      if (u32(u, 7) !== addr || (u[11] | u[12] << 8) !== n) return;
      if (this.stallAfter !== null && ++this.reads > this.stallAfter) return;
      this.next(Uint8Array.from(u.slice(14, 14 + n)));
    }
  }
}
const accessOf = () => ({ inputs: new Map([["i", { name: "GRAINE FM-1", state: "connected", open: async () => {} }]]),
                          outputs: new Map([["o", { name: "GRAINE FM-1", state: "connected", open: async () => {} }]]) });
const connect = (dev) => GI.find(accessOf(), () => dev);
const err = async (p) => { try { await p; return null; } catch (e) { return e; } };

const tone = new Float32Array(30000);
for (let i = 0; i < tone.length; i++) tone[i] = i < 3000 ? 0 : 0.4 * Math.sin(i / 7);
const bytes = GI.encode(tone);
check(bytes.length === 27000, "encode: the silence before cut");
check(Math.max(...bytes) > 240 && Math.min(...bytes) < 15 && GI.ulaw(0) === 128, "encode: normalized mu-law, 128 the middle");
check(GI.ulaw(0.5) === 239 && GI.ulaw(-0.5) === 16, "mu-law as tools/graine_import.py (and store.c ulaw_enc)");
check(GI.rateFor(2.0) === 22050 && GI.rateFor(3.5) === 16000 && GI.rateFor(5) === 11025 && GI.rateFor(20) === 8000 && GI.rateFor(1, "16000") === 16000, "the rate chosen to fit");
check(String.fromCharCode(...GI.nameBytes("Mé voix-1!").filter((b) => b)) === "ME VOIX-1", "the name: 9 capitals, accents gone");
{
  const dev = new FakeGraine();
  const code = await GI.send((await connect(dev)).link, 3, 16000, bytes, "nappe");
  const s = dev.slots[2];
  check(code === 0 && s && s.rate === 16000 && s.name === "NAPPE" && s.bytes.length === bytes.length && s.bytes.every((v, i) => v === bytes[i]), "sent into U3: kept byte for byte, its rate and name");
  const c2 = await GI.send((await connect(dev)).link, 3, 22050, new Uint8Array(0), "");
  check(c2 === 0 && !dev.slots[2], "cleared: U3 empty");
}
{
  const dev = new FakeGraine({ corrupt: true });
  const code = await GI.send((await connect(dev)).link, 1, 22050, bytes, "x");
  check(code === 22 && !dev.slots[0], "a byte changed on the way: refused, nothing kept");
}
{
  const e = await err(connect(new FakeGraine({ identity: "FM-1_1100" })));
  check(e && e.code === "model", "another firmware on the FM-1: not sent");
}
{
  const dev = new FakeGraine({ stallAfter: 3 });
  const e = await err(GI.send((await connect(dev)).link, 1, 22050, bytes, "x"));
  check(e && e.code === "stopped" && !dev.slots[0], "an FM-1 that stops asking: said so, nothing kept");
}
console.log(fails ? `${fails} FAILED` : "all ok");
process.exit(fails ? 1 : 0);
