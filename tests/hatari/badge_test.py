#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The Hebrew keyboard's badge (Erez, 6 Nov 2026; guideline 5.3): the
letters ayin and bet at the right end of the main window's top row (the
message list's header: the info line is the AES's own, which it draws
again unasked) and of the Settings dialog's title band while the keyboard
is on (F10), gone when it is off. EMail starts offline (STARTOFFLINE=1):
no network.

Each step is a screenshot, compared with the one before:
1. F10: only a small patch at the right end of the top row changes.
2. The editor (Ctrl+N) over the main window, the main window on top
   again (^W), the editor closed (^W, ^U): the badge is still there.
3. Settings (Ctrl+,): the band's right end has the badge, white on black.
4. Settings left (Undo), F10 again: the info line as it was at first.

    badge_test.py WORKDIR [MACHINE]
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
F10, UNDO, COMMA = 0x44, 0x61, 0x33

work, machine, more = rig.cli("Falcon")
os.makedirs(work, exist_ok=True)
hd = os.path.join(work, "hd")
shutil.rmtree(hd, ignore_errors=True)
os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd)
with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
    f.write("[options]\r\ntz=180\r\nstartoffline=1\r\n[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=10143\r\nuser=dana\r\n"
            "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=10025\r\n")
fails = []


def check(what, good, detail=""):
    print("  %s %s%s" % ("ok  " if good else "FAIL", what, ("  (" + detail + ")") if detail else ""), flush=True)
    if not good:
        fails.append(what)


def diff(a, b):
    """the box where two screenshots differ, or None"""
    return ImageChops.difference(Image.open(a).convert("RGB"), Image.open(b).convert("RGB")).getbbox()


r = rig.Rig(work, machine)
try:
    r.settle(30, 240)
    off = r.shot("b0-off")
    w, h = Image.open(off).size
    r.key(F10)
    time.sleep(2)
    on = r.shot("b1-on")
    box = diff(off, on)
    check("F10: something drawn", box is not None, str(box))
    if box:
        x0, y0, x1, y1 = box
        check("... only a patch the badge's size", x1 - x0 <= 24 and y1 - y0 <= 16, str(box))
        check("... at the right end of the window's top row (the list's header), left of the slider column",
              w - 60 < x1 < w - 10, str(box))
        check("... in the work area's top row, under the info line", h // 10 < y0 < h // 4, str(box))
    def badge_there(shot):
        if not box:
            return False
        crop = box
        return ImageChops.difference(Image.open(on).convert("RGB").crop(crop),
                                     Image.open(shot).convert("RGB").crop(crop)).getbbox() is None
    r.ctrl("n")
    time.sleep(3)
    check("the editor opened over the window: the badge still there", badge_there(r.shot("b2-editor")))
    r.ctrl("w")
    time.sleep(2)
    check("^W, the main window on top: the badge there", badge_there(r.shot("b3-main")))
    r.ctrl("w")
    time.sleep(2)
    r.ctrl("u")
    time.sleep(3)
    check("the editor closed: the badge there", badge_there(r.shot("b3-back")))
    r.key(COMMA, ctrl=True)
    time.sleep(4)
    st = r.shot("b4-settings")
    r.key(UNDO)
    time.sleep(3)
    r.key(F10)
    time.sleep(2)
    off2 = r.shot("b5-off")
    # Settings: the band is the black row with the most black; its right
    # end, where the badge is, has white pixels in it
    im = Image.open(st).convert("L")
    best, by = 0, -1
    for y in range(h):
        n = sum(1 for x in range(0, w, 4) if im.getpixel((x, y)) < 60)
        if n > best:
            best, by = n, y
    xs = [x for x in range(w) if im.getpixel((x, by)) < 60]
    if xs:
        right = max(xs)
        white = sum(1 for x in range(right - 22, right - 2) for y in range(max(0, by - 8), min(h, by + 8))
                    if im.getpixel((x, y)) > 200)
        check("Settings: white pixels at the right end of the black band", white > 6,
              "band row %d, right %d, white %d" % (by, right, white))
    else:
        check("Settings: a black band", False)
    box2 = diff(off, off2)
    check("F10 again: the window as at first", box2 is None, str(box2))
finally:
    r.stop()
print("badge test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
