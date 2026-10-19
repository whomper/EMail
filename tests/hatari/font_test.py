#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A GDOS font chosen in EMAIL.PRG, as Erez uses it under NVDI: the
windows should redraw in it, and EMail, started again with it from
EMAIL.INF, should draw its list and a Hebrew message whole.

    font_test.py WORKDIR [MACHINE]     (needs Xvfb, xdotool, dovecot)

Dovecot and FAKESTNG.PRG as in net_test.py. EMail reads the Hebrew
message (tests/mail/1-hebrew.eml), then Options > Font...: Down moves to
the first family after the system font, the first monospaced GDOS font
(the dialog lists only those), at the size it offers; Return is OK. Checks:
the windows redrew, no dialog is left (against the dialog's own shot:
the shot after it would hide a dialog that stayed), and EMAIL.INF says
FONT= that font. Then ^Q, EMAIL.INF set offline, and EMail starts again
in that font: the message list draws, and the Hebrew message, read from
disk, draws right to left; the hint in the reader's header bar stands
clear of the subject, and each header's label of its value (Erez's NVDI,
Courier 14: they ran together). Whether its letters are the right Hebrew ones
is for the eye: shots/g2-hebrew-line.png is the first body line, enlarged.

Without a GDOS the dialog lists only the system font: "no GDOS fonts",
the font steps are skipped and the restart is checked in the system
font. Under NVDI (AERA_WITH_NVDI=1 on a Falcon, common/tools/machines.py)
the boot takes 90 s (120 s on Falcon-4) and the font list longer.
"""
import os
import re
import shutil
import subprocess
import sys
import time

import rig
from rig import K, SCAN

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
IMAP, SMTP = 10143, 10025

work, machine, more = rig.cli("Falcon")
name = machine.partition(":")[0]
nvdi = bool(os.environ.get("AERA_WITH_NVDI")) and name.startswith("Falcon")
boot = (120 if name == "Falcon-4" else 90) if nvdi else 45
fonts_wait = 40 if nvdi else 5          # the dialog measures each font once (EMAIL.FNT)

os.makedirs(work, exist_ok=True)
srv = os.path.join(work, "srv")
hd = os.path.join(work, "hd")
for d in (srv, hd, hd + ".home"):
    shutil.rmtree(d, ignore_errors=True)
run_dir = subprocess.run(["mktemp", "-d", "/tmp/tdv.XXXXXX"], capture_output=True, text=True).stdout.strip()
for d in ("state", "home/dana/Maildir/new", "home/dana/Maildir/cur", "home/dana/Maildir/tmp"):
    os.makedirs(os.path.join(srv, d))
os.chmod(work, 0o755)
os.chmod(srv, 0o755)
with open(os.path.join(srv, "users"), "w") as f:
    f.write("dana:{PLAIN}secret\n")
uid, gid = subprocess.check_output(["id", "-u", "nobody"]).strip(), subprocess.check_output(["id", "-g", "nobody"]).strip()
conf = open(os.path.join(ROOT, "tests", "dovecot.conf.in")).read()
for k, v in (("@RUN@", run_dir), ("@DIR@", srv), ("@IMAP@", str(IMAP)), ("@POP3@", "10110"),
             ("@UID@", uid.decode()), ("@GID@", gid.decode())):
    conf = conf.replace(k, v)
with open(os.path.join(srv, "dovecot.conf"), "w") as f:
    f.write(conf)
for m in sorted(os.listdir(os.path.join(ROOT, "tests", "mail"))):
    shutil.copy(os.path.join(ROOT, "tests", "mail", m), os.path.join(srv, "home/dana/Maildir/new", m))
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

# drive C:
os.makedirs(os.path.join(hd, "AUTO"))
shutil.copy(os.path.join(ROOT, "binary", "EMAIL.PRG"), hd)
rig.fakesting(os.path.join(hd, "AUTO"))
INF = os.path.join(hd, "EMAIL.INF")
with open(INF, "w", newline="") as f:
    f.write("[options]\r\ntz=180\r\n[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
            "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n" % (IMAP, SMTP))

fifo_out, fifo_in = os.path.join(work, "st_out"), os.path.join(work, "st_in")
for p in (fifo_out, fifo_in):
    if os.path.exists(p):
        os.remove(p)
    os.mkfifo(p)
bridge = rig.bridge(work, fifo_out, fifo_in)
shutil.rmtree(os.path.join(work, "shots"), ignore_errors=True)

fails = []


def check(what, good, detail=""):
    print("  %s %s%s" % ("ok  " if good else "FAIL", what, ("  (" + detail + ")") if detail else ""), flush=True)
    if not good:
        fails.append(what)


def changed(a, b, box=None):
    """the share of pixels that differ between two screenshots (in box)"""
    from PIL import Image, ImageChops
    x, y = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    if box is None:
        w, h = x.size
        box = (0, h // 20, w, h)        # below the menu bar
    d = ImageChops.difference(x.crop(box), y.crop(box)).convert("L").point(lambda v: 255 if v else 0)
    return d.histogram()[255] / float((box[2] - box[0]) * (box[3] - box[1]))


def ink_rows(shot, box):
    """the rows in box that have pixels of another colour than its most
    common one, with each row's leftmost and rightmost such pixel"""
    from PIL import Image
    im = Image.open(shot).convert("RGB").crop(box)
    w, h = im.size
    bg = max(im.getcolors(w * h), key=lambda c: c[0])[1]
    px = im.load()
    rows = {}
    for y in range(h):
        xs = [x for x in range(w) if px[x, y] != bg]
        if xs:
            rows[y] = (xs[0], xs[-1])
    return rows


def header_bar(shot, x0, x1):
    """the reader's header bar: the lowest run of 8 rows or more, below
    the top third, where under 60% of the pixels are the windows' white"""
    from PIL import Image
    im = Image.open(shot).convert("RGB")
    W, H = im.size
    px = im.load()
    best, run, start = None, 0, 0
    for y in range(H // 3, H):
        white = sum(px[x, y] == (255, 255, 255) for x in range(x0, x1))
        dark = white < 0.6 * (x1 - x0)
        if dark:
            if not run:
                start = y
            run += 1
        else:
            if run >= 8:
                best = (start, y - 1)
            run = 0
    return best


def bar_gap(shot, x0, x1, y0, y1):
    """in a header bar: (whether its text reaches the right end, the
    widest gap in it, the next widest)"""
    from PIL import Image
    band = Image.open(shot).convert("RGB").crop((x0, y0, x1, y1 + 1))
    w, h = band.size
    p = band.load()
    bg = max(band.getcolors(w * h), key=lambda c: c[0])[1]
    rows = [y for y in range(h) if sum(p[x, y] != bg for x in range(w)) < 0.9 * w]   # not a rule
    ink = [any(p[x, y] != bg for y in rows) for x in range(w)]
    xs = [x for x in range(w) if ink[x]]
    if not xs:
        return False, 0, 0
    gaps, s = [], None
    for x in range(xs[0], xs[-1] + 1):
        if not ink[x] and s is None:
            s = x
        if ink[x] and s is not None:
            gaps.append(x - s)
            s = None
    gaps = sorted(gaps, reverse=True) + [0, 0]
    return xs[-1] > 0.9 * w, gaps[0], gaps[1]


def label_gaps(shot, x0, x1, y0, y1, labels):
    """the header lines under the bar: for each, the gap between its label
    (the dim colour, bold) and its value (the text colour), in the label's
    own character widths"""
    from PIL import Image
    im = Image.open(shot).convert("RGB").crop((x0, y0, x1, y1))
    w, h = im.size
    p = im.load()
    bg = max(im.getcolors(w * h), key=lambda c: c[0])[1]
    lines, s = [], None
    for y in range(h + 1):
        on = y < h and any(p[x, y] != bg for x in range(w))
        if on and s is None:
            s = y
        if not on and s is not None:
            lines.append((s, y - 1))
            s = None
    out = []
    for (a, z), label in zip(lines, labels):
        cols = {}
        for y in range(a, z + 1):
            for x in range(w):
                if p[x, y] != bg:
                    cols.setdefault(p[x, y], []).append(x)
        first = min(cols.items(), key=lambda kv: min(kv[1]))      # the label's colour: leftmost
        lab = first[1]
        rest = [x for c, v in cols.items() if c != first[0] for x in v if x > min(lab)]
        if not rest:
            out.append((label, None))
            continue
        cell = (max(lab) - min(lab) + 1) / float(len(label))
        out.append((label, (min(rest) - max(lab) - 1) / cell))
    return out


def size():
    from PIL import Image
    return Image.open(start).size


def font_line():
    for line in open(INF, errors="replace"):
        if re.match(r"\s*FONT\s*=", line, re.I):
            return line.split("=", 1)[1].strip()
    return ""


skipped = False
# 1. EMail as Erez starts it: the system font; read the Hebrew message, choose a GDOS font
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
try:
    time.sleep(boot)                     # boot, log in, mirror the inbox
    start = r.shot("f0-start")
    W, H = size()
    r.click(330, 108)                    # newest message: the Hebrew one
    time.sleep(12)                       # downloaded over the serial line
    read = r.shot("f1-read")
    r.click(340, 8)                      # Options
    time.sleep(0.5)
    r.click(348, 56)                     # Font...
    time.sleep(fonts_wait)
    dlg = r.shot("f2-font-dialog")
    check("the font dialog opened", changed(read, dlg) > 0.1, "%.0f%% of the screen" % (100 * changed(read, dlg)))
    r.key(K["DOWN"])                     # the first family after the system font
    time.sleep(3)
    moved = r.shot("f3-first-gdos-font")
    if changed(dlg, moved) < 0.0005:
        print("  no GDOS fonts listed (no GDOS here): the font steps skipped", flush=True)
        skipped = True
        r.key(K["ESC"])
        time.sleep(2)
    else:
        r.key(K["RETURN"])               # OK
        time.sleep(8 if nvdi else 4)
        after = r.shot("f4-chosen")
        # the dialog's box: 64 x 23 cells of the system font, in the middle
        cw, chh = 8, (16 if H >= 400 else 8)
        dw, dh = 64 * cw, 23 * chh
        box = ((W - dw) // 2 + 8, (H - dh) // 2 + 8, (W + dw) // 2 - 8, (H + dh) // 2 - 8)
        check("no dialog left (against the dialog's own shot)", changed(moved, after, box) > 0.15,
              "%.0f%% of its box changed" % (100 * changed(moved, after, box)))
        check("the windows redrew in the new font", changed(read, after) > 0.01,
              "%.1f%% changed from before the dialog" % (100 * changed(read, after)))
        time.sleep(4)
        later = r.shot("f5-later")
        check("... and stayed so", changed(after, later) < 0.02,
              "%.1f%% changed since" % (100 * changed(after, later)))
    r.ctrl("q")                          # quit: the settings are saved
    time.sleep(8)
finally:
    r.stop()

fl = font_line()
if skipped:
    check("EMAIL.INF keeps the system font", fl.split(",")[0].strip() in ("", "1"), "FONT=" + fl)
else:
    check("EMAIL.INF says FONT= the GDOS font", bool(fl) and fl.split(",")[0].strip() not in ("", "1"),
          "FONT=" + fl)

# 2. started again in that font, offline: from what is on disk
with open(INF, newline="") as f:
    t = f.read()
t, n = re.subn(r"(?im)^(\s*OFFLINE\s*=\s*)\d+", r"\g<1>1", t)
if not n:
    t = t.replace("[options]\r\n", "[options]\r\noffline=1\r\n", 1)
with open(INF, "w", newline="") as f:
    f.write(t)
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
try:
    time.sleep(boot)
    again = r.shot("g0-again")
    W, H = size()
    # below the list's header, left of its scroll bar: three messages' lines
    lst = ink_rows(again, (220, 95, W - 24, 220))
    check("the message list drew", len(lst) >= 20, "%d rows with ink" % len(lst))
    r.click(330, 150)                    # into the list (rows may be taller now)
    time.sleep(2)
    r.key(0x47)                          # Clr/Home: the newest, the Hebrew one
    time.sleep(1)
    r.key(K["RETURN"])                   # read it
    time.sleep(6)
    heb = r.shot("g1-hebrew")
    rbox = (220, H // 2, W - 24, H - 4)     # the reader, left of its scroll bar
    check("the message drew", changed(again, heb, rbox) > 0.01,
          "%.1f%% of the reader changed" % (100 * changed(again, heb, rbox)))
    rows = ink_rows(heb, rbox)
    # the body's lines: right to left, so their ink ends near the right edge
    bw = rbox[2] - rbox[0]
    right = [y for y, (x0, x1) in rows.items() if x1 > bw * 0.85 and x0 > bw * 0.4]
    check("its Hebrew lines are right to left", len(right) >= 6,
          "%d rows of ink against the right edge only" % len(right))
    if right:
        from PIL import Image
        y0 = min(right)
        y1 = y0
        while y1 + 1 in rows:
            y1 += 1
        im = Image.open(heb).crop((rbox[0] + bw // 2, rbox[1] + y0 - 2, rbox[2], rbox[1] + y1 + 3))
        im.resize((im.size[0] * 3, im.size[1] * 3), Image.NEAREST).save(
            os.path.join(work, "shots", "g2-hebrew-line.png"))
    # the reader's header bar: the subject, and the hint at the right
    # with clear space before it; the labels clear of their values
    from PIL import Image
    mono = len(Image.open(heb).convert("RGB").getcolors(1 << 16) or []) <= 2
    bar = None if mono else header_bar(heb, 220, W - 24)
    if mono:
        print("  (a mono screen: labels and values share a colour, the header checks skipped)", flush=True)
    elif not bar:
        check("the reader's header bar found", False)
    else:
        right_end, widest, next_ = bar_gap(heb, 220, W - 24, bar[0], bar[1])
        if right_end:
            check("the hint stands clear of the subject", widest >= 1.6 * max(next_, 1),
                  "widest gap %d px, next %d px" % (widest, next_))
        else:
            print("  (the hint didn't fit: not shown)", flush=True)
        for label, g in label_gaps(heb, 220, W - 24, bar[1] + 2, bar[1] + 2 + 5 * 40,
                                   ["From:", "To:", "Date:", "Subject:"]):
            check("%s stands clear of its value" % label, g is not None and g >= 0.6,
                  "a gap of %s characters" % ("no" if g is None else "%.1f" % g))
    r.ctrl("q")
    time.sleep(4)
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)

print("FONT=" + font_line())
print("screenshots in", os.path.join(work, "shots"))
if skipped and not fails:
    print("font test: no GDOS fonts, the font steps skipped; the rest passed")
    sys.exit(0)
print("font test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
