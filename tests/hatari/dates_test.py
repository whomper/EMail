#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The message list's short dates (decided 24 Nov 2026), read back from
the screen: an Inbox holding a message from today, one from earlier this
year and one from last year shows "HH:MM", "DD.MM" and "DD.MM.YY", newest
first, each starting in the same column, under the "Date" title.

Each message's subject is "0123456789.:" and its sender "A", so every
row carries the glyphs its date is made of: the rig learns them from the
row's own subject (the row's colours, selected or not) and reads the date
cells with them. EMail runs on the host's clock, in UTC (tz=0).

    dates_test.py WORKDIR [MACHINE]     (needs Xvfb, xdotool, dovecot)
"""
import calendar
import os
import shutil
import subprocess
import sys
import time

from PIL import Image

import rig

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
IMAP = 10143
GLYPHS = "0123456789.:"

work, machine, more = rig.cli("Falcon")
os.makedirs(work, exist_ok=True)
srv = os.path.join(work, "srv")
hd = os.path.join(work, "hd")
for d in (srv, hd):
    shutil.rmtree(d, ignore_errors=True)
run_dir = subprocess.run(["mktemp", "-d", "/tmp/tdv.XXXXXX"], capture_output=True, text=True).stdout.strip()
for d in ("state", "home/dana/Maildir/new", "home/dana/Maildir/cur", "home/dana/Maildir/tmp"):
    os.makedirs(os.path.join(srv, d))
os.chmod(work, 0o755)
os.chmod(srv, 0o755)
with open(os.path.join(srv, "users"), "w") as f:
    f.write("dana:{PLAIN}secret\n")
uid = subprocess.check_output(["id", "-u", "nobody"]).strip().decode()
gid = subprocess.check_output(["id", "-g", "nobody"]).strip().decode()
conf = open(os.path.join(ROOT, "tests", "dovecot.conf.in")).read()
for k, v in (("@RUN@", run_dir), ("@DIR@", srv), ("@IMAP@", str(IMAP)), ("@POP3@", "10110"),
             ("@UID@", uid), ("@GID@", gid)):
    conf = conf.replace(k, v)
with open(os.path.join(srv, "dovecot.conf"), "w") as f:
    f.write(conf)

# the three dates, UTC; today's a few minutes ago, but never before midnight
now = time.time()
g = time.gmtime(now)
midnight = calendar.timegm((g.tm_year, g.tm_mon, g.tm_mday, 0, 0, 0))
today = max(now - 300, midnight + 60)
year_day = (1, 15) if (g.tm_mon, g.tm_mday) != (1, 15) else (2, 15)
this_year = calendar.timegm((g.tm_year, year_day[0], year_day[1], 9, 48, 0))
older = calendar.timegm((g.tm_year - 1, 6, 5, 7, 36, 0))
want = [time.strftime("%H:%M", time.gmtime(today)),
        time.strftime("%d.%m", time.gmtime(this_year)),
        time.strftime("%d.%m.%y", time.gmtime(older))]
for k, t in enumerate((today, this_year, older)):
    msg = ("From: A <a@example.com>\r\nTo: dana@test.local\r\nSubject: %s\r\n"
           "Date: %s\r\nMessage-ID: <dates-%d@example.com>\r\nMIME-Version: 1.0\r\n"
           "Content-Type: text/plain; charset=us-ascii\r\n\r\nDate row %d.\r\n"
           % (GLYPHS, time.strftime("%a, %d %b %Y %H:%M:%S +0000", time.gmtime(t)), k, k))
    with open(os.path.join(srv, "home/dana/Maildir/new", "%d.dates%d" % (int(t), k)), "w", newline="") as f:
        f.write(msg)
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
rig.stop_at_exit(srv)

os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd)
rig.fakesting(os.path.join(hd, "AUTO"))
with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
    f.write("[options]\r\nnatfeats=1\r\ntz=0\r\nlog=1\r\n[account]\r\nname=Dana\r\nfullname=Dana\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\npass=secret\r\n"
            % IMAP)
fifo_out, fifo_in = os.path.join(work, "st_out"), os.path.join(work, "st_in")
for p in (fifo_out, fifo_in):
    if os.path.exists(p):
        os.remove(p)
    os.mkfifo(p)
bridge = rig.bridge(work, fifo_out, fifo_in)
shutil.rmtree(os.path.join(work, "shots"), ignore_errors=True)


def cells(img, y, x0, x1):
    """the 8-pixel cells of the text row at y (16 lines): each a mask of
    the pixels that differ from the row's background, its commonest colour"""
    px = img.load()
    hist = {}
    for x in range(x0, x1):
        for yy in (y, y + 15):
            hist[px[x, yy]] = hist.get(px[x, yy], 0) + 1
    bg = max(hist, key=hist.get)
    out = []
    for cx in range(x0, x1 - 7, 8):
        out.append(tuple(tuple(px[cx + i, y + j] != bg for i in range(8)) for j in range(16)))
    return out


def blank(c):
    return not any(any(r) for r in c)


def read_row(img, y, x0, x1):
    """(the date's text, its first cell's x) for the list row at y"""
    cs = cells(img, y, x0, x1)
    for s in range(len(cs) - len(GLYPHS)):
        run = cs[s:s + len(GLYPHS)]
        if all(not blank(c) for c in run) and len(set(run)) == len(GLYPHS) and \
                (s == 0 or blank(cs[s - 1])) and blank(cs[s + len(GLYPHS)]):
            glyph = dict(zip(run, GLYPHS))
            k = s + len(GLYPHS)
            while k < len(cs) and blank(cs[k]):
                k += 1
            start, text = k, ""
            while k < len(cs) and not blank(cs[k]):
                text += glyph.get(cs[k], "?")
                k += 1
            return text, x0 + 8 * start
    return None, None


r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
ok = True
try:
    time.sleep(40)                       # boot, log in, mirror the inbox
    r.click(330, 108)                    # the first row: selected, so the rows can be found
    time.sleep(10)
    img = Image.open(r.shot("d1-list")).convert("RGB")
    W, H = img.size
    # the selected row: the lines between y 80 and 140 that are mostly one non-white colour
    px = img.load()
    sel = []
    for y in range(80, 140):
        row = [px[x, y] for x in range(250, 580)]
        top = max(set(row), key=row.count)
        if top != (255, 255, 255) and row.count(top) > 0.5 * len(row):
            sel.append(y)
    y0 = sel[0] if sel else None
    print("first row at y", y0, "screen", W, "x", H)
    if y0 is None:
        print("FAIL: no selected row found")
        ok = False
    else:
        got, starts = [], []
        for k in range(3):
            text, x = read_row(img, y0 + 16 * k, 240, W - 48)
            got.append(text)
            starts.append(x)
            print("row %d: date %r at x %s, want %r" % (k, text, x, want[k]))
        ok = got == want
        print("dates, newest first, each its form:", ok)
        same = None not in starts and len(set(starts)) == 1
        print("each date in the same column:", same)
        ok = ok and same
        if same:
            # the "Date" title over the column: ink in the title band at its first cell,
            # none in the cell before
            x = starts[0]
            band = [(px[xx, yy] != px[x - 9, y0 - 20]) for yy in range(y0 - 20, y0 - 3) for xx in range(x, x + 8)]
            before = [(px[xx, yy] != px[x - 9, y0 - 20]) for yy in range(y0 - 20, y0 - 3) for xx in range(x - 8, x)]
            title = any(band) and not any(before)
            print("the Date title over the column:", title)
            ok = ok and title
finally:
    r.stop()
print("dates test:", "PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
