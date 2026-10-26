#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""How much EMail draws, and how long it takes, on a Falcon as sold.

Erez's videos of 26 Oct 2026, a stock Falcon (16 MHz, no DFB1X): one
click on the message pane's scroll bar turned two pages; deleting a
message from the right-click menu drew the three panes again and again;
every page of a message took about a second to draw. EMAIL.PRG is the
test build build/trace/EMAIL.PRG, which says on the emulator's console
what each pane drew and how long it took ("DRAW reader 0 400 ... ms
840"), each new top row ("TOP reader 24") and how long showing a message
took ("SHOW fetch .. parse .. body .. layout .."). As scroll_test.py
(Dovecot, FAKESTNG.PRG over the serial port), with 40 short messages and
a long one; on the Falcon-4 unless another machine is named:

1. the long message shown: how long its steps and its drawing took;
2. one click (pressed 0.12 s, as a hand does) on the message pane's
   scroll bar under the slider: one page, not two;
3. Delete from the right-click menu: what each pane drew, how often and
   for how long;
4. Delete by the key, on another message: the same.

    redraw_test.py WORKDIR [MACHINE]     (needs Xvfb, xdotool, dovecot)

AERA_FONT=5419,10 (a GDOS font's id and size, under AERA_WITH_NVDI=1)
starts EMail with that font, as Erez runs it.
"""
import os
import re
import shutil
import subprocess
import sys
import time

from PIL import Image

import rig

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
IMAP, SMTP = 10143, 10025
TRACE = os.path.join(ROOT, "build", "trace", rig.PRG)
# EMail's first layout with the system font, by the screen's height (as
# scroll_test.py): the message pane's scroll bar is the window's own
SPOTS = {480: dict(track=(628, 420), first=(330, 108)),
         400: dict(track=(630, 350), first=(330, 105))}

work, machine, more = rig.cli("Falcon-4")
if not os.path.exists(TRACE):
    sys.exit("first: make build/trace/%s" % rig.PRG)
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
uid, gid = subprocess.check_output(["id", "-u", "nobody"]).strip(), subprocess.check_output(["id", "-g", "nobody"]).strip()
conf = open(os.path.join(ROOT, "tests", "dovecot.conf.in")).read()
for k, v in (("@RUN@", run_dir), ("@DIR@", srv), ("@IMAP@", str(IMAP)), ("@POP3@", "10110"),
             ("@UID@", uid.decode()), ("@GID@", gid.decode())):
    conf = conf.replace(k, v)
with open(os.path.join(srv, "dovecot.conf"), "w") as f:
    f.write(conf)
NAMES = ["Avi", "Dana", "Grace", "Maya", "Noam", "Tal", "Yael", "Omer", "Shira", "Eitan"]
for i in range(40):
    who = "%s %c" % (NAMES[i % 10], "ABCD"[i // 10])
    with open(os.path.join(srv, "home/dana/Maildir/new", "%02d.eml" % i), "w", newline="") as f:
        f.write("From: %s <p%02d@example.org>\r\nTo: dana@test.local\r\n"
                "Subject: Message number %d\r\nDate: %s Oct 2026 10:00:00 +0300\r\n"
                "Message-ID: <m%02d@example.org>\r\n\r\nThe text of message %d.\r\n"
                % (who, i, 40 - i, 19 - i % 19, i, 40 - i))
# the newest: a long letter, its lines as long as a mail's paragraphs wrap to
with open(os.path.join(srv, "home/dana/Maildir/new", "99.eml"), "w", newline="") as f:
    f.write("From: Ruth Long <ruth@example.org>\r\nTo: dana@test.local\r\nSubject: A long one\r\n"
            "Date: 20 Oct 2026 10:00:00 +0300\r\nMessage-ID: <long@example.org>\r\n\r\n" +
            "".join("Paragraph %d. Search Console has identified that some pages on your site are not "
                    "being indexed due to the following new reason, and here is what to do about it.\r\n\r\n"
                    % k for k in range(1, 41)))
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd, TRACE)
rig.fakesting(os.path.join(hd, "AUTO"))
with open(os.path.join(hd, "EMAIL.INF"), "w", newline="") as f:
    font = os.environ.get("AERA_FONT")
    f.write("[options]\r\ntz=180\r\nlog=1\r\n" + ("font=%s\r\n" % font if font else "") +
            "[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
            "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n" % (IMAP, SMTP))

fifo_out, fifo_in = os.path.join(work, "st_out"), os.path.join(work, "st_in")
for p in (fifo_out, fifo_in):
    if os.path.exists(p):
        os.remove(p)
    os.mkfifo(p)
bridge = rig.bridge(work, fifo_out, fifo_in)
shots = os.path.join(work, "shots")
shutil.rmtree(shots, ignore_errors=True)
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
log = os.path.join(work, "hatari.log")
checks = []


def check(what, good):
    checks.append(good)
    print("  %s   %s" % ("ok" if good else "FAIL", what), flush=True)


def mark():
    return len(open(log, errors="replace").read().splitlines())


def since(n):
    """the trace lines after line n"""
    draws, tops, shows = [], [], []
    for line in open(log, errors="replace").read().splitlines()[n:]:
        m = re.search(r"\b(DRAW|COPY) (folders|list|reader|window) (-?\d+) (\d+) heap \d+ \d+ ms (\d+)", line)
        if m:
            draws.append((m.group(1), m.group(2), int(m.group(3)), int(m.group(4)), int(m.group(5))))
        m = re.search(r"\bTOP (\w+) (\d+)", line)
        if m:
            tops.append((m.group(1), int(m.group(2))))
        m = re.search(r"\bSHOW fetch (\d+) parse (\d+) body (\d+) layout (\d+) lines (\d+)", line)
        if m:
            shows.append(tuple(int(x) for x in m.groups()))
    return draws, tops, shows


def summary(what, draws):
    """per pane: how many times drawn, the rows' height drawn, the ms"""
    out = {}
    for kind, pane, y, h, ms in draws:
        if kind != "DRAW":
            continue
        n, hh, t = out.get(pane, (0, 0, 0))
        out[pane] = (n + 1, hh + h, t + ms)
    print("%s: %s" % (what, ", ".join("%s %d times, %d px, %d ms" % (p, n, hh, t)
                                      for p, (n, hh, t) in sorted(out.items())) or "nothing"), flush=True)
    return out


PROFILE = os.environ.get("PROFILE")    # a symbol file (nm's form) for Hatari's profiler


def profile(on, what=""):
    """Hatari's CPU profile of a step: where its time went, by function"""
    if not PROFILE:
        return
    if on:
        r.cmd("hatari-debug", "symbols", PROFILE, "TEXT")
        r.cmd("hatari-debug", "profile", "on")
        return
    print("PROFILE %s" % what, flush=True)
    r.cmd("hatari-shortcut", "debug")    # the profile is counted as the debugger is entered
    time.sleep(2)
    r.cmd("hatari-debug", "profile", "symbols", "30")
    time.sleep(2)
    r.cmd("hatari-debug", "profile", "off")


def once(draws, what):
    """a Delete draws the list and the reader once each, and of the folder
    pane no more than the rows whose counts changed. The tallest drawing
    of a pane is the whole of it; smaller ones, the area under the menu
    as it closes, are counted apart (none when the menu puts back what
    it covered)"""
    for pane in ("list", "reader"):
        all_ = [x for x in draws if x[0] == "DRAW" and x[1] == pane]
        tall = max([x[3] for x in all_] or [0])
        under = [x for x in all_ if x[3] < tall]
        whole = len(all_) - len(under)
        check("%s: the %s drawn once (%d%s)" % (what, pane, whole,
                                               ", and %d times under the menu" % len(under) if under else ""),
              whole == 1)
    f = sum(x[3] for x in draws if x[0] == "DRAW" and x[1] == "folders")
    check("%s: no more of the folder pane than its rows that changed (%d px)" % (what, f), f <= 2 * 20)


def hand_click(x, y):
    """a click as a hand makes it: the button down 0.12 s"""
    r.move(x, y)
    r.xdo("mousedown", "1")
    time.sleep(0.12)
    r.xdo("mouseup", "1")


def shot(name):
    r.shot(name)
    return Image.open(os.path.join(shots, name + ".png")).convert("RGB")


try:
    r.settle(60, 240)                    # boot, log in, mirror the inbox
    spots = SPOTS[shot("r1-inbox").size[1]]
    FX, FY = spots["first"]

    # 1. the long message
    n = mark()
    profile(1)
    r.click(FX, FY)
    r.settle(10, 120)
    profile(0, "show")
    shot("r2-read")
    draws, tops, shows = since(n)
    print("show:", shows, flush=True)
    rd = summary("the long message shown", draws)
    page_ms = max([d[4] for d in draws if d[1] == "reader" and d[0] == "DRAW"] or [0])
    print("METRIC show_ms %d page_ms %d" % (sum(shows[-1][:4]) if shows else -1, page_ms), flush=True)

    # 2. one click on the scroll bar under the slider: one page
    n = mark()
    hand_click(*spots["track"])
    r.settle(6, 60)
    shot("r3-paged")
    draws, tops, shows = since(n)
    summary("one click on the scroll bar", draws)
    rt = [t for p, t in tops if p == "reader"]
    print("reader tops:", rt, flush=True)
    check("one click on the scroll bar turns one page (%s)" % rt, len(rt) == 1)

    # 3. Delete from the right-click menu, on the message shown
    n = mark()
    r.rclick(FX, FY)
    time.sleep(2)
    shot("r4-menu")
    profile(1)
    hand_click(FX + 30, FY + 191)        # the menu's last row, Delete
    r.settle(8, 90)
    profile(0, "delete from the menu")
    shot("r5-deleted")
    draws, tops, shows = since(n)
    summary("Delete from the menu", draws)
    once(draws, "Delete from the menu")

    # 4. Delete by the key, on another message
    r.click(FX, FY)
    r.settle(6, 60)
    n = mark()
    profile(1)
    r.key(0x53)                          # Delete
    r.settle(8, 90)
    profile(0, "delete by the key")
    shot("r6-deleted-key")
    draws, tops, shows = since(n)
    summary("Delete by the key", draws)
    once(draws, "Delete by the key")
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
