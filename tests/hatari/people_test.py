#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EMail and EContact (the AERA guidelines, 4.1): people by name.

As net_test.py (Dovecot, the SMTP stand-in, FAKESTNG.PRG over the serial
port), with build/fakecontact.acc as C:\\ECONTACT.ACC: a stand-in for
EContact that knows Dana, Avi, Maya and "Baker, Grace". EMail opens Dana's
message, replies, adds two people with ^B (picked in "EContact") and
sends. The test checks what EMail asked (C:\\PEOPLE.LOG), what reached the
server, and leaves screenshots of the names in the list, the reader and
the To: line.

    people_test.py WORKDIR [MACHINE]  (make build/fakecontact.acc first)
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
shutil.copy(os.path.join(ROOT, "binary", "EMAIL.PRG"), hd)
rig.fakesting(os.path.join(hd, "AUTO"))
shutil.copy(os.path.join(ROOT, "build", "fakecontact.acc"), os.path.join(hd, "ECONTACT.ACC"))
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


try:
    time.sleep(40)                       # boot, log in, mirror the inbox
    r.shot("p1-list")                    # Dana Cohen, Avi Levi: EContact's names
    r.click(330, 108)                    # newest message: Dana's
    time.sleep(12)
    r.move(290, 259)
    time.sleep(0.3)
    r.move(291, 260)                     # rest on the From: name
    time.sleep(2)
    r.shot("p2-reader")
    r.ctrl("r")                          # reply: To: Dana
    time.sleep(3)
    r.shot("p3-reply")
    r.ctrl("b")                          # ^B: picked in EContact
    time.sleep(3)
    r.shot("p4-picked")
    r.click(300, 300)                    # into the text: the To: line all names
    time.sleep(1)
    r.move(230, 89)
    time.sleep(0.3)
    r.move(231, 90)
    time.sleep(2)
    r.shot("p5-tooltip")
    r.click(130, 89)                     # a click on Dana: her text, to change it
    time.sleep(1)
    r.shot("p5b-edit")
    r.key(rig.SCAN["d"], shift=True, ctrl=True)   # Shift+^D: send now
    time.sleep(35)
    r.shot("p6-sent")

    log = open(os.path.join(hd, "PEOPLE.LOG"), errors="replace").read() if os.path.exists(
        os.path.join(hd, "PEOPLE.LOG")) else ""
    print("PEOPLE.LOG:", log.replace("\r\n", " | "))
    check("EMail asked EContact for names", "names:EMAIL " in log and "dana@example.com" in log)
    check("^B asked EContact to pick", "pick:EMAIL" in log)

    m = imaplib.IMAP4("127.0.0.1", IMAP)
    m.login("dana", "secret")
    m.select("INBOX")
    typ, data = m.search(None, "ALL")
    to = ""
    for num in data[0].split():
        typ, msg = m.fetch(num, "(RFC822)")
        e = email.message_from_bytes(msg[0][1])
        subj = str(email.header.make_header(email.header.decode_header(e["Subject"])))
        if subj.startswith("Re: "):
            to = str(email.header.make_header(email.header.decode_header(e["To"])))
    print("To:", to)
    check("the reply went to Dana", "dana@example.com" in to)
    check("and to Maya, picked", "Maya Carter <maya@pixelpress.example>" in to)
    check("and to Grace, her name quoted", '"Baker, Grace" <grace@example.net>' in to)
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
