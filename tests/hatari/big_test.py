#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A large message on a 1 MB ST (a tester's fault in 2.25: one large
message gave a memory error; decided 21 Nov 2026: above Settings' limit, 500
KB, it comes without its attachments, which stay on the server).

Dovecot holds a short message and, newest, one of about 800 KB: a text and
a PDF. EMail (ST-1, TOS 1.04 by default) reads the inbox and opens the
large one: it comes without the PDF (EMAIL.LOG: BODYSTRUCTURE and the
sections asked for, never the whole message), its text shown, no memory
error. A click on the PDF's line, Save to disk, Return in the file
selector: the PDF downloaded from the server into C:\\EMAIL, a piece at a
time, the same bytes as were sent. No CRASH.LOG.

    big_test.py WORKDIR [MACHINE]     (needs Xvfb, xdotool, dovecot)
"""
import base64
import os
import random
import shutil
import subprocess
import sys
import time

import rig
from rig import K  # noqa: F401  (the keys, kept for edits)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
IMAP, SMTP = 10143, 10025

work, machine, more = rig.cli("ST-1:mono")
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

# the PDF: 800 KB the same every run (more than a 1 MB ST has free)
random.seed(2026)
pdf = bytes(random.getrandbits(8) for _ in range(800 * 1024))
new = os.path.join(srv, "home/dana/Maildir/new")
with open(os.path.join(new, "1-small.eml"), "w", newline="") as f:
    f.write("From: Avi <avi@test.local>\r\nTo: dana@test.local\r\nSubject: Small one\r\n"
            "Date: Thu, 20 Nov 2026 10:01:00 +0200\r\n\r\nA short note.\r\n")
with open(os.path.join(new, "2-big.eml"), "w", newline="") as f:
    f.write("From: Ruth <ruth@test.local>\r\nTo: dana@test.local\r\nSubject: Big drawings\r\n"
            "Date: Thu, 20 Nov 2026 10:02:00 +0200\r\nMIME-Version: 1.0\r\n"
            "Content-Type: multipart/mixed; boundary=\"outer\"\r\n\r\n"
            "--outer\r\nContent-Type: text/plain; charset=\"us-ascii\"\r\n\r\n"
            "The drawings for the Falcon case.\r\n"
            "--outer\r\nContent-Type: application/pdf; name=\"case.pdf\"\r\n"
            "Content-Disposition: attachment; filename=\"case.pdf\"\r\nContent-Transfer-Encoding: base64\r\n\r\n"
            + base64.encodebytes(pdf).decode().replace("\n", "\r\n") + "\r\n--outer--\r\n")
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
rig.stop_at_exit(srv)

os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd, os.environ.get("BIG_PRG"))   # BIG_PRG: another build (the last release, to see it fail)
rig.fakesting(os.path.join(hd, "AUTO"))
with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
    f.write("[options]\r\nnatfeats=1\r\ntz=180\r\nlog=1\r\n[account]\r\nname=Dana\r\nfullname=Dana\r\n"
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
checks = []


def check(what, good):
    checks.append(good)
    print("  %s   %s" % ("ok" if good else "FAIL", what), flush=True)


def log():
    try:
        return open(rig.ci_path(hd, "email.log"), errors="replace").read()
    except OSError:
        return ""


try:
    time.sleep(45)                       # boot, log in, mirror the inbox
    r.shot("b1-inbox")
    r.click(330, 108)                    # the newest: the large one
    r.settle(15, 120)
    r.shot("b2-read")
    l = log()
    check("its parts asked for (BODYSTRUCTURE)", "BODYSTRUCTURE" in l)
    check("its header and text asked for, not all of it", "BODY.PEEK[HEADER]" in l and "BODY.PEEK[])" not in l)
    # 2.25 put up "Dana: not enough memory for this message" here (an
    # alert, not in EMAIL.LOG) and the message never opened: what shows the
    # fault gone is its PDF's line to click, below, and the PDF saved from
    # it (EMail writes its cache later, so the cache can't show it here)
    open(os.path.join(work, "email-read.log"), "w").write(l)
    # the PDF's line: under the note, the text and the rule; its click
    # asks Cancel | Save to disk | Open with EPDF; Save to disk, then
    # Return in the file selector (C:\EMAIL\, CASE.PDF)
    # the rows sit lower on a 480-line screen (the reference configuration), and
    # the alert is centred on the screen
    from PIL import Image
    tall = Image.open(os.path.join(shots, "b2-read.png")).height
    down = 42 if tall >= 480 else 0
    r.click(280, 283 + down)
    time.sleep(3)
    r.shot("b3-pdf-alert")
    r.click(315, 233 + (tall - 400) // 2)   # Save to disk
    time.sleep(4)
    r.shot("b4-fsel")
    # Save in the family's file dialog (guidelines 5.12): its first row is
    # chosen when it opens, and Return there would open that folder
    r.click(440, 401 - (480 - tall) // 2)
    # downloaded over the serial line: until the file stops growing and
    # EMail is idle again (up to 20 minutes on an 8 MHz ST)
    saved = os.path.join(hd, "EMAIL", "CASE.PDF")
    last, still, t0 = -1, 0, time.time()
    while time.time() - t0 < 1200:
        time.sleep(10)
        found = [os.path.join(dp, f) for dp, _, fs in os.walk(hd) for f in fs if f.lower() == "case.pdf"]
        if found:
            saved = found[0]
        size = os.path.getsize(saved) if os.path.exists(saved) else -1
        still = still + 1 if size == last and size >= 0 else 0
        last = size
        if still >= 3:
            break
    print("saved: %s, %d bytes in %d s" % (saved, last, time.time() - t0))
    r.shot("b5-saved")
    got = open(saved, "rb").read() if os.path.exists(saved) else b""
    check("the PDF saved from the server, the bytes that were sent (%d of %d)" % (len(got), len(pdf)), got == pdf)
    l = log()
    check("... downloaded as its own section, not the whole message",
          "BODY.PEEK[2]" in l and "BODY.PEEK[])" not in l)
    check("no CRASH.LOG", not any(n.lower() == "crash.log" for _, _, fs in os.walk(hd) for n in fs))
finally:
    r.stop()
    bridge.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
