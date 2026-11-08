#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The Help key (the AERA guidelines, 4.3): with a help viewer loaded,
EMail's Help key asks it for EMAIL.HYP with VA_START, on a Falcon in 16
colours and on a TT in black and white.

EMail runs alone (guidelines 1.2): build/fakehelp.acc is C:\\EHYPVIEW.ACC,
a stand-in that answers to EHYPVIEW and writes what VA_START named into
C:\\EHYPVIEW.LOG. How EHypView shows the page is the hub's family check.

    help_test.py WORKDIR [MACHINE]   (make help build/fakehelp.acc first)
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
shutil.rmtree(work, ignore_errors=True)
os.makedirs(work)
checks = []


def check(what, good, detail=""):
    checks.append(good)
    print("  %s   %s%s" % ("ok" if good else "FAIL", what, ("  (" + detail + ")") if detail else ""))


def look(machine):
    name = machine.replace(":", "-")
    w = os.path.join(work, name)
    hd = os.path.join(w, "hd")
    os.makedirs(hd)
    shutil.copy(os.path.join(ROOT, "build", "fakehelp.acc"), os.path.join(hd, "EHYPVIEW.ACC"))
    shutil.copy(os.path.join(ROOT, "binary", "email.hyp"), hd)
    rig.install_prg(hd)
    with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
        f.write("[options]\r\ntz=180\r\noffline=1\r\n[account]\r\nname=Dana\r\nfullname=Dana\r\n"
                "email=dana@test.local\r\nin=imap\r\nhost=192.168.1.2\r\nport=143\r\nuser=dana\r\n"
                "pass=x\r\nsmtphost=192.168.1.2\r\nsmtpport=587\r\n")
    r = rig.Rig(w, machine)
    try:
        r.settle(20, 90)                     # EMail's window, offline
        r.key(K["HELP"])                     # EMail asks the viewer: VA_START C:\email.hyp
        time.sleep(5)
        r.shot("%s-1" % name)
    finally:
        r.stop()
    p = os.path.join(hd, "EHYPVIEW.LOG")
    log = open(p, errors="replace").read() if os.path.exists(p) else ""
    starts = [l[6:] for l in log.splitlines() if l.startswith("start ")]
    what = r.machine.describe()
    check("%s: the Help key asks the viewer for EMAIL.HYP" % what,
          any(s.lower().split(" ")[0] == "c:\\email.hyp" for s in starts), log.strip().replace("\n", " | "))


for m in looks:
    look(m)
print("screenshots in", work)
ok = checks and all(checks)
print("help test:", "all passed" if ok else "%d failed" % checks.count(False))
sys.exit(0 if ok else 1)
