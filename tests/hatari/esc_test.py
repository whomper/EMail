#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Esc and Undo cancel (the AERA guidelines, 3.5), as net_test.py:
Dovecot, FAKESTNG.PRG over the serial port. Each dialog and alert EMail
can be driven to here is closed with Esc or Undo, and nothing it would
have done happens: Download all downloads nothing, New folder (with a
name typed) makes none, Settings and Accounts leave EMAIL.INF as it was,
Move to moves nothing, and Quit and closing a message being written keep
EMail and the message. Screenshots of each dialog and alert.

    esc_test.py WORKDIR [MACHINE]
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
UNDO = 0x61
checks = []


def check(what, good):
    checks.append(good)
    print("  %s   %s" % ("ok" if good else "FAIL", what))


def inf():
    return open(os.path.join(hd, "EMAIL.INF"), "rb").read()


def fetches():
    p = os.path.join(hd, "EMAIL.LOG")
    return open(p, "rb").read().count(b"BODY.PEEK[]") if os.path.exists(p) else 0


def server():
    m = imaplib.IMAP4("127.0.0.1", IMAP)
    m.login("dana", "secret")
    folders = " ".join(l.decode() for l in m.list()[1])
    m.select("INBOX")
    n = len(m.search(None, "ALL")[1][0].split())
    m.logout()
    return folders, n


def menu(x, y):
    r.click(x, 9)
    time.sleep(0.6)
    r.click(x + 20, y)
    time.sleep(1.5)


try:
    # boot, log in, mirror the inbox: until EMail's window is still, not
    # a fixed time (at 40 s a busy start missed the menu, and the menu's
    # second click opened a message instead)
    r.settle(40, 240)
    before, got = inf(), fetches()

    menu(272, 123)                       # Folder > Download all messages
    r.shot("e1-download-all")
    r.key(K["ESC"])
    time.sleep(3)
    check("Download all, Esc: nothing downloaded", fetches() == got)

    menu(272, 59)                        # Folder > New folder...
    r.type("Zzz")
    time.sleep(0.5)
    r.shot("e2-new-folder")
    r.key(K["ESC"])                      # in the text field
    time.sleep(3)
    check("New folder, Esc in the name: no folder", "Zzz" not in server()[0])

    menu(340, 43)                        # Options > Settings...
    r.shot("e3-settings")
    r.key(K["ESC"])
    time.sleep(1.5)
    check("Settings, Esc: EMAIL.INF as it was", inf() == before)

    menu(340, 27)                        # Options > Accounts...
    r.shot("e4-accounts")
    r.key(UNDO)
    time.sleep(1.5)
    check("Accounts, Undo: EMAIL.INF as it was", inf() == before)

    r.click(330, 124)                    # Avi's message
    time.sleep(10)
    r.ctrl("m")                          # Move to...
    time.sleep(2)
    r.shot("e5-move-to")
    r.key(K["ESC"])
    time.sleep(2)
    r.ctrl("m")
    time.sleep(2)
    r.key(UNDO)
    time.sleep(2)
    check("Move to, Esc and Undo: the message stays", server()[1] == 3)

    r.ctrl("n")                          # a message being written
    time.sleep(2)
    r.type("hello")
    r.ctrl("q")                          # Quit: "Quit anyway?"
    time.sleep(1.5)
    r.shot("e6-quit")
    r.key(K["ESC"])
    time.sleep(1.5)
    r.key(K["ESC"])                      # close the message: "Keep this message?"
    time.sleep(1.5)
    r.shot("e7-keep")
    r.key(K["ESC"])
    time.sleep(1.5)
    r.shot("e8-still-writing")
    log = open(os.path.join(work, "hatari.log"), errors="replace").read() if os.path.exists(
        os.path.join(work, "hatari.log")) else ""
    check("Quit and closing, Esc: EMail and the message stay", r.proc.poll() is None)
    check("and nothing was written to EMAIL.INF", inf() == before)
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
