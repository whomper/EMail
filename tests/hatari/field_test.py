#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A text field holds more than it shows (the AERA guidelines, 5.4;
common/ui/field.c), as net_test.py: Dovecot, FAKESTNG.PRG over the
serial port. More is typed into each kind of EMail's fields than the
field shows, and the whole of it comes back:

- Accounts, "Your name" (shows 30): a name of 58, with a letter put in
  at its start (Shift+Left) and ^C copying the whole of it (SCRAP.TXT);
  EMAIL.INF has it all;
- Settings, Signature (a row shows 40): a line of 65;
- Settings, Contacts, EContact's folder (shows 32): a path of 57;
- New folder (shows 46): a name of 52, made on the server.

    field_test.py WORKDIR [MACHINE]
"""
import imaplib
import os
import shutil
import subprocess
import sys
import time

from PIL import Image

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
    f.write("[options]\r\ntz=180\r\nlog=1\r\n[account]\r\nname=Dana\r\nfullname=Dana\r\n"
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
# the Settings dialog's list of parts and where to click in a part, by
# the screen's height
SPOTS = {480: dict(signature=(140, 226), contacts=(140, 306), sigrow=(260, 152), pdir=(260, 246)),
         400: dict(signature=(140, 186), contacts=(140, 266), sigrow=(260, 112), pdir=(260, 206))}
NAME = "Dana " + "".join("abcdefghij"[k % 10] for k in range(52))     # 57, and the X: 58
SIG = "-- " + "a signature line of sixty-four, past what a row shows: " + "xyz1234"
PDIR = "C:\\" + "\\".join(["econtact", "folders", "deep", "inside", "with", "long", "names", "here", "end1"])
FOLDER = "a folder name that is longer than the field shows x1"


def check(what, good):
    checks.append(good)
    print("  %s   %s" % ("ok" if good else "FAIL", what), flush=True)


def inf():
    return open(os.path.join(hd, "EMAIL.INF"), "rb").read().decode("latin-1")


def inf_value(key):
    for line in inf().splitlines():
        if line.lower().startswith(key.lower() + "="):
            return line.split("=", 1)[1]
    return None


def menu(x, y):
    r.click(x, 9)
    time.sleep(0.6)
    r.click(x + 20, y)
    time.sleep(1.5)


def scrap():
    d = os.path.join(hd, "CLIPBRD")
    p = os.path.join(d, "SCRAP.TXT") if os.path.isdir(d) else ""
    return open(p, "rb").read().decode("latin-1") if p and os.path.exists(p) else ""


try:
    r.settle(40, 120)                    # boot, log in, mirror the inbox
    spots = SPOTS[Image.open(r.shot("f0-start")).size[1]]

    menu(340, 27)                        # Options > Accounts...
    r.key(K["RETURN"])                   # Edit: the one account
    time.sleep(2)
    r.key(K["TAB"])                      # Your name: "Dana", the cursor at its end
    time.sleep(0.5)
    r.type(NAME[4:], pace=0.05)
    r.key(K["LEFT"], shift=True)        # to the start
    r.key(SCAN["x"], shift=True)         # an X put in there
    time.sleep(0.5)
    r.shot("f1-account")
    r.ctrl("c")                          # the whole name to the clipboard
    time.sleep(2)
    copied = scrap()
    r.key(K["RETURN"])                   # OK
    time.sleep(2)
    r.key(K["ESC"])                      # "Check this account now?": Later
    time.sleep(3)
    print("fullname:", inf_value("fullname"))
    check("Your name holds %d characters and shows 30" % len("X" + NAME), inf_value("fullname") == "X" + NAME)
    check("^C copied the whole of it (%r)" % copied.strip()[:20], copied.strip() == "X" + NAME)

    menu(340, 43)                        # Options > Settings...
    r.click(*spots["signature"])
    time.sleep(1)
    r.click(*spots["sigrow"])            # the signature's first row
    time.sleep(0.5)
    r.type(SIG, pace=0.05)
    time.sleep(0.5)
    r.shot("f2-signature")
    r.click(*spots["contacts"])
    time.sleep(1)
    r.click(*spots["pdir"])              # EContact's folder
    time.sleep(0.5)
    r.type(PDIR, pace=0.05)
    time.sleep(0.5)
    r.shot("f3-contacts")
    r.key(K["RETURN"])                   # OK
    time.sleep(3)
    print("signature:", inf_value("signature"))
    print("peopledir:", inf_value("peopledir"))
    check("a signature row holds a line of %d and shows 40" % len(SIG), inf_value("signature") == SIG)
    check("EContact's folder holds a path of %d and shows 32" % len(PDIR), inf_value("peopledir") == PDIR)

    menu(272, 59)                        # Folder > New folder...
    r.type(FOLDER, pace=0.05)
    time.sleep(0.5)
    r.shot("f4-new-folder")
    r.key(K["RETURN"])
    time.sleep(8)
    m = imaplib.IMAP4("127.0.0.1", IMAP)
    m.login("dana", "secret")
    folders = " ".join(l.decode() for l in m.list()[1])
    m.logout()
    print("folders:", folders)
    check("New folder holds a name of %d and shows 46" % len(FOLDER), FOLDER in folders)
    r.shot("f5-done")
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
