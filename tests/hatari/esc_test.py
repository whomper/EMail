#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Esc and Undo (the AERA guidelines, 3.5: Erez, 27 Oct 2026), as
net_test.py: Dovecot, FAKESTNG.PRG over the serial port. Undo cancels a
dialog; Esc empties the text field the cursor is in and the dialog stays,
and cancels a dialog without a field and an alert. Each dialog and alert
EMail can be driven to here is closed so, and nothing it would have done
happens: Download all downloads nothing, New folder (a name typed, Esc,
Undo) makes none, Accounts leaves EMAIL.INF as it was, Move to moves
nothing, and Quit and closing a message being written (^U) keep EMail
and the message, in which Esc does nothing. Settings: Esc empties its
first field (Check for new mail every), and OK keeps it so. Screenshots
of each dialog and alert.

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
    got = fetches()                      # (TOS 1.04 passes the menu's click on to the list too)
    r.key(K["ESC"])
    time.sleep(3)
    check("Download all, Esc: nothing downloaded", fetches() == got)

    menu(272, 59)                        # Folder > New folder...
    r.type("Zzz")
    time.sleep(0.5)
    r.shot("e2-new-folder")
    r.key(K["ESC"])                      # in the text field: emptied
    time.sleep(1)
    r.shot("e2-new-folder-emptied")
    r.key(UNDO)                          # cancels
    time.sleep(3)
    check("New folder, Esc in the name then Undo: no folder", "Zzz" not in server()[0])

    menu(340, 43)                        # Options > Settings...
    r.shot("e3-settings")
    r.key(K["ESC"])                      # empties "Check for new mail every"
    time.sleep(1)
    r.shot("e3-settings-emptied")
    r.key(K["RETURN"])                   # OK: the dialog was still open
    time.sleep(2)
    now = inf()
    check("Settings, Esc in a field: the dialog stays, the field empty (CHECK=0 on OK)",
          b"CHECK=0" in now and now != before)
    before = now

    menu(340, 27)                        # Options > Accounts...: Edit, New, Cancel
    r.shot("e4-accounts")
    r.key(K["RETURN"])                   # Edit, the alert's default: the account dialog
    time.sleep(2)
    r.key(K["ESC"])                      # the tester's case (27 Oct 2026): in the name
    time.sleep(1)                        # Esc empties it; the account stays open
    r.shot("e4-account-esc")
    r.key(UNDO)
    time.sleep(1.5)
    check("Account, Esc in the name then Undo: EMAIL.INF as it was", inf() == before)
    menu(340, 27)
    r.key(K["RETURN"])
    time.sleep(2)
    r.key(K["ESC"])                      # emptied, still open: a new name typed
    r.type("Esc test")
    r.key(K["RETURN"])
    time.sleep(4)
    now = inf()
    check("Account, Esc then a name and OK: the account renamed, not cancelled",
          b"Esc test" in now)
    before = now

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
    r.key(K["ESC"])                      # in the text: nothing
    time.sleep(1)
    r.ctrl("u")                          # close the message: "Keep this message?"
    time.sleep(1.5)
    r.shot("e7-keep")
    r.key(K["ESC"])
    time.sleep(1.5)
    r.shot("e8-still-writing")
    check("Quit and closing (^U), Esc: EMail and the message stay", r.proc.poll() is None)
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
