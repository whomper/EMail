#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Tooltips over text cut short (Erez, 27 Oct 2026), as net_test.py:
Dovecot, FAKESTNG.PRG over the serial port, the test build
(build/trace/EMAIL.PRG says each tooltip on the console: "TIP text").
The mouse rests on the list's From and Subject of a message whose name
and subject don't fit (the tooltip shows them whole), on a name that
fits (none), and down the folder pane, where one folder's name is longer
than the pane (its tooltip, none for the others). Screenshots of each.

    tip_test.py WORKDIR [MACHINE]   (make build/trace/EMAIL.PRG first)
"""
import email
import email.header
import imaplib
import os
import shutil
import subprocess
import sys
import time

import rig
from rig import K, SCAN

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
IMAP, SMTP = 10143, 10025
LONG = "Projects and correspondence of 2026"

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
uid, gid = subprocess.check_output(["id", "-u", "nobody"]).strip(), subprocess.check_output(["id", "-g", "nobody"]).strip()
conf = open(os.path.join(ROOT, "tests", "dovecot.conf.in")).read()
for k, v in (("@RUN@", run_dir), ("@DIR@", srv), ("@IMAP@", str(IMAP)), ("@POP3@", "10110"),
             ("@UID@", uid.decode()), ("@GID@", gid.decode())):
    conf = conf.replace(k, v)
with open(os.path.join(srv, "dovecot.conf"), "w") as f:
    f.write(conf)
for m in sorted(os.listdir(os.path.join(ROOT, "tests", "mail"))):
    shutil.copy(os.path.join(ROOT, "tests", "mail", m), os.path.join(srv, "home/dana/Maildir/new", m))
# a folder whose name the folder pane cuts short
for d in ("cur", "new", "tmp"):
    os.makedirs(os.path.join(srv, "home/dana/Maildir", "." + LONG, d))
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

# drive C:
os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd, os.path.join(ROOT, "build", "trace", "EMAIL.PRG"))
rig.fakesting(os.path.join(hd, "AUTO"))
with open(os.path.join(hd, "EMAIL.INF"), "w", newline="") as f:
    f.write("[options]\r\ntz=180\r\nlog=1\r\nsignature=Sent from my Falcon\r\n[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
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
checks = []


def check(what, good):
    checks.append(good)
    print("  %s   %s" % ("ok" if good else "FAIL", what))




def tips():
    log = open(os.path.join(work, "hatari.log"), errors="replace").read()
    return [l[4:] for l in log.splitlines() if l.startswith("TIP ")]


def rest(x, y, name):
    """the mouse moved there and left still: the tooltip comes after 0.7 s"""
    n = len(tips())
    r.move(x - 3, y)
    r.move(x, y)
    time.sleep(2)
    r.shot(name)
    new = tips()[n:]
    r.move(424, 64)                      # away, onto the pane's title where nothing
    time.sleep(0.6)                      # is cut (not a list row: its own tooltip)
    return new


try:
    r.settle(40, 240)
    got = rest(400, 140, "t1-subject")     # the third row: "Newsletter October ..."
    check("a subject cut short: its tooltip, all of it", any(t.startswith("Newsletter") and len(t) > 13
                                                          for t in got))
    got = rest(296, 140, "t2-from")        # its From: "Atari Newsletter ..."
    check("a name cut short: its tooltip", any(t.startswith("Atari") and len(t) > 8 for t in got))
    got = rest(424, 64, "t3-title")        # the pane's title: nothing cut
    check("where nothing is cut: no tooltip", got == [])
    found = []
    for y in range(104, 232, 16):          # down the folder pane
        found += rest(60, y, "t4-folder-%d" % y)
    check("a folder's name cut short: its tooltip", LONG in found)
    check("... and none for the folders that fit", all(t == LONG for t in found))
    print("tooltips:", tips())
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
print("PASS" if all(checks) else "FAIL")
sys.exit(0 if all(checks) else 1)
