#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 GRAINE contributors
"""GRAINE's site: fm1-base's (tools/make_site.py: the front page app/site/landing.md, the web installer, the manual
app/docs/MANUAL.md, the emulator, the firmware to download) and GRAINE's own sound importer (app/web/importer).

    python3 tools/site.py VERSION [--package FILE] [--publish]

Without --package it makes the release build itself (./build.sh --release VERSION: the JieLi toolchain and the AC79
SDK, JIELI_TOOLCHAIN and AC79_SDK); with it, the package given (the attested one from a release). The emulator is
built with Emscripten, or clang through tools/emcc-wasi. The site lands in app/build/site; --publish pushes it to the
repository's gh-pages branch (replacing what was there), which GitHub Pages serves once it is set to that branch
(the repository's Settings, Pages: Deploy from a branch, gh-pages, / (root))."""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP, BASE = ROOT / "app", ROOT / "base"
sys.path.insert(0, str(BASE / "tools"))


def run(*cmd, env=None):
    print("+", " ".join(str(c) for c in cmd))
    subprocess.run([str(c) for c in cmd], check=True, env=env, cwd=ROOT)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("version", help="X.Y (the release's)")
    ap.add_argument("--package", help="a release package made elsewhere")
    ap.add_argument("--publish", action="store_true", help="push the site to the gh-pages branch")
    a = ap.parse_args(argv)
    import make_site                                # noqa: E402 (base/tools)
    from build import read_conf                     # noqa: E402
    conf = read_conf(APP / "fm1.toml")
    stem = conf["name"].lower()
    if a.package:
        pkg = Path(a.package).resolve()
    else:
        run(sys.executable, BASE / "tools" / "build.py", APP, "--release", a.version)
        pkg = APP / "build" / f"{stem}-{a.version}.fwsc"
    env = dict(os.environ)
    if not shutil.which("emcc"):
        env["PATH"] = f"{ROOT / 'tools' / 'emcc-wasi'}{os.pathsep}{env['PATH']}"
    run(sys.executable, BASE / "tools" / "build.py", APP, "--web", "--release", a.version, env=env)
    out = APP / "build" / "site"
    make_site.main(APP, pkg, a.version, out)
    # GRAINE's own page: the sound importer
    html = (APP / "web" / "importer" / "index.html").read_text(encoding="utf-8")
    html = make_site.fill(html, {"NAME": conf["name"], "ID": conf.get("id_prefix", ""), "PORTS_JS": make_site.ports_js()},
                          "app/web/importer")
    (out / "importer").mkdir(exist_ok=True)
    (out / "importer" / "index.html").write_text(html, encoding="utf-8")
    print(f"site: {out}/importer/index.html")
    if a.publish:
        remote = subprocess.run(["git", "-C", str(ROOT), "remote", "get-url", "origin"], check=True,
                                capture_output=True, text=True).stdout.strip()
        sha = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--short", "HEAD"], check=True,
                             capture_output=True, text=True).stdout.strip()
        shutil.rmtree(out / ".git", ignore_errors=True)
        git = ["git", "-C", str(out)]
        subprocess.run(git + ["init", "-q", "-b", "gh-pages"], check=True)
        subprocess.run(git + ["add", "-A"], check=True)
        subprocess.run(git + ["commit", "-q", "-m", f"Site for {conf['name']} {a.version} (from {sha})"], check=True)
        subprocess.run(git + ["push", "-q", "-f", remote, "gh-pages"], check=True)
        shutil.rmtree(out / ".git", ignore_errors=True)
        print(f"pushed the site for {conf['name']} {a.version} to gh-pages")
    return 0


if __name__ == "__main__":
    sys.exit(main())
