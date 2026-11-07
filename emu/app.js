// made by tools/build.py: the emulator's parts (fm1.toml emu) and the app's own part of the page
import { compose } from "./parts/parts.js";
import * as data from "./parts/data.js";
import * as midi from "./parts/midi.js";
const own = {};
export const { attach, message } = compose({ data, midi }, own, { id: "graine", name: "GRAINE", backupExt: "bak" });
