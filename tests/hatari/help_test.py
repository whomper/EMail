#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EMAIL.HYP in EHypView's latest release (the AERA guidelines, 1.2 and
4.3), as Erez reads it: the front page opens with its two pictures (the
banner and the main window), on a Falcon in 16 colours and on a TT in
black and white. Screenshots of the page, top to bottom, for people to
look at.

    help_test.py WORKDIR [MACHINE] [vX.Y]   (make help first; EHypView's latest release, or that one)
"""
import os
import shutil
import sys
import time

import rig
from rig import K

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))

work, machine, more = rig.cli("Falcon")
# MACHINE given: that one only; else a Falcon in 16 colours and a TT in black and white
looks = [machine] if len(sys.argv) > 2 and sys.argv[2] == machine else ["Falcon:16", "TT:mono"]
spec = "EHypView@" + more[0] if more else "EHypView"
shutil.rmtree(work, ignore_errors=True)
os.makedirs(work)
ehv = os.path.join(work, "ehypview")
tag = rig.release(spec, ehv, "EHYPVIEW.PRG")
checks = []


def check(what, good):
    checks.append(good)
    print("  %s   %s" % ("ok" if good else "FAIL", what))


def look(machine):
    name = machine.replace(":", "-")
    w = os.path.join(work, name)
    hd = os.path.join(w, "hd")
    os.makedirs(hd)
    shutil.copy(os.path.join(ehv, "EHYPVIEW.PRG"), hd)
    shutil.copy(os.path.join(ROOT, "binary", "EMAIL.HYP"), hd)
    with open(os.path.join(hd, "ST-GUIDE.INF"), "w", newline="") as f:
        f.write("PATHS=C:\\\r\nDEFHYP=C:\\EMAIL.HYP\r\n")
    r = rig.Rig(w, machine, auto="C:\\EHYPVIEW.PRG")
    try:
        time.sleep(25)
        if "EHV page" not in open(os.path.join(w, "hatari.log"), errors="replace").read():
            # EHypView 1.22 and later start on their list of hypertexts, EMail's chosen: open it
            r.key(K["RETURN"])
            time.sleep(10)
        r.shot("%s-1" % name)
        for i in range(2, 5):
            r.key(K["DOWN"], shift=True)     # a page down
            time.sleep(1)
            r.shot("%s-%d" % (name, i))
    finally:
        r.stop()
    log = open(os.path.join(w, "hatari.log"), errors="replace").read()
    what = r.machine.describe()
    check("%s: EHypView %s opens EMAIL.HYP's front page" % (what, tag), "EHV page C:\\EMAIL.HYP Main" in log)
    check("%s: with both its pictures" % what, "EHV pictures 2" in log)


for m in looks:
    look(m)
print("screenshots in", work)
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
