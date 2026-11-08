#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Settings > Connection > Languages... (the family's dialog, common/ui/langs.c;
Erez, 8 Nov 2026) and Translate into's pop-up, on an emulated Falcon in 16
colours, against common's stand-in translategate (common/tests/langs/fakegate.py,
port 8480: it has Hebrew installed, 600 MB of room, and keeps what it was asked).
EMail runs alone and offline; FAKESTNG.PRG carries its connection to the fake.

1. Languages..., German ticked (G, G, Space), Esc: the list was asked, no POST.
2. Translate into's pop-up: English and Hebrew (two rows).
3. Languages... again, German ticked, Return: one POST, "de,he"; the pop-up
   during the job still has two rows (what the Gateway has until it's done).
4. Settings closed with OK: EMail follows the job (GET /job) to "Languages
   installed"; Settings again, the pop-up asks the list again and has three
   rows; German chosen (its second row) and OK: EMAIL.INF has TRANSLATETO=de.

    langs_test.py WORKDIR [MACHINE]   (Falcon:16 only: the clicks are placed for it)
"""
import os
import shutil
import sys
import time

from PIL import Image, ImageChops

import rig
from rig import K

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "common", "tests", "langs"))
import fakegate  # noqa: E402

work, machine, more = rig.cli("Falcon:16")
if machine != "Falcon:16":
    print("SKIPPED: the whole test on %s: the clicks are placed for the Falcon's 640x480 in 16 colours" % machine)
    print("%s: 0 passed, 1 skipped" % os.path.basename(__file__))
    sys.exit(0)
os.makedirs(work, exist_ok=True)
hd = os.path.join(work, "hd")
for d in (hd, hd + ".home", os.path.join(work, "shots")):
    shutil.rmtree(d, ignore_errors=True)
os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd)
rig.fakesting(os.path.join(hd, "AUTO"))
with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
    f.write("[options]\r\ntz=180\r\noffline=1\r\n[account]\r\nname=Dana\r\nfullname=Dana\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=10143\r\nuser=dana\r\n"
            "pass=x\r\nsmtphost=127.0.0.1\r\nsmtpport=10025\r\n")
gate = fakegate.Gate(port=8480, cap_mb=600, installed=("he",))
gate.start()

CONNECTION = (150, 353)         # Settings' sidebar: Connection
LANGS = (270, 318)              # its "Languages..." button
POPUP = (480, 296)              # "Translate messages into"'s pop-up
OK = (495, 360)                 # Settings' OK
fails = []


def check(what, good, detail=""):
    print("  %s %s%s" % ("ok  " if good else "FAIL", what, ("  (" + detail + ")") if detail else ""), flush=True)
    if not good:
        fails.append(what)


def asked(method, path):
    return [b for m, p, b in gate.log if m == method and p.startswith(path)]


def popup_rows(shot, before):
    """the pop-up's rows: the height of what changed under the button, by 16 lines"""
    a, b = Image.open(before).convert("RGB"), Image.open(shot).convert("RGB")
    box = (POPUP[0] - 60, POPUP[1] + 10, POPUP[0] + 60, POPUP[1] + 120)
    d = ImageChops.difference(a.crop(box), b.crop(box)).getbbox()
    return 0 if not d else round((d[3] - d[1] - 6) / 18.0)


def settings(r):
    r.key(rig.SCAN[","], ctrl=True)
    time.sleep(4)
    r.click(*CONNECTION)
    time.sleep(2)


def german(r):
    r.type("g")                      # Galician
    time.sleep(1)
    r.type("g")                      # German
    time.sleep(1)
    r.key(0x39)                      # Space: ticked
    time.sleep(1)


fifo_out, fifo_in = os.path.join(work, "st_out"), os.path.join(work, "st_in")
for p in (fifo_out, fifo_in):
    if os.path.exists(p):
        os.remove(p)
    os.mkfifo(p)
bridge = rig.bridge(work, fifo_out, fifo_in)
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
try:
    r.settle(20, 90)
    # 1. ticked, then Esc: nothing sent
    settings(r)
    page = r.shot("1-connection")
    r.click(*LANGS)
    time.sleep(6)
    dialog = r.shot("1-languages")
    check("Languages... opens the dialog", rig.Rig.clicked(page, dialog))
    check("... which asked the Gateway for its list", len(asked("GET", "/languages")) == 1, str(gate.log))
    german(r)
    r.shot("1-german-ticked")
    r.key(K["ESC"])
    time.sleep(2)
    check("Esc sends nothing", not asked("POST", "/languages"), str(gate.log))
    # 2. the pop-up: what the Gateway has
    plain = r.shot("2-before-popup")
    r.click(*POPUP)
    time.sleep(3)
    n = popup_rows(r.shot("2-popup"), plain)
    check("Translate into lists English and Hebrew", n == 2, "%d rows" % n)
    r.key(K["ESC"])
    time.sleep(1)
    # 3. ticked, then Return: one POST
    r.click(*LANGS)
    time.sleep(6)
    german(r)
    r.key(K["RETURN"])
    time.sleep(3)
    posts = asked("POST", "/languages")
    check("OK sends one request, the ticked set", posts == [b"de,he"], str(posts))
    plain = r.shot("3-before-popup")
    r.click(*POPUP)
    time.sleep(3)
    n = popup_rows(r.shot("3-popup-during-job"), plain)
    check("during the job the pop-up still lists the two the Gateway has", n == 2, "%d rows" % n)
    r.key(K["ESC"])
    time.sleep(1)
    # 4. Settings closed: the job followed to its end
    r.click(*OK)
    time.sleep(12)
    r.shot("4-installed")
    check("EMail follows the job", len(asked("GET", "/job")) >= 1, str(gate.log))
    settings(r)
    gets = len(asked("GET", "/languages"))
    plain = r.shot("4-before-popup")
    r.click(*POPUP)
    time.sleep(6)
    n = popup_rows(r.shot("4-popup"), plain)
    check("after the job the pop-up asks the list again", len(asked("GET", "/languages")) == gets + 1, str(gate.log))
    check("... and lists English, German and Hebrew", n == 3, "%d rows" % n)
    r.click(POPUP[0] - 20, POPUP[1] + 38)   # its second row: German
    time.sleep(2)
    r.click(*OK)
    time.sleep(3)
    r.ctrl("q")
    time.sleep(5)
finally:
    r.stop()
    bridge.terminate()
    gate.stop()
inf = open(os.path.join(hd, "email.inf"), errors="replace").read().lower()
check("German chosen and kept: TRANSLATETO=de", "translateto=de" in inf)
print("screenshots in", os.path.join(work, "shots"))
print("languages test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
