#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""What EMail draws while the lists are used, on an emulated Falcon.

Erez's video of 20 Oct 2026 showed panes drawn again for no reason: every
row of the list at each scroll step and again when EContact's names came,
the whole folder pane when a message was chosen, the reader blank while
a message was laid out. EMAIL.PRG is the test build build/trace/EMAIL.PRG
(make build/trace/EMAIL.PRG), which says on the emulator's console what
each pane draws ("DRAW list y h", "COPY list y h"). With 40 messages on
Dovecot (as net_test.py: FAKESTNG.PRG and serial_bridge.py), it checks:

- a click on the message shown draws nothing again;
- three clicks on the list's down arrow move the rows with a copy and
  draw one row each, and nothing in the folder pane or the reader;
- the list scrolled that way looks as the same list drawn from scratch;
- another message chosen draws no more of the folder pane than its rows
  that changed (the Inbox's unread count), and the reader isn't drawn
  blank before the message.

    scroll_test.py WORKDIR [MACHINE]     (needs Xvfb, xdotool, dovecot)

AERA_FONT=5419,10 (a GDOS font's id and size, under AERA_WITH_NVDI=1)
starts EMail with that font, as Erez runs it; a row's height is taken
from what is drawn.
"""
import os
import re
import shutil
import subprocess
import sys
import time

from PIL import Image, ImageChops

import rig
from rig import K, SCAN

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
IMAP, SMTP = 10143, 10025
TRACE = os.path.join(ROOT, "build", "trace", rig.PRG)
# EMail's first layout with the system font: where the arrows and the
# rows are, by the screen's height (the Falcon's 640x480 with TOS 4's
# 23-pixel bars, ST high with TOS 1.04's 19). The list's arrow is its
# own bar's (panebar.c); the message pane's is the window's own, in the
# column beside the AES's size box (the guidelines, 5.1)
ROW = 16                                 # a list row with the system font, until one is drawn
SPOTS = {480: dict(arrow=(605, 217), list_box=(220, 98, 594, 226),
                   rarrow=(628, 445), reader_box=(220, 254, 617, 462)),
         400: dict(arrow=(611, 182), list_box=(216, 95, 602, 192),
                   rarrow=(630, 372), reader_box=(216, 217, 621, 370))}

work, machine, more = rig.cli("Falcon")
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
# 40 messages, each from someone else, one a day back from 19 Oct 2026
NAMES = ["Avi", "Dana", "Grace", "Maya", "Noam", "Tal", "Yael", "Omer", "Shira", "Eitan"]
for i in range(40):
    who = "%s %c" % (NAMES[i % 10], "ABCD"[i // 10])
    with open(os.path.join(srv, "home/dana/Maildir/new", "%02d.eml" % i), "w", newline="") as f:
        f.write("From: %s <p%02d@example.org>\r\nTo: dana@test.local\r\n"
                "Subject: Message number %d\r\nDate: %s Oct 2026 10:00:00 +0300\r\n"
                "Message-ID: <m%02d@example.org>\r\n\r\nThe text of message %d.\r\n"
                % (who, i, 40 - i, 19 - i % 19, i, 40 - i))
# and the newest, long enough for the reader to scroll
with open(os.path.join(srv, "home/dana/Maildir/new", "99.eml"), "w", newline="") as f:
    f.write("From: Ruth Long <ruth@example.org>\r\nTo: dana@test.local\r\nSubject: A long one\r\n"
            "Date: 20 Oct 2026 10:00:00 +0300\r\nMessage-ID: <long@example.org>\r\n\r\n" +
            "".join("Line %d of the long message, with words enough to fill it.\r\n" % k for k in range(1, 61)))
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
    f.write("[options]\r\ntz=180\r\nlog=1\r\n" + ("font=%s\r\n" % font if font else "") + "[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
            "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n"
            % (IMAP, SMTP))

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
    """the trace lines after line n: (what, pane, y, h)"""
    out = []
    for line in open(log, errors="replace").read().splitlines()[n:]:
        m = re.search(r"\b(DRAW|COPY) (folders|list|reader) (-?\d+) (\d+)", line)
        if m:
            out.append((m.group(1), m.group(2), int(m.group(3)), int(m.group(4))))
    return out


def shot(name):
    r.shot(name)
    return Image.open(os.path.join(shots, name + ".png")).convert("RGB")


try:
    r.settle(40, 120)                    # boot, log in, mirror the inbox
    spots = SPOTS[shot("s1-inbox").size[1]]
    ARROW_X, ARROW_Y = spots["arrow"]
    RARROW_X, RARROW_Y = spots["rarrow"]
    LIST_BOX, READER_BOX = spots["list_box"], spots["reader_box"]
    r.click(330, 108)                    # the newest message
    r.settle(10, 60)                     # downloaded over the serial line
    shot("s2-read")

    n = mark()
    r.click(330, 108)                    # the same message again
    r.settle(3, 20)
    t = since(n)
    print("again:", t)
    check("a click on the message shown draws nothing again",
          not [x for x in t if x[1] in ("reader", "folders")] and len([x for x in t if x[1] == "list"]) == 0)

    # the reader's down arrow: the long message moves a line at a time
    n = mark()
    for k in range(3):
        r.click(RARROW_X, RARROW_Y)
        time.sleep(1.5)
    r.settle(3, 20)
    t = since(n)
    print("reader:", t)
    # a row's height from what was drawn: 16 with the system font, 17
    # with a GDOS font of 10 points (the hub's run under NVDI)
    drawn = [x for x in t if x[0] == "DRAW"]
    row = drawn[0][3] if drawn else ROW
    # the reader's arrow is the window's own (5.1): the AES sends a step
    # for each click, and TOS 4's another while the button is still down,
    # so three clicks are three steps or more; each one a copy and a line
    copies = len([x for x in t if x[0] == "COPY" and x[1] == "reader"])
    check("three steps down the reader: a copy and a line (%d) drawn at each (%d steps)" % (row, copies),
          3 <= copies <= 6 and
          len([x for x in drawn if x[1] == "reader" and x[3] == row]) == copies and
          len(drawn) == copies and row < 30)
    rscrolled = shot("s2b-reader")
    r.ctrl(",")                          # Settings, OK: everything drawn from scratch
    time.sleep(3)
    r.key(K["RETURN"])
    r.settle(4, 30)
    diff = ImageChops.difference(rscrolled.crop(READER_BOX), shot("s2c-redrawn").crop(READER_BOX)).getbbox()
    check("the scrolled reader looks as drawn afresh%s" % ("" if not diff else " (differs at %s)" % (diff,)),
          diff is None)

    # the list's down arrow: the bottom of its scroll bar
    img = shot("s3-before")
    n = mark()
    for k in range(3):
        r.click(ARROW_X, ARROW_Y)
        time.sleep(1.5)
    r.settle(3, 20)
    t = since(n)
    print("scroll:", t)
    copies = [x for x in t if x[0] == "COPY" and x[1] == "list"]
    rows = [x for x in t if x[0] == "DRAW" and x[1] == "list"]
    check("three steps down the list: three copies", len(copies) == 3)
    check("and one row drawn at each (%s)" % [x[3] for x in rows],
          len(rows) == 3 and len(set(x[3] for x in rows)) == 1 and rows[0][3] < 30)
    check("nothing in the folder pane or the reader", not [x for x in t if x[1] in ("reader", "folders")])
    scrolled = shot("s4-scrolled")

    r.ctrl(",")                          # Settings, OK: everything drawn from scratch
    time.sleep(3)
    r.key(K["RETURN"])
    r.settle(4, 30)
    fresh = shot("s5-redrawn")
    box = LIST_BOX
    diff = ImageChops.difference(scrolled.crop(box), fresh.crop(box)).getbbox()
    check("the scrolled list looks as the same list drawn afresh%s" % ("" if not diff else " (differs at %s)" % (diff,)),
          diff is None)

    n = mark()
    r.click(330, 108 + row)              # another message
    r.settle(10, 60)
    shot("s6-other")
    t = since(n)
    print("other:", t)
    folders = [x for x in t if x[1] == "folders"]
    check("another message: the folder pane draws no more than a row at a time (%s)" % [x[3] for x in folders],
          all(x[3] <= row for x in folders))
    readers = [x for x in t if x[1] == "reader"]
    check("and the reader is drawn once with the message, not blank first (%d)" % len(readers), len(readers) <= 2)
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
