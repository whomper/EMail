#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Settings from the keyboard (D25, guidelines 5.10; Erez, 10 Nov 2026:
"kept as built", as the family's Settings engine). EMail offline, no
server: Options > Settings opens on Mail with the cursor in its first
field;

- Tab three times: past the three numbers to "Keep read messages on disk",
  a ring round its tick; Space switches it off;
- Shift+Tab four times: back through the fields to the list of pages,
  the ring round "Mail"; Down three times: the Look page;
- Tab: "Use colours"; Space switches it off; Return: OK.

EMAIL.INF then says KEEPCACHE=0 and COLOUR=0 (both were 1). The ring
shows in the screenshots k1 to k3.

    keys_test.py WORKDIR [MACHINE]
"""
import os
import shutil
import sys
import time

import rig
from rig import K

work, machine, more = rig.cli("Falcon")
os.makedirs(work, exist_ok=True)
hd = os.path.join(work, "hd")
shutil.rmtree(hd, ignore_errors=True)
os.makedirs(hd)
rig.install_prg(hd)
with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
    f.write("[options]\r\nnatfeats=1\r\ntz=180\r\nstartoffline=1\r\nkeepcache=1\r\ncolour=1\r\n"
            "[account]\r\nname=Dana\r\nfullname=Dana\r\nemail=dana@test.local\r\nin=imap\r\n"
            "host=127.0.0.1\r\nport=10143\r\nuser=dana\r\npass=secret\r\n")
shots = os.path.join(work, "shots")
shutil.rmtree(shots, ignore_errors=True)
r = rig.Rig(work, machine)
checks = []


def check(what, good):
    checks.append(good)
    print("  %s   %s" % ("ok" if good else "FAIL", what), flush=True)


def inf_value(key):
    for line in open(os.path.join(hd, "email.inf"), "rb").read().decode("latin-1").splitlines():
        if line.lower().startswith(key.lower() + "="):
            return line.split("=", 1)[1]
    return None


def menu(x, y):
    r.click(x, 9)
    time.sleep(0.6)
    r.click(x + 20, y)
    time.sleep(2)


try:
    r.settle(30, 90)
    r.shot("k0-start")
    menu(340, 43)                        # Options > Settings...
    r.key(K["TAB"])
    time.sleep(0.4)
    r.key(K["TAB"])                      # the large messages' size (2.26)
    time.sleep(0.4)
    r.key(K["TAB"])                      # the tick "Keep read messages on disk"
    time.sleep(0.6)
    r.shot("k1-ring-on-tick")
    r.key(0x39)                          # Space: off
    time.sleep(0.8)
    for _ in range(4):
        r.key(K["TAB"], shift=True)      # the fields, then the list
        time.sleep(0.4)
    r.shot("k2-ring-on-list")
    for _ in range(3):
        r.key(K["DOWN"])                 # Writing, Signature, Look
        time.sleep(1.2)
    r.key(K["TAB"])                      # "Use colours"
    time.sleep(0.6)
    r.shot("k3-look-tick")
    r.key(0x39)                          # Space: off
    time.sleep(0.8)
    r.key(K["RETURN"])                   # OK
    time.sleep(3)
    print("keepcache:", inf_value("keepcache"), "colour:", inf_value("colour"))
    check("Tab, Tab, Space: Keep read messages on disk switched off", inf_value("keepcache") == "0")
    check("Shift+Tab to the list, Down to Look, Tab, Space: Use colours off", inf_value("colour") == "0")
finally:
    r.stop()
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
