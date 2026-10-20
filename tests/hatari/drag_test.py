#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Dragging messages onto a folder (as net_test.py: Dovecot, FAKESTNG.PRG
over the serial port): Avi's message dragged onto Junk moves there; the
newsletter dragged with Esc pressed on the way, and dropped on the Inbox
it is in, stays. The test checks the folders on the server and leaves
screenshots of the drag (the outline, the folder lit).

    drag_test.py WORKDIR [MACHINE]
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
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

# drive C:
os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd)
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


def folder(name):
    m = imaplib.IMAP4("127.0.0.1", IMAP)
    m.login("dana", "secret")
    m.select(name)
    typ, data = m.search(None, "ALL")
    subjects = []
    for num in data[0].split():
        typ, msg = m.fetch(num, "(RFC822.HEADER)")
        e = email.message_from_bytes(msg[0][1])
        subjects.append(str(email.header.make_header(email.header.decode_header(e["Subject"]))))
    m.logout()
    return subjects


try:
    time.sleep(40)                       # boot, log in, mirror the inbox
    r.shot("d1-inbox")
    # Avi's message onto Junk, slowly, with a look on the way
    r.move(330, 124)
    r.xdo("mousedown", "1")
    time.sleep(0.2)
    r.move(150, 150, 0.8)
    r.move(40, 169, 0.8)
    time.sleep(0.5)
    r.shot("d2-dragging")
    r.xdo("mouseup", "1")
    time.sleep(8)                        # moved on the server
    r.shot("d3-moved")
    # the newsletter: Esc on the way, then let go over Junk
    r.move(330, 124)
    r.xdo("mousedown", "1")
    time.sleep(0.2)
    r.move(40, 169, 0.8)
    r.key(K["ESC"])
    time.sleep(0.5)
    r.xdo("mouseup", "1")
    time.sleep(2)
    # and dropped on the Inbox it is in
    r.drag(330, 124, 40, 105)
    time.sleep(3)
    r.shot("d4-stayed")
    inbox, junk = folder("INBOX"), folder("Junk")
    print("INBOX:", inbox)
    print("Junk:", junk)
    check("Avi's message moved to Junk", "old style" in " ".join(junk) or any("מכתב" in s for s in junk))
    check("and is no longer in the Inbox", not any("old style" in s for s in inbox))
    check("Esc stopped the second drag", any("Newsletter" in s for s in inbox))
    check("a drop on its own folder did nothing", len(inbox) == 2)
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
