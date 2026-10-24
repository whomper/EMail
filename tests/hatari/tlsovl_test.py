#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""TLS.OVL, a Direct connection's TLS in a file of its own (the AERA
guidelines, 7.4), as EMail finds it when Options > Direct connection is
chosen. Offline, so no server is needed.

1. On a Falcon without TLS.OVL next to EMAIL.PRG: an alert says so, the
   Direct connection stays off, and EMail goes on and quits as ever (no
   CRASH.LOG).
2. On a Falcon with it: it loads (EMAIL.LOG: "TLS.OVL 1.0 loaded"), the
   Direct connection is on, and EMAIL.INF says FALCON=1.
3. On an ST (a 68000): an alert says it needs a 68020, and TLS.OVL,
   though it is there, isn't loaded.

    tlsovl_test.py WORKDIR
"""
import os
import shutil
import sys
import time

import rig
from rig import K

work = sys.argv[1] if len(sys.argv) > 1 else sys.exit(__doc__)
fails = []


def check(what, good, detail=""):
    print("  %s %s%s" % ("ok  " if good else "FAIL", what, ("  (" + detail + ")") if detail else ""), flush=True)
    if not good:
        fails.append(what)


def changed(a, b):
    from PIL import Image, ImageChops
    x, y = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    d = ImageChops.difference(x, y).convert("L").point(lambda v: 255 if v else 0)
    return d.histogram()[255] / float(x.size[0] * x.size[1])


def run(name, machine, ovl, option_y):
    """EMail offline on machine, Options > Direct connection chosen; the
    alert's screen (or None), EMAIL.INF and EMAIL.LOG afterwards"""
    w = os.path.join(work, name)
    shutil.rmtree(w, ignore_errors=True)
    hd = os.path.join(w, "hd")
    os.makedirs(hd)
    rig.install_prg(hd, ovl=ovl)
    shutil.copy(os.path.join(rig.ROOT, "binary", "CACERT.PEM"), hd)
    with open(os.path.join(hd, "EMAIL.INF"), "w", newline="") as f:
        f.write("[options]\r\ntz=180\r\noffline=1\r\nlog=1\r\n[account]\r\nname=Dana\r\nfullname=Dana\r\n"
                "email=dana@test.local\r\nin=imap\r\nhost=192.168.1.2\r\nport=143\r\nuser=dana\r\npass=x\r\n"
                "smtphost=192.168.1.2\r\nsmtpport=587\r\ndhost=imap.test.local\r\ndport=993\r\n"
                "dsmtphost=smtp.test.local\r\ndsmtpport=465\r\n")
    r = rig.Rig(w, machine)
    try:
        r.settle(20, 90)
        before = r.shot("%s-0" % name)
        r.click(340, option_y[0])        # Options
        time.sleep(1)
        r.shot("%s-1-menu" % name)
        r.click(380, option_y[1])        # Direct connection (TLS)
        time.sleep(6)
        after = r.shot("%s-2" % name)
        r.key(K["RETURN"])               # the alert's OK, if there is one
        time.sleep(2)
        r.ctrl("q")
        time.sleep(6)
    finally:
        r.stop()
    inf = open(os.path.join(hd, "EMAIL.INF"), errors="replace").read()
    log = open(os.path.join(hd, "EMAIL.LOG"), errors="replace").read() if os.path.exists(
        os.path.join(hd, "EMAIL.LOG")) else ""
    return changed(before, after), inf, log, os.path.exists(os.path.join(hd, "CRASH.LOG"))


# the Options menu and its Direct connection item (640x480 and 640x400:
# the menu bar's items are 16 pixels apart either way)
FALCON = (8, 104)

d, inf, log, crash = run("missing", "Falcon:16", False, FALCON)
check("Falcon without TLS.OVL: an alert", d > 0.01, "%.1f%% of the screen" % (100 * d))
check("... the Direct connection stays off", "FALCON=0" in inf)
check("... and EMail quits as ever, no CRASH.LOG", not crash)

d, inf, log, crash = run("present", "Falcon:16", True, FALCON)
line = [l for l in log.splitlines() if "TLS.OVL" in l]
check("Falcon with TLS.OVL: it loads", any("loaded" in l for l in line), " | ".join(line[:2]))
check("... the Direct connection is on", "FALCON=1" in inf)
check("... no CRASH.LOG", not crash)

d, inf, log, crash = run("st", "ST-1", True, FALCON)
check("ST: an alert (a 68020 or better)", d > 0.01, "%.1f%% of the screen" % (100 * d))
check("... TLS.OVL not loaded", "TLS.OVL" not in log)
check("... the Direct connection stays off, no CRASH.LOG", "FALCON=0" in inf and not crash)

print("screenshots in", work)
print("tlsovl test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
