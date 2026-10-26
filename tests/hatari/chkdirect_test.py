#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EMail check with a Direct connection account on a Falcon as sold
(Falcon-4: ST RAM only). The accessory takes the 230 KB a Direct
connection needs only from TT RAM, so it leaves the account unchecked;
Erez's Falcon without its DFB1X said only "no memory for a Direct
connection here: HEAPKB= in EMAILCHK.INF" (26 Oct 2026). Now Check now
says what it is and the two ways out: the gateway, or HEAPKB=300 in
C:\\EMAILCHK.INF and a restart.

Nothing is reached on the network (the account is never connected to),
so no server runs: the accessory from the Desk menu, Check now, and the
console's line and the box's text are checked.

    chkdirect_test.py WORKDIR [MACHINE]     (Falcon-4:16 without)
"""
import os
import sys
import time

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "common", "tests", "speed"))
import speed  # noqa: E402

a = sys.argv[1:]
if not a:
    sys.exit(__doc__)
work = os.path.abspath(a[0])
machine = a[1] if len(a) > 1 else "Falcon-4:16"
fails = []


def check(what, good, detail=""):
    print("  %s %s%s" % ("ok  " if good else "FAIL", what, ("  (" + detail + ")") if detail else ""), flush=True)
    if not good:
        fails.append(what)


r = speed.Rig(work, machine, display=os.environ.get("SPEED_DISPLAY", ":91"))
try:
    r.put(os.path.join(ROOT, "binary", "EMAILCHK.ACC"))
    r.put(os.path.join(ROOT, "binary", "TLS.OVL"), "EMAIL")
    r.write("EMAILCHK.INF", "FOLDER=C:\\EMAIL\\\n")
    r.write(os.path.join("EMAIL", "EMAIL.INF"),
            "[options]\ntz=0\ncheck=1\nfalcon=1\n[account]\nname=Dana\nemail=dana@test.local\nin=imap\n"
            "host=192.168.1.2\nport=143\nuser=dana\npass=secret\nsmtphost=192.168.1.2\nsmtpport=587\n"
            "dhost=imap.example.org\ndport=993\ndsec=1\n")
    r.start(boot=120)
    time.sleep(10)
    r.still()
    row = 16 if Image.open(r.shot("desk")).size[1] >= 400 else 8
    r.menu(40, row + 2 * row + row // 2 + 3)           # the Desk menu's accessory
    time.sleep(3)
    r.shot("opened")
    check("the Desk menu opens it", "EMAILCHK opened" in r.console())
    r.key("Return")                                    # Check now
    time.sleep(4)
    r.still()
    box = r.shot("checked")
    con = r.console()
    line = [l for l in con.splitlines() if l.startswith("EMAILCHK checked")]
    check("the account is left unchecked, and why", bool(line) and "a Direct connection needs more memory" in line[-1],
          line[-1] if line else "no check line")
    # the box: Hatari's screen read back as text is beyond this rig; the
    # box is in shots/checked.png, and its text is the program's own
    print("the box:", box, flush=True)
    r.key("Return")
    time.sleep(2)
finally:
    r.stop()

# the way out the box gives: HEAPKB=300, and a restart: now it tries the
# account (the server isn't there: an error, but not the memory)
r = speed.Rig(work + "-heap", machine, display=os.environ.get("SPEED_DISPLAY", ":91"))
try:
    r.put(os.path.join(ROOT, "binary", "EMAILCHK.ACC"))
    r.put(os.path.join(ROOT, "binary", "TLS.OVL"), "EMAIL")
    r.put(os.path.join(ROOT, "binary", "ROOTS.DAT"), "EMAIL")
    r.write("EMAILCHK.INF", "FOLDER=C:\\EMAIL\\\nHEAPKB=300\n")
    r.write(os.path.join("EMAIL", "EMAIL.INF"),
            "[options]\ntz=0\ncheck=1\nfalcon=1\n[account]\nname=Dana\nemail=dana@test.local\nin=imap\n"
            "host=192.168.1.2\nport=143\nuser=dana\npass=secret\nsmtphost=192.168.1.2\nsmtpport=587\n"
            "dhost=imap.example.org\ndport=993\ndsec=1\n")
    r.start(boot=120)
    time.sleep(10)
    r.still()
    r.menu(40, row + 2 * row + row // 2 + 3)
    time.sleep(3)
    r.key("Return")                                    # Check now
    time.sleep(20)
    r.still()
    r.shot("heapkb-checked")
    line = [l for l in r.console().splitlines() if l.startswith("EMAILCHK checked")]
    check("with HEAPKB=300 it tries the account (no memory note)",
          bool(line) and "needs more memory" not in line[-1], line[-1] if line else "no check line")
    r.key("Return")
finally:
    r.stop()
print("all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
