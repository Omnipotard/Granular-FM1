// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Henri David
// The worklet's side of the emulator's optional parts (fm1.toml emu = [...], tools/build.py --web): the build's
// app_worklet.js loads the app's own app_worklet.js (as app_own_worklet.js), this, then each part's NAME_worklet.js,
// which calls
//   fm1Part({ name, load(fm1, data), message(fm1, m), process(fm1) })
//   load      after the wasm is instantiated, before the device boots; data: what the page's side put in the load
//             message (msg.parts[name], parts.js beforeLoad)
//   message   the page side's messages ({ type: "app", part: name, ... })
//   process   before each render quantum (after the app's own)
// fm1 is the worklet's processor (worklet.js: ex, the wasm's exports; mem; port); fm1.part(name, m, transfer) sends
// a message to the part's page side. The app's own fm1App (load, message, process, env) keeps working beside them.

const own = globalThis.fm1App || {};
const parts = [];
globalThis.fm1Part = (p) => { parts.push(p); };

// the load message's part data, which fm1App.load is not given: kept by the processor as it comes
const register = globalThis.registerProcessor;
globalThis.registerProcessor = (name, Proc) => register(name, name !== "fm1" ? Proc : class extends Proc {
  onMessage(m) {
    if (m && m.type === "load") this.partData = m.parts || {};
    return super.onMessage(m);
  }
});

globalThis.fm1App = {
  env: own.env,                          // (the app's own imports for app.wasm, worklet.js: kept as the app gave them)
  load(fm1) {
    fm1.part = (name, m, transfer) => fm1.port.postMessage({ ...m, type: "app", part: name }, transfer || []);
    for (const p of parts) if (p.load) p.load(fm1, (fm1.partData || {})[p.name]);
    if (own.load) own.load(fm1);
  },
  message(fm1, m) {
    if (m.part) {
      for (const p of parts) if (p.name === m.part && p.message) p.message(fm1, m);
    } else if (own.message) {
      own.message(fm1, m);
    }
  },
  process(fm1) {
    if (own.process) own.process(fm1);
    for (const p of parts) if (p.process) p.process(fm1);
  },
};
// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Henri David
// Emulator part "resample" (on in every build whose page takes parts; fm1.toml emu = ["-resample"] leaves it out,
// tools/build.py EMU_DEFAULT): the FM-1 makes 44.1 kHz; the page asks the browser for an
// AudioContext at 44,100 Hz, and a browser that runs it at another rate (48 kHz, 96 kHz, 22.05 kHz...) would play
// the device's samples one for one: off pitch and off tempo. With this part the worklet runs the device at its own
// rate and resamples (a 4-point cubic Hermite curve through the device's samples) to the context's, so the device's
// clock keeps time with the page's. At 44.1 kHz it does nothing.

const DEVICE_RATE = 44100;
const KEEP = 8192;                       // device frames held: a quantum's worth at any rate down to 3 kHz

function hermite(a, b, c, d, f) {        // the curve through b (f = 0) and c (f = 1), its slopes from a and d
  const c1 = 0.5 * (c - a), c2 = a - 2.5 * b + 2 * c - 0.5 * d, c3 = 0.5 * (d - a) + 1.5 * (b - c);
  return ((c3 * f + c2) * f + c1) * f + b;
}

fm1Part({
  name: "resample",
  load(fm1) {
    if (sampleRate === DEVICE_RATE) return;
    const ex = fm1.ex, step = DEVICE_RATE / sampleRate;
    const src = [new Float32Array(KEEP), new Float32Array(KEEP)];
    let len = 1, at = 1;                 // frames held (a silent one before the first); the read position among them
    const render = (n) => {
      const need = Math.floor(at + n * step) + 3;
      while (len < need) {
        const k = Math.min(1024, need - len);
        ex.web_render(k);
        src[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), k), len);
        src[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), k), len);
        len += k;
      }
      const outs = [new Float32Array(ex.memory.buffer, ex.web_out_l(), n), new Float32Array(ex.memory.buffer, ex.web_out_r(), n)];
      for (let i = 0; i < n; i++, at += step) {
        const j = Math.floor(at), f = at - j;
        for (let ch = 0; ch < 2; ch++) {
          const s = src[ch];
          outs[ch][i] = hermite(s[j - 1], s[j], s[j + 1], s[j + 2], f);
        }
      }
      const drop = Math.floor(at) - 1;   // keep one frame before the read position
      for (const s of src) s.copyWithin(0, drop, len);
      len -= drop;
      at -= drop;
    };
    fm1.ex = { ...ex, web_render: (n) => render(Math.min(n >>> 0, 1024)) };
  },
});
// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Henri David
// Emulator part "data" (fm1.toml emu = ["data"]), the worklet's side: the data area (plat_data and its windows,
// web_data: what an app keeps beyond its saved objects, as on the FM-1's flash) put back before the device boots
// from what the page kept, and sent to the page when it changed, looked at every 2 s of the page's time and when the
// page asks ({ flush: 1 }); { dump: 1 } sends it as it is, changed or not ({ dump: 1 } back). The page's side is
// data.js.

const EVERY_S = 2;
let snap = null, lastLook = 0;

function area(ex) {
  return new Uint8Array(ex.memory.buffer, ex.web_data(), ex.web_data_len());
}
function changed(now) {                  // (a word at a time where it can)
  if (now.byteOffset & 3 || now.length & 3) return now.some((v, i) => v !== snap[i]);
  const a = new Uint32Array(now.buffer, now.byteOffset, now.length >> 2), b = new Uint32Array(snap.buffer);
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return true;
  return false;
}
function send(fm1, always, extra = {}) {
  const now = area(fm1.ex);
  if (!always && !changed(now)) return;
  snap.set(now);
  const copy = now.slice();
  fm1.part("data", { ...extra, data: copy.buffer, base: fm1.ex.web_data_base() }, [copy.buffer]);
}

fm1Part({
  name: "data",
  load(fm1, kept) {
    if (!fm1.ex.web_data) return;        // (built without FM1B_EMU_DATA)
    const now = area(fm1.ex);
    if (kept instanceof ArrayBuffer && kept.byteLength === now.length) now.set(new Uint8Array(kept));
    snap = now.slice();
  },
  message(fm1, m) {
    if (snap && (m.flush || m.dump)) send(fm1, !!m.dump, m.dump ? { dump: 1 } : {});
  },
  process(fm1) {
    if (!snap || currentTime - lastLook < EVERY_S) return;
    lastLook = currentTime;
    send(fm1, false);
  },
});
// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Henri David
// Emulator part "midi" (fm1.toml emu = ["midi"]), the worklet's side: MIDI bytes from the page (whole messages, as
// Web MIDI gives them; running status too) into the device as its USB port takes them: channel and system messages
// as USB-MIDI packets (web_midi_in, PORT_USB), a SysEx frame whole to the app (web_sx_in, an app with APP_SYSEX;
// dropped otherwise, as a device without a SysEx handler drops it). Out, before each render quantum: what the app
// sent (web_out_pkts, as USB-MIDI packets; web_sx_out_*, its SysEx frames) as whole messages to the page,
// { msgs: [Uint8Array, ...] }. The page's side is midi.js.

const PORT_USB = 0;
const dataLen = (s) => (s >= 0xf8 || s === 0xf6 ? 0 : s === 0xf1 || s === 0xf3 || (s & 0xe0) === 0xc0 ? 1 : 2);
const CIN_BYTES = [0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1];   // a USB-MIDI packet's bytes by its code index

function packet(m) {
  const s = m[0];
  const cin = s < 0xf0 ? s >> 4 : s >= 0xf8 ? 0xf : s === 0xf2 ? 3 : s === 0xf6 ? 5 : 2;
  return (cin | s << 8 | (m[1] || 0) << 16 | (m[2] || 0) << 24) >>> 0;
}

// the bytes in: SysEx collected, channel messages with running status, real time anywhere
let sx = null, status = 0, cur = [];
function feed(fm1, bytes) {
  const ex = fm1.ex;
  for (const b of bytes) {
    if (b >= 0xf8) { ex.web_midi_in(PORT_USB, packet([b])); continue; }
    if (b === 0xf0) { sx = [b]; continue; }
    if (sx) {
      if (b < 0x80 || b === 0xf7) {
        sx.push(b);
        if (b === 0xf7) { sysexIn(ex, sx); sx = null; }
        continue;
      }
      sx = null;                            // (a status byte inside: the frame is lost, the status taken)
    }
    if (b & 0x80) {
      if (b === 0xf7) continue;
      status = b < 0xf0 ? b : 0;
      cur = [b];
    } else if (cur.length) {
      cur.push(b);
    } else if (status) {
      cur = [status, b];
    } else {
      continue;
    }
    if (cur.length === 1 + dataLen(cur[0])) { ex.web_midi_in(PORT_USB, packet(cur)); cur = []; }
  }
}
function sysexIn(ex, frame) {
  if (!ex.web_sx_in || frame.length > ex.web_sx_in_max()) return;
  new Uint8Array(ex.memory.buffer, ex.web_sx_in_buf(), frame.length).set(frame);
  ex.web_sx_in(frame.length);
}

// the bytes out: packets (a SysEx in packets too, CIN 4..7) and the app's frames, as whole messages
let osx = null;
function drain(fm1) {
  const ex = fm1.ex, msgs = [];
  const n = ex.web_out_n();
  if (n) {
    const pk = new Uint32Array(ex.memory.buffer, ex.web_out_pkts(), n);
    for (const p of pk) {
      const cin = p & 0xf, bytes = [p >> 8 & 0xff, p >> 16 & 0xff, p >>> 24].slice(0, CIN_BYTES[cin]);
      if (cin >= 4 && cin <= 7) {
        for (const b of bytes) {
          if (b === 0xf0) osx = [];
          if (osx) osx.push(b);
          if (b === 0xf7 && osx) { msgs.push(Uint8Array.from(osx)); osx = null; }
        }
      } else if (bytes.length) {
        msgs.push(Uint8Array.from(bytes));
      }
    }
    ex.web_out_clear();
  }
  if (ex.web_sx_out_len && ex.web_sx_out_len()) {
    const all = new Uint8Array(ex.memory.buffer, ex.web_sx_out_buf(), ex.web_sx_out_len());
    for (let at = 0; at < all.length;) {
      let k = at + 1;
      while (k < all.length && all[k] !== 0xf7 && all[k] !== 0xf0) k++;
      const end = k < all.length && all[k] === 0xf7 ? k + 1 : k;   // (a frame cut short: up to the next F0)
      if (all[end - 1] === 0xf7) msgs.push(all.slice(at, end));
      at = end;
    }
    ex.web_sx_out_clear();
  }
  if (msgs.length) fm1.part("midi", { msgs }, msgs.map((m) => m.buffer));
}

fm1Part({
  name: "midi",
  message(fm1, m) { if (m.data) feed(fm1, m.data); },
  process(fm1) { drain(fm1); },
});
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
    if (ex.graine_user) ex.graine_user(m.slot);          // (the user slot's source: app.c knows its number)
    fm1.part("graine", { ok: m.slot });
  },
});

// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Charles Vestal
// An fm1-base app in an AudioWorklet: app.wasm is the whole FM-1 app (web/emu/emu_web.c). Each render
// quantum asks it for 128 frames, which runs the device's clock forward; between quanta the page's
// input goes in and, about 30 times a second, the screen, the lights, any saved objects and what the
// firmware said to a screen reader (fm1-reader's SysEx frames) go out.
// An app can add its own part (APP_DIR/web/emu/app_worklet.js, loaded first): globalThis.fm1App with any of
// load(fm1), message(fm1, m) for the page's { type: "app" } messages, process(fm1) before each quantum, and env:
// functions app.wasm imports (its "env" imports; any other is a stub that returns 0).

/** the dim controls' opacity for the dim layer's pulse of ns (APP_DIM_LEVELS, plat_dim_ns): 0.25 at 500 ns up to 1 at
 *  4,000 (a plain linear step: the page's view of the levels, not a measure of the light) */
function dimOpacity(ns) {
  const t = Math.min(4000, Math.max(500, ns));
  return +(0.25 + 0.75 * (t - 500) / 3500).toFixed(3);
}

const clock = globalThis.performance ? () => globalThis.performance.now() : () => Date.now();

class Fm1 extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.input = null;
    this.sentBlits = -1;
    this.sentWrites = -1;
    this.lastFrame = 0;
    this.busy = 0;
    this.frames = 0;
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  async onMessage(m) {
    if (m.type === "load") {
      const now = () => BigInt(Math.round(currentTime * 1e9));
      const wasi = {
        clock_time_get: (id, prec, out) => {
          new DataView(this.mem.buffer).setBigUint64(out, now(), true);
          return 0;
        },
        fd_write: (fd, iov, n, out) => { new DataView(this.mem.buffer).setUint32(out, 0, true); return 0; },
        proc_exit: () => {},
      };
      const stub = new Proxy(wasi, { get: (t, k) => (k in t ? t[k] : () => 0) });
      const appEnv = (globalThis.fm1App && globalThis.fm1App.env) || {};
      const env = new Proxy(appEnv, { get: (t, k) => (k in t ? t[k] : () => 0) });
      const { instance } = await WebAssembly.instantiate(m.wasm, { wasi_snapshot_preview1: stub, env });
      this.ex = instance.exports;
      this.mem = this.ex.memory;
      if (this.ex._initialize) this.ex._initialize();
      this.app = globalThis.fm1App || {};
      if (this.app.load) this.app.load(this);
      const max = this.ex.web_store_max();
      for (const [obj, bytes] of Object.entries(m.store || {})) {
        const b = new Uint8Array(bytes).subarray(0, max);
        new Uint8Array(this.mem.buffer, this.ex.web_store(+obj), b.length).set(b);
        this.ex.web_store_set_len(+obj, b.length);
      }
      this.sentWrites = this.ex.web_store_writes();
      this.ex.web_master(m.master ?? 2800);
      this.ex.web_boot();
      this.port.postMessage({ type: "ready" });
    } else if (!this.ex) {
      return;
    } else if (m.type === "buttons") {
      this.ex.web_buttons(m.mask >>> 0);
    } else if (m.type === "keys") {
      this.ex.web_keys(m.mask >>> 0);
    } else if (m.type === "enc") {
      this.ex.web_enc(m.role, m.n | 0);
    } else if (m.type === "master") {
      this.ex.web_master(m.value | 0);
    } else if (m.type === "sysex") {                  // a screen reader's frame: the bytes between F0 and F7
      const b = Uint8Array.from(m.bytes).subarray(0, 64);
      new Uint8Array(this.mem.buffer, this.ex.web_sysex_buf(), b.length).set(b);
      this.ex.web_sysex(b.length);
    } else if (m.type === "app" && this.app.message) {
      this.app.message(this, m);
    }
  }

  publish() {
    const ex = this.ex;
    const blits = ex.web_blits();
    const msg = { type: "frame", buttons: ex.web_lit_buttons() >>> 0, keys: ex.web_lit_keys() >>> 0,
                  dimButtons: ex.web_dim_buttons() >>> 0, dimKeys: ex.web_dim_keys() >>> 0 };
    if (ex.web_dim_ns) msg.dimOp = dimOpacity(ex.web_dim_ns() >>> 0);   // only an app with APP_DIM_LEVELS
    if (this.frames >= 44100) {                       // the share of real time spent running the device
      msg.load = Math.round(100 * this.busy / (this.frames / 44.1));
      this.busy = this.frames = 0;
    }
    const transfer = [];
    const said = ex.web_said_len();
    if (said) {                                       // whole F0..F7 frames
      msg.said = new Uint8Array(this.mem.buffer, ex.web_said_buf(), said).slice();
      ex.web_said_clear();
      transfer.push(msg.said.buffer);
    }
    if (blits !== this.sentBlits) {
      this.sentBlits = blits;
      msg.fb = new Uint16Array(this.mem.buffer, ex.web_fb(), 240 * 240).slice();
      transfer.push(msg.fb.buffer);
    }
    const writes = ex.web_store_writes();
    if (writes !== this.sentWrites) {
      this.sentWrites = writes;
      msg.store = {};
      for (let o = 0; o < ex.web_nobj(); o++) {
        const n = ex.web_store_len(o);
        if (n >= 0) msg.store[o] = new Uint8Array(this.mem.buffer, ex.web_store(o), n).slice();
      }
    }
    this.port.postMessage(msg, transfer);
  }

  process(inputs, outputs) {
    if (!this.ex) return true;
    const out = outputs[0];
    const n = out[0].length;
    const t0 = clock();
    if (this.app.process) this.app.process(this);
    this.ex.web_render(n);
    this.busy += clock() - t0;
    this.frames += n;
    out[0].set(new Float32Array(this.mem.buffer, this.ex.web_out_l(), n));
    if (out[1]) out[1].set(new Float32Array(this.mem.buffer, this.ex.web_out_r(), n));
    if (currentTime - this.lastFrame >= 1 / 30) {
      this.lastFrame = currentTime;
      this.publish();
    }
    return true;
  }
}

registerProcessor("fm1", Fm1);