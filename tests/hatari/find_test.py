#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Find (Erez, 31 Oct 2026), as net_test.py: Dovecot, FAKESTNG.PRG over
the serial port, the test build (build/trace/email.prg says on the
console "FIND on", "FIND off" and, each time the list follows the field,
"FIND disk|server "text" N", N the rows listed).

Ctrl+F opens the field over the list; typing leaves the messages whose
From or Subject has the text, case aside, once the keys pause; Esc
empties the field and the whole folder is back; a word only in a body
is found by Search server (Shift+Return), not on disk; Undo, Ctrl+F
again and another folder leave Find. Screenshots of each.

    find_test.py WORKDIR [MACHINE]   (make build/trace/email.prg first)
"""
import os
import re
import shutil
import subprocess
import sys
import time

import rig
from rig import K

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
IMAP, SMTP = 10143, 10025
UNDO = 0x61

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

os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd, os.path.join(ROOT, "build", "trace", "email.prg"))
rig.fakesting(os.path.join(hd, "AUTO"))
with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
    f.write("[options]\r\ntz=180\r\nlog=1\r\n[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
            "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n" % (IMAP, SMTP))

fifo_out, fifo_in = os.path.join(work, "st_out"), os.path.join(work, "st_in")
for p in (fifo_out, fifo_in):
    if os.path.exists(p):
        os.remove(p)
    os.mkfifo(p)
bridge = rig.bridge(work, fifo_out, fifo_in)
shutil.rmtree(os.path.join(work, "shots"), ignore_errors=True)
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
checks = []


def check(what, good, more=""):
    checks.append(good)
    print("  %s   %s%s" % ("ok" if good else "FAIL", what, ("   (%s)" % more) if more and not good else ""))


def finds():
    log = open(os.path.join(work, "hatari.log"), errors="replace").read()
    return [l for l in log.splitlines() if l.startswith("FIND ")]


def after(n, secs=8):
    """the FIND lines that came after the first n, once they stop coming"""
    end = time.time() + secs
    got = finds()[n:]
    while time.time() < end:
        time.sleep(0.5)
        now = finds()[n:]
        if now and now == got:
            break
        got = now
    return got


def listed(lines, where, text):
    """the rows listed for the last FIND of that text, or None"""
    for l in reversed(lines):
        m = re.match(r'FIND (disk|server) "(.*)" (\d+)$', l)
        if m and m.group(1) == where and m.group(2) == text:
            return int(m.group(3))
    return None


try:
    r.settle(40, 240)
    n = len(finds())
    r.ctrl("f")
    got = after(n, 4)
    r.shot("f1-open")
    check("Ctrl+F opens Find", "FIND on" in got, got)

    n = len(finds())
    r.type("news")
    got = after(n)
    r.shot("f2-news")
    check("typed: only the newsletter, once the keys paused (1)", listed(got, "disk", "news") == 1, got)
    check("... the list followed once, not at each key",
          len([l for l in got if l.startswith("FIND disk")]) == 1, got)

    n = len(finds())
    r.key(K["ESC"])
    got = after(n)
    r.shot("f3-esc")
    check("Esc empties the field: the whole folder (3)", listed(got, "disk", "") == 3, got)
    check("... and Find stays open", "FIND off" not in got, got)

    n = len(finds())
    r.type("TEL AVIV")
    got = after(n)
    check("case aside: the Hebrew message's subject (1)", listed(got, "disk", "TEL AVIV") == 1, got)

    r.key(K["ESC"])
    time.sleep(1)
    n = len(finds())
    r.type("Falcon030")
    got = after(n)
    r.shot("f4-disk-none")
    check("a word only in a body: nothing on disk (0)", listed(got, "disk", "Falcon030") == 0, got)
    n = len(finds())
    r.key(K["RETURN"], shift=True)
    got = after(n, 15)
    r.shot("f5-server")
    check("Search server finds it in the body (1)", listed(got, "server", "Falcon030") == 1, got)

    n = len(finds())
    r.key(UNDO)
    got = after(n, 4)
    r.shot("f6-undo")
    check("Undo leaves Find", "FIND off" in got, got)

    n = len(finds())
    r.ctrl("f")
    time.sleep(1.5)
    r.ctrl("f")
    got = after(n, 4)
    check("Ctrl+F again leaves it", got[-1:] == ["FIND off"] and "FIND on" in got, got)
    r.shot("f7-closed")

    # another folder leaves Find; back in the Inbox, the list as before
    n = len(finds())
    r.ctrl("f")
    time.sleep(1)
    r.type("news")
    after(n)
    n = len(finds())
    r.click(48, 121)                     # Outbox
    got = after(n, 6)
    check("another folder leaves Find", "FIND off" in got, got)
    r.click(48, 105)                     # the Inbox again
    time.sleep(3)
    r.shot("f8-inbox-again")
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
print("PASS" if all(checks) else "FAIL")
sys.exit(0 if all(checks) else 1)
