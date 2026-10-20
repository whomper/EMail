#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EMail with the real EContact (the AERA guidelines, 4.1 and 1.2): its
latest release, as Erez has it, not a build of its main.

As people_test.py (Dovecot, the SMTP stand-in, FAKESTNG.PRG over the
serial port), but C:\\ECONTACT.ACC is EContact's release (ECONTST.ACC, the ST
edition, on ST, as a user there has it; not on ST-1, where the two don't
fit together and EContact runs as a program: Erez, 19 Oct 2026), with an address
book of Dana, Avi and Grace (tests/hatari/people.vcf) made by EContact's
card-cli from the same release's source. EMail replies to Dana, types
"Grace" on the To: line and presses ^B: EContact's window opens searching
for her, Return chooses her, and the reply goes to Dana and Grace Baker.

    econtact_test.py WORKDIR [MACHINE] [vX.Y]   (EContact's latest release, or that one)
"""
import email
import email.header
import imaplib
import os
import re
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
if machine.partition(":")[0] == "ST-1":
    print("SKIP on %s: 1 MB: EMail and the accessory don't fit together, Erez 19 Oct" % machine)
    sys.exit(0)
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

# EContact: its release, and its address book made by its own card-cli
# from the same release's source
ec_spec = "EContact@" + more[0] if more else "EContact"
ec_dir = os.path.join(work, "econtact")
shutil.rmtree(ec_dir, ignore_errors=True)
acc = "ECONTST.ACC" if machine.partition(":")[0] == "ST" else "ECONTACT.ACC"
tag = rig.release(ec_spec, ec_dir, acc)
print("EContact", tag)
ec_src = os.path.join(ec_dir, "src")
subprocess.run(["git", "-c", "advice.detachedHead=false", "clone", "-q", "--depth", "1", "--branch", tag, "https://github.com/whomper/EContact",
                ec_src], check=True)
subprocess.run(["make", "-s", "-C", ec_src, "build/card-cli"], check=True, stdout=subprocess.DEVNULL)
cli = os.path.join(ec_src, "build", "card-cli")
book = os.path.join(hd, "ECONTACT")
os.makedirs(book)
subprocess.run([cli, book, "local", "Contacts"], check=True, stdout=subprocess.DEVNULL)
subprocess.run([cli, book, "import", "Contacts", os.path.join(HERE, "people.vcf")], check=True,
               stdout=subprocess.DEVNULL)
inf = open(os.path.join(book, "ECONTACT.INF"), "rb").read()
inf = inf.replace(b"\nSYNC_OPEN=1", b"\nSYNC_OPEN=0")
inf = re.sub(rb"\nSTART=[^\r\n]*", rb"\nSTART=now", inf)
inf = re.sub(rb"\nFOLDER=[^\r\n]*", rb"\nFOLDER=C:\\ECONTACT", inf)
os.remove(os.path.join(book, "ECONTACT.INF"))
open(os.path.join(hd, "ECONTACT.INF"), "wb").write(inf)
shutil.copy(os.path.join(ec_dir, acc), hd)

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


try:
    time.sleep(45)                       # boot, EContact reads its book, EMail mirrors the inbox
    r.shot("c1-list")                    # Dana Cohen, Avi Levi: EContact's names
    r.click(330, 108)                    # newest message: Dana's
    time.sleep(12)
    r.shot("c2-reader")
    r.ctrl("r")                          # reply: To: Dana
    time.sleep(3)
    r.click(500, 89)                     # the end of the To: line
    time.sleep(0.5)
    r.type(", Grace")
    r.ctrl("b")                          # ^B: EContact's window, searching for Grace
    time.sleep(8)
    r.shot("c3-choosing")
    r.key(K["RETURN"])                   # she is chosen
    time.sleep(4)
    r.shot("c4-picked")
    r.key(rig.SCAN["d"], shift=True, ctrl=True)   # Shift+^D: send now
    time.sleep(35)
    r.shot("c5-sent")

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
    check("and to Grace, chosen in EContact %s" % tag, "Grace Baker <grace@example.net>" in to)

    # the other way: EContact's mail button hands EMail (either edition,
    # av_send "EMAIL|EMAILST") a mailto:, and EMail opens a new message
    r.click(40, 9)                       # the Desk menu
    time.sleep(1)
    r.click(60, 60)                      # EContact, the first accessory under About EMail
    time.sleep(8)
    r.ctrl("f")                          # a new search: the pick left "Grace" in it
    time.sleep(1)
    r.type("Avi")
    time.sleep(4)
    r.shot("c6-econtact")
    r.click(447, 182)                    # mail, under his name
    time.sleep(10)
    r.shot("c7-mailto")
    r.type("Hello Avi, from EContact's mail button.")
    r.key(rig.SCAN["d"], shift=True, ctrl=True)   # Shift+^D: send now
    time.sleep(35)
    r.shot("c8-sent")
    # Avi has no mailbox here: the SMTP stand-in files his mail in srv/sink
    to = ""
    sink = os.path.join(srv, "sink", "new")
    for name in sorted(os.listdir(sink)) if os.path.isdir(sink) else []:
        raw = open(os.path.join(sink, name), "rb").read()
        if b"from EContact's mail button" in raw:
            e = email.message_from_bytes(raw)
            to = str(email.header.make_header(email.header.decode_header(e["To"])))
    print("mailto To:", to)
    check("EContact's mail button opened a message in %s, to Avi" % rig.PRG, "avi@" in to.lower())
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
