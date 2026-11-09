#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Options > Translate and the reader's "Translate..." (Erez, 7 Nov 2026),
with the Gateway's translategate (gateway/translategate.py, the released
copy) and its stand-in engine (it answers "en: TEXT IN CAPITALS") on port 8480,
as net_test.py otherwise (Dovecot, FAKESTNG.PRG over the serial port), on
an emulated Falcon with the test build (build/trace/email.prg says
"TRANSLATE ..." on the console each time the Gateway is asked).

1. Translate off (as EMail starts with no TRANSLATE= in EMAIL.INF):
   messages read (the Hebrew one and two more), Settings opened and left,
   a word chosen in the reader. Nothing may reach port 8480: the serial
   bridge's log names every connection the Atari opens, and the console
   has no TRANSLATE line (the code was never run).
2. Translate on (TRANSLATE=1): the Hebrew message read is sent to the
   Gateway, and the reader shows something else than it did in 1.
3. A word chosen (a double click), the right-click's "Translate...": the
   words go to the Gateway on their own, a box shows the answer.
4. Into Hebrew (TRANSLATETO=he), against common's stand-in translategate
   (common/tests/langs/fakegate.py: it answers in Hebrew, with a Latin run
   and a number in it): the English newsletter's translation is drawn right
   to left, set against the reader's right edge, and so is Translate...'s
   box (Erez, 8 Nov 2026).
5. Into a language the Gateway hasn't installed (German): its 422 gives
   the alert that names Settings > Languages ("TRANSLATE not installed"
   on the console).

    translate_test.py WORKDIR [MACHINE]   (make build/trace/email.prg first)

TRANSLATEGATE names another translategate.py to run.
"""
import os
import shutil
import subprocess
import sys
import time

from PIL import Image, ImageChops

import rig
from rig import K

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
IMAP, SMTP, TRANSLATE = 10143, 10025, 8480

work, machine, more = rig.cli("Falcon")
if not machine.startswith("Falcon"):
    sys.exit("translate_test.py: the clicks are placed for the Falcon's 640x480")
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
for m in sorted(os.listdir(os.path.join(ROOT, "tests", "mail"))):
    shutil.copy(os.path.join(ROOT, "tests", "mail", m), os.path.join(srv, "home/dana/Maildir/new", m))
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

# translategate as released (gateway/, AERA-Gateway's copy), with its stand-in engine
gate = os.environ.get("TRANSLATEGATE") or os.path.join(ROOT, "gateway", "translategate.py")
gate_log = os.path.join(work, "translategate.log")
tg = subprocess.Popen([sys.executable, gate, "--port", str(TRANSLATE), "--bind", "127.0.0.1"],
                      stdout=open(gate_log, "w"), stderr=subprocess.STDOUT)

# drive C:
os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd, os.path.join(ROOT, "build", "trace", "email.prg"))
rig.fakesting(os.path.join(hd, "AUTO"))


def inf(translate, to="en"):
    with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
        f.write("[options]\r\ntz=180\r\nlog=1\r\n%s[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
                "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
                "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n"
                % ("translate=1\r\ntranslateto=%s\r\n" % to if translate else "", IMAP, SMTP))


def right_set(im):
    """the text lines of im (a crop) drawn against its right edge: runs of 6
    or more pixel rows whose ink reaches the last 32 pixels and starts past
    the first third (wrapped lines touch, so it goes by pixel rows); the
    background is the crop's commonest colour"""
    im = im.convert("RGB")
    w, h = im.size
    px = im.load()
    bg = max(im.getcolors(w * h), key=lambda c: c[0])[1]
    lines = run = 0
    for y in range(h + 1):
        xs = [x for x in range(w) if px[x, y] != bg] if y < h else []
        if xs and min(xs) >= w // 3 and max(xs) >= w - 32:
            run += 1
            continue
        if run >= 6:
            lines += 1
        run = 0
    return lines


fails = []


def check(what, good, detail=""):
    print("  %s %s%s" % ("ok  " if good else "FAIL", what, ("  (" + detail + ")") if detail else ""), flush=True)
    if not good:
        fails.append(what)


def to_gateway():
    """the connections the Atari opened to translategate's port"""
    log = open(os.path.join(work, "bridge.log"), errors="replace").read()
    return [l for l in log.splitlines() if ":%d" % TRANSLATE in l and "open" in l]


def traced():
    log = open(os.path.join(work, "hatari.log"), errors="replace").read()
    return [l for l in log.splitlines() if l.startswith("TRANSLATE")]


def start():
    fifo_out, fifo_in = os.path.join(work, "st_out"), os.path.join(work, "st_in")
    for p in (fifo_out, fifo_in):
        if os.path.exists(p):
            os.remove(p)
        os.mkfifo(p)
    br = rig.bridge(work, fifo_out, fifo_in)
    return br, rig.Rig(work, machine, serial=(fifo_out, fifo_in))


def reader(shot):
    """the reader pane's part of a screenshot (below the message list)"""
    im = Image.open(shot).convert("RGB")
    w, h = im.size
    return im.crop((236, 256, w - 24, h - 24))


WORD = (296, 262)               # a word of the reader's first header line (From:)
try:
    # 1. Translate off
    inf(False)
    bridge, r = start()
    try:
        time.sleep(40)                   # boot, log in, mirror the inbox
        r.click(330, 108)                # the newest message: the Hebrew one
        time.sleep(12)
        off_shot = r.shot("t1-off-read")
        r.key(K["DOWN"])
        time.sleep(8)
        r.key(K["DOWN"])
        time.sleep(8)
        r.key(rig.SCAN[","], ctrl=True)  # Settings, and left
        time.sleep(4)
        r.shot("t2-off-settings")
        r.key(0x61)                      # Undo
        time.sleep(3)
        r.click(330, 108)                # back to the Hebrew one
        time.sleep(8)
        r.click(*WORD, double=True)      # a word chosen: still nothing sent
        time.sleep(2)
        r.shot("t3-off-chosen")
    finally:
        r.stop()
        bridge.terminate()
    check("Translate off: no connection to the Gateway's port %d" % TRANSLATE, to_gateway() == [],
          "; ".join(to_gateway()))
    check("... and the translation code never ran (no TRANSLATE on the console)", traced() == [],
          "; ".join(traced()))

    # 2. Translate on
    inf(True)
    bridge, r = start()
    try:
        time.sleep(40)
        r.click(330, 108)                # the Hebrew one, sent whole to the Gateway
        time.sleep(30)
        on_shot = r.shot("t4-on-read")
        sent = to_gateway()
        check("Translate on: the message went to the Gateway", len(sent) >= 1, "%d connections" % len(sent))
        check("... the console says so", "TRANSLATE message" in traced())
        check("... and the reader shows its translation, not the original",
              ImageChops.difference(reader(off_shot), reader(on_shot)).getbbox() is not None)
        # 3. words chosen, the right-click's Translate...
        n = len(to_gateway())
        r.click(*WORD, double=True)
        time.sleep(2)
        chosen = r.shot("t5-chosen")
        r.rclick(*WORD)
        time.sleep(2)
        menu = r.shot("t5-menu")
        check("the right-click's menu opened", rig.Rig.clicked(chosen, menu))
        r.click(WORD[0] + 20, WORD[1] + 24)   # the menu's second row: Translate...
        time.sleep(10)
        box = r.shot("t6-box")
        check("Translate...: the words went to the Gateway on their own", len(to_gateway()) > n,
              "%d -> %d" % (n, len(to_gateway())))
        check("... the console says so", "TRANSLATE words" in traced())
        check("... its box is on the screen", ImageChops.difference(
            Image.open(on_shot).convert("RGB"), Image.open(box).convert("RGB")).getbbox() is not None)
        r.key(K["RETURN"])               # the box's OK
        time.sleep(3)
        r.shot("t7-closed")
    finally:
        r.stop()
        bridge.terminate()
    print("translategate's log:", open(gate_log, errors="replace").read().strip().replace("\n", " | ")[:600])

    # 4. into Hebrew, against common's stand-in, which answers in Hebrew
    tg.terminate()
    tg.wait()
    sys.path.insert(0, os.path.join(ROOT, "common", "tests", "langs"))
    import fakegate
    fg = fakegate.Gate(port=TRANSLATE, installed=("he",))
    fg.start()
    try:
        inf(True, "he")
        bridge, r = start()
        try:
            time.sleep(40)
            r.click(330, 140)                # the third: the English newsletter
            time.sleep(30)
            he_shot = r.shot("t8-he-read")
            n = right_set(reader(he_shot))
            check("into Hebrew: the reader's translation is set against the right edge", n >= 1,
                  "%d lines" % n)
            r.click(*WORD, double=True)      # a word of From: ("Atari News")
            time.sleep(2)
            chosen = r.shot("t9-he-chosen")
            r.rclick(*WORD)
            time.sleep(2)
            r.click(WORD[0] + 20, WORD[1] + 24)
            time.sleep(10)
            box_shot = r.shot("t10-he-box")
            d = ImageChops.difference(Image.open(chosen).convert("RGB"),
                                      Image.open(box_shot).convert("RGB")).getbbox()
            n = right_set(Image.open(box_shot).crop((d[0] + 8, d[1] + 40, d[2] - 8, d[3] - 40))) if d else 0
            check("... and Translate...'s box sets it right to left, at the right edge", n >= 1,
                  "%d lines, box %s" % (n, d))
            r.key(K["RETURN"])
            time.sleep(3)
        finally:
            r.stop()
            bridge.terminate()
        # 5. a language that isn't installed
        inf(True, "de")
        bridge, r = start()
        try:
            time.sleep(40)
            r.click(330, 140)
            time.sleep(30)
            r.shot("t11-de-not-installed")
            r.key(K["RETURN"])
            time.sleep(3)
        finally:
            r.stop()
            bridge.terminate()
        check("into German, not installed: the alert that names Settings > Languages",
              "TRANSLATE not installed" in traced(), "; ".join(traced()))
        print("the stand-in was asked:", [(m, p) for m, p, _ in fg.log])
    finally:
        fg.stop()
finally:
    tg.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
print("translate test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
