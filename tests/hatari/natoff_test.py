#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""One boot with NatFeats off (the regression gate's row 12; TEST-MATRIX.md,
Erez, 17 Nov 2026): EMAIL.INF with no NATFEATS=, as every user's, so EMail
never probes the emulator. It starts offline with an account, its window
is drawn, ^Q quits it, the desktop is back, and there is no CRASH.LOG.

    natoff_test.py WORKDIR [MACHINE]
"""
import os
import shutil
import sys
import time

from PIL import Image

import rig

work, machine, more = rig.cli("Falcon")
os.makedirs(work, exist_ok=True)
hd = os.path.join(work, "hd")
shutil.rmtree(hd, ignore_errors=True)
os.makedirs(hd)
rig.install_prg(hd)
with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
    f.write("[options]\r\ntz=180\r\nstartoffline=1\r\n[account]\r\nname=Dana\r\nfullname=Dana\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=10143\r\nuser=dana\r\npass=secret\r\n")
shots = os.path.join(work, "shots")
shutil.rmtree(shots, ignore_errors=True)
r = rig.Rig(work, machine)
checks = []


def check(what, good):
    checks.append(good)
    print("  %s   %s" % ("ok" if good else "FAIL", what), flush=True)


def white(path):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    return sum(n for n, c in im.getcolors(w * h) if c == (255, 255, 255)) / float(w * h)


try:
    up = r.settle(30, 150)
    s1 = r.shot("n1-running")
    check("EMail started and drew its window (no NATFEATS= in EMAIL.INF)", up)
    r.ctrl("q")
    time.sleep(8)
    s2 = r.shot("n2-quit")
    check("^Q: EMail gone, the desktop back (white %.0f%% -> %.0f%%)" % (100 * white(s1), 100 * white(s2)),
          white(s2) < white(s1) - 0.15)
    crash = [f for f in os.listdir(hd) if f.lower() == "crash.log"]
    check("no CRASH.LOG", not crash)
    inf = open(os.path.join(hd, "email.inf"), "rb").read().decode("latin-1").lower()
    check("EMAIL.INF still without NATFEATS=", "natfeats=" not in inf)
finally:
    r.stop()
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
