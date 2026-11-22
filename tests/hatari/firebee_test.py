#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Each of EMail's files on the machine it isn't for says which one to run
(the FireBee edition: Erez, 22 Nov 2026; the guidelines, 7.4).

1. EMAIL.PRG on the FireBee stand-in (_CF_ in the jar): its alert
   (cpu_cf_alert: run EMAILFB.PRG), and after Return it is gone: no
   EMAIL.LOG (its own code never ran), no CRASH.LOG.
2. EMAILCHK.ACC on the FireBee stand-in: its alert once as the desktop
   comes up, then the accessory stays quiet and the desktop goes on (its
   Desk menu opens and closes).
3. EMAILFB.PRG on a Falcon (a 68030, no _CF_) and on an ST (a 68000): its
   alert (cpu_alert: run EMAIL.PRG), then gone, no EMAIL.LOG.
4. EMCHKFB.ACC on an ST-1: its alert once, then the desktop goes on.

The FireBee edition's own code can't run here (the stand-in is a 68060,
without the ColdFire's own instructions): make cftest runs its core under
qemu-m68k -cpu cfv4e.

    firebee_test.py WORKDIR
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


INF = ("[options]\r\nnatfeats=1\r\ntz=180\r\noffline=1\r\nlog=1\r\n[account]\r\nname=Dana\r\nfullname=Dana\r\n"
       "email=dana@test.local\r\nin=imap\r\nhost=192.168.1.2\r\nport=143\r\nuser=dana\r\npass=x\r\n")


def prg(name, machine, src, as_name):
    """a program started by the desktop: its alert's screen against the
    desktop's after Return, and EMAIL.LOG and CRASH.LOG afterwards"""
    w = os.path.join(work, name)
    shutil.rmtree(w, ignore_errors=True)
    hd = os.path.join(w, "hd")
    os.makedirs(hd)
    shutil.copy(os.path.join(rig.ROOT, "binary", src), os.path.join(hd, as_name))
    with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
        f.write(INF)
    r = rig.Rig(w, machine, auto="C:\\" + as_name.upper())
    try:
        time.sleep(30)
        alert = r.shot("%s-1-alert" % name)
        r.key(K["RETURN"])
        time.sleep(6)
        after = r.shot("%s-2-after" % name)
        alive = r.proc.poll() is None
    finally:
        r.stop()
    has = lambda n: any(f.lower() == n for f in os.listdir(hd))
    return changed(alert, after), alive, has("email.log"), has("crash.log")


def acc(name, machine, src):
    """an accessory in C:\\ and no program: its alert as the desktop comes
    up; after Return the desktop's Desk menu opens and closes"""
    w = os.path.join(work, name)
    shutil.rmtree(w, ignore_errors=True)
    hd = os.path.join(w, "hd")
    os.makedirs(hd)
    shutil.copy(os.path.join(rig.ROOT, "binary", src), os.path.join(hd, src))
    r = rig.Rig(w, machine, auto=None)
    try:
        time.sleep(30)
        alert = r.shot("%s-1-alert" % name)
        r.key(K["RETURN"])
        time.sleep(4)
        desk = r.shot("%s-2-desktop" % name)
        r.move(20, 8)                    # the Desk menu
        time.sleep(2)
        menu = r.shot("%s-3-desk-menu" % name)
        r.move(300, 300)
        r.click(300, 300)                # closed again
        time.sleep(2)
        again = r.shot("%s-4-desktop" % name)
        alive = r.proc.poll() is None
    finally:
        r.stop()
    return changed(alert, desk), changed(desk, menu), changed(desk, again), alive


d, alive, log, crash = prg("prg-firebee", "FireBee:16", "email.prg", "email.prg")
check("EMAIL.PRG on the FireBee stand-in: an alert", d > 0.01, "%.1f%% of the screen" % (100 * d))
check("... then gone, EMail's own code never ran (no EMAIL.LOG), no CRASH.LOG", alive and not log and not crash)

d, menu, again, alive = acc("acc-firebee", "FireBee:16", "emailchk.acc")
check("EMAILCHK.ACC on the FireBee stand-in: an alert", d > 0.01, "%.1f%% of the screen" % (100 * d))
check("... then the desktop goes on: its Desk menu opens and closes", alive and menu > 0.002 and again < 0.002,
      "menu %.2f%%, after %.2f%%" % (100 * menu, 100 * again))

for name, machine in (("fb-falcon", "Falcon:16"), ("fb-st", "ST:mono")):
    d, alive, log, crash = prg(name, machine, "emailfb.prg", "emailfb.prg")
    check("EMAILFB.PRG on %s: an alert" % machine, d > 0.01, "%.1f%% of the screen" % (100 * d))
    check("... then gone, no EMAIL.LOG, no CRASH.LOG", alive and not log and not crash)

d, menu, again, alive = acc("fbacc-st1", "ST-1", "emchkfb.acc")
check("EMCHKFB.ACC on an ST-1: an alert", d > 0.01, "%.1f%% of the screen" % (100 * d))
check("... then the desktop goes on: its Desk menu opens and closes", alive and menu > 0.002 and again < 0.002,
      "menu %.2f%%, after %.2f%%" % (100 * menu, 100 * again))

print("screenshots in", work)
print("firebee test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
