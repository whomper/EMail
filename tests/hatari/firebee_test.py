#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Each package's files on the other package's machine say which package to
install (two packages, the same names inside: decided 22 Nov 2026; the
guidelines, 7.4). The standard package's are binary/, the FireBee
package's binary/firebee/.

1. The standard EMAIL.PRG on the FireBee stand-in (_CF_ in the jar): its
   alert (cpu_cf_alert: install the FireBee package), and after Return it
   is gone: no EMAIL.LOG (its own code never ran), no CRASH.LOG.
2. The standard EMAILCHK.ACC on the FireBee stand-in: nothing as the desktop comes up
   (the guidelines, 2.2); its Desk entry, "EMail check", shows the alert
   each time it is opened, and after OK the desktop is as it was.
3. The FireBee package's EMAIL.PRG on a Falcon (a 68030, no _CF_) and on
   an ST (a 68000): its alert (cpu_alert: install the standard package),
   then gone, no EMAIL.LOG.
4. The FireBee package's EMAILCHK.ACC on an ST-1: as 2.

The FireBee package's own code can't run here (the stand-in is a 68060,
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


def prg(name, machine, src):
    """a program started by the desktop: its alert's screen against the
    desktop's after Return, and EMAIL.LOG and CRASH.LOG afterwards"""
    w = os.path.join(work, name)
    shutil.rmtree(w, ignore_errors=True)
    hd = os.path.join(w, "hd")
    os.makedirs(hd)
    shutil.copy(os.path.join(rig.ROOT, "binary", src), os.path.join(hd, "email.prg"))
    with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
        f.write(INF)
    r = rig.Rig(w, machine, auto="C:\\EMAIL.PRG")
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
    """an accessory in C:\\ and no program: the desktop as it came up, then
    the accessory's Desk entry opened twice (the Desk menu's third row,
    after Desktop info and the line): the alert's screen each time against
    the desktop's, and the desktop after OK against the first"""
    w = os.path.join(work, name)
    shutil.rmtree(w, ignore_errors=True)
    hd = os.path.join(w, "hd")
    os.makedirs(hd)
    shutil.copy(os.path.join(rig.ROOT, "binary", src), os.path.join(hd, "emailchk.acc"))
    r = rig.Rig(w, machine, auto=None)
    alerts = []
    try:
        time.sleep(30)
        desk = r.shot("%s-0-desktop" % name)
        for k in (1, 2):
            r.move(20, 8)                    # the Desk menu
            time.sleep(2)
            r.shot("%s-%d-desk-menu" % (name, k))
            r.click(60, 19 + 2 * 16 + 8)     # its third row: "  EMail check"
            time.sleep(4)
            alerts.append(changed(desk, r.shot("%s-%d-alert" % (name, k))))
            r.key(K["RETURN"])
            time.sleep(3)
        after = changed(desk, r.shot("%s-3-desktop" % name))
        alive = r.proc.poll() is None
    finally:
        r.stop()
    return alerts, after, alive


d, alive, log, crash = prg("prg-firebee", "FireBee:16", "email.prg")
check("the standard EMAIL.PRG on the FireBee stand-in: an alert", d > 0.01, "%.1f%% of the screen" % (100 * d))
check("... then gone, EMail's own code never ran (no EMAIL.LOG), no CRASH.LOG", alive and not log and not crash)

alerts, after, alive = acc("acc-firebee", "FireBee:16", "emailchk.acc")
check("the standard EMAILCHK.ACC on the FireBee stand-in: its Desk entry gives the alert, each time",
      min(alerts) > 0.01, " / ".join("%.1f%%" % (100 * a) for a in alerts))
check("... nothing at boot, and after OK the desktop as it was", alive and after < 0.002, "%.2f%%" % (100 * after))

for name, machine in (("fb-falcon", "Falcon:16"), ("fb-st", "ST:mono")):
    d, alive, log, crash = prg(name, machine, "firebee/email.prg")
    check("the FireBee package's EMAIL.PRG on %s: an alert" % machine, d > 0.01, "%.1f%% of the screen" % (100 * d))
    check("... then gone, no EMAIL.LOG, no CRASH.LOG", alive and not log and not crash)

alerts, after, alive = acc("fbacc-st1", "ST-1", "firebee/emailchk.acc")
check("the FireBee package's EMAILCHK.ACC on an ST-1: its Desk entry gives the alert, each time",
      min(alerts) > 0.01, " / ".join("%.1f%%" % (100 * a) for a in alerts))
check("... nothing at boot, and after OK the desktop as it was", alive and after < 0.002, "%.2f%%" % (100 * after))

print("screenshots in", work)
print("firebee test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
