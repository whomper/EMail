#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EMail under a multitasking AES: FreeMiNT with XaAES (common's rig,
common/tests/xaaes/run.py: a Falcon with EmuTOS, 640x480, 16 colours).

A multitasking AES shows what is behind a window until its program draws
it, and EMail used to leave its pane bars and dividers to the AES's first
WM_REDRAW, which waited behind the welcome box or the first check: the
desktop showed through in strips (a tester under MiNT with MyAES, 25 Oct
2026). Here the desktop is cyan, a colour EMail never uses, so any cyan
inside EMail's window is a part it didn't draw.

1. The first start: the welcome box over the whole window.
2. With an account (EMAIL.INF): the check at the start, as the tester's
   (POP3); it can't connect here, and its box comes over the window.

    xaaes_test.py WORKDIR          (EMAIL_PRG= another build to test)

Needs `make base` in common/tests/xaaes once, and EmuTOS 1.4's 512k image
(~/.cache/aera/emutos-1.4/etos512us.img, common/tests/xaaes/fetch.py).
"""
import os
import subprocess
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
RUN = os.path.join(ROOT, "common", "tests", "xaaes", "run.py")
ETOS = os.path.expanduser("~/.cache/aera/emutos-1.4/etos512us.img")
CYAN = (0, 255, 255)

work = sys.argv[1] if len(sys.argv) > 1 else "/tmp/email-xaaes"
os.makedirs(work, exist_ok=True)
prg = os.environ.get("EMAIL_PRG", os.path.join(ROOT, "binary", "EMAIL.PRG"))
fails = []


def check(what, good, detail=""):
    print("  %s %s%s" % ("ok  " if good else "FAIL", what, ("  (" + detail + ")") if detail else ""), flush=True)
    if not good:
        fails.append(what)


def run(name, steps, copy=()):
    w = os.path.join(work, name)
    cmd = [sys.executable, RUN, w, ETOS, prg, "--display", ":97", "--steps", steps]
    for c in copy:
        cmd += ["--copy", c]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
    print(r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr[-400:], flush=True)
    return w, r.returncode


def undrawn(shot):
    """the desktop's pixels inside EMail's window (below the menu bar); not
    the strip at the left a fulled window leaves, its work area on 8
    pixels across (wind_x8: the family's, 26 Oct 2026)"""
    im = Image.open(shot).convert("RGB")
    w, h = im.size
    return sum(1 for x in range(8, w) for y in range(40, h) if im.getpixel((x, y)) == CYAN)


# 1. the first start: the welcome box comes at once
w, rc = run("first", "until EMail 30;wait 20;shot")
check("the first start runs", rc == 0)
shot = os.path.join(w, "shots", "grab0001.png")
if os.path.exists(shot):
    n = undrawn(shot)
    check("behind the welcome box, EMail's window is drawn whole", n < 20, "%d desktop pixels" % n)

# 2. an account: the check at the start, then its box
inf = os.path.join(work, "EMAIL.INF")
with open(inf, "w", newline="") as f:
    f.write("[options]\r\n[account]\r\nname=Dana\r\nemail=dana@example.com\r\nin=pop3\r\n"
            "host=192.168.1.2\r\nport=110\r\nuser=dana\r\npass=secret\r\n")
w, rc = run("account", "until EMail 30;wait 20;shot", copy=[inf])
check("with an account it runs", rc == 0)
shot = os.path.join(w, "shots", "grab0001.png")
if os.path.exists(shot):
    n = undrawn(shot)
    check("its window is drawn whole, behind the check's box", n < 20, "%d desktop pixels" % n)

print("screenshots in %s/*/shots" % work)
print("%d failed" % len(fails) if fails else "all passed")
sys.exit(1 if fails else 0)
