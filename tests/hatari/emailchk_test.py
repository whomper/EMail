#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EMAILCHK.ACC, "EMail check": new mail found while EMail isn't running.

As link_test.py (Dovecot, FAKESTNG.PRG over the serial port), with
EMail and the accessory in C:\\ (CHECK=1 in EMAIL.INF: a check a minute),
EMAILCHK.INF naming the folder (START=now).

1. Booted to the desktop. The first check finds nothing new: EMail has
   never looked at the Inbox, so what is there is not news.
2. Two unread messages and one read elsewhere come: the next check
   finds 2 and says so. On plain TOS the desktop can't be asked to start
   EMail, so the box only tells ("Start EMail to read them"); the
   server's flags stay as they were.
3. Checked again with nothing more: not told again. One more comes:
   told, 3 new.
4. The accessory from the Desk menu: when it last looked, Check now.
5. Booted with EMail started (Hatari's --auto): while it runs the
   accessory leaves the checking to it. EMail quits; one more message
   comes: a minute after EMail ended the accessory finds 1.
The console lines ("EMAILCHK checked ...") say what each check found.

With "direct" after the machine, the account is a Direct connection
(FALCON=1, IMAPS with the test certificate the root it trusts): the
accessory loads TLS.OVL into its own memory and does TLS itself. That
memory is taken only in TT RAM (Falcon, TT; not Falcon-4, where such an
account is left unchecked and the check says why).

With "pop3", the account is POP3 (Dovecot's, port 10110), the accessory's
own POP3 path: new is what EMail hasn't downloaded (UIDL.DAT), so the
Inbox starts empty, and the message read on the phone counts too (POP3
has no flags): 3 new where IMAP finds 2, then 4 where it finds 3.

With "trace", Hatari writes its GEMDOS and AES trace (trace1.txt, trace2.txt)
and acc_trace.py checks the accessory against them: no file of its open
across an event call, no GEMDOS memory taken after its first one (the
guidelines, 2.2). CHK_ACC names another build of the accessory to test,
such as an AES_PROFILE one (its stretches go to hatari.log as AESGAP).

    emailchk_test.py WORKDIR [MACHINE] [direct] [pop3] [trace]
"""
import imaplib
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

work, machine, more = rig.cli("Falcon")
direct = "direct" in more
trace = "trace" in more
pop = "pop3" in more
NEW2, NEW3 = (3, 4) if pop else (2, 3)


def traced(n):
    return ["--trace", "gemdos,aes", "--trace-file", os.path.join(work, "trace%d.txt" % n)] if trace else []
IMAPS = 10993
os.makedirs(work, exist_ok=True)
srv = os.path.join(work, "srv")
hd = os.path.join(work, "hd")
for d in (srv, hd, hd + ".home"):
    shutil.rmtree(d, ignore_errors=True)
run_dir = subprocess.run(["mktemp", "-d", "/tmp/tdv.XXXXXX"], capture_output=True, text=True).stdout.strip()
MD = os.path.join(srv, "home/dana/Maildir")
for d in ("state", "home/dana/Maildir/new", "home/dana/Maildir/cur", "home/dana/Maildir/tmp"):
    os.makedirs(os.path.join(srv, d))
os.chmod(work, 0o755)
os.chmod(srv, 0o755)
with open(os.path.join(srv, "users"), "w") as f:
    f.write("dana:{PLAIN}secret\n")
uid, gid = subprocess.check_output(["id", "-u", "nobody"]).strip(), subprocess.check_output(["id", "-g", "nobody"]).strip()
conf = open(os.path.join(ROOT, "tests", "dovecot.conf.in")).read()
if direct:
    cert, key = os.path.join(srv, "cert.pem"), os.path.join(srv, "key.pem")
    subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2",
                    "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost,DNS:127.0.0.1",
                    "-keyout", key, "-out", cert], check=True, capture_output=True)
    os.chmod(key, 0o644)
    conf = conf.replace("ssl = no", "ssl = yes\nssl_cert = <%s\nssl_key = <%s" % (cert, key))
    conf = conf.replace("inet_listener imaps {\n    port = 0", "inet_listener imaps {\n    port = %d" % IMAPS)
for k, v in (("@RUN@", run_dir), ("@DIR@", srv), ("@IMAP@", str(IMAP)), ("@POP3@", "10110"),
             ("@UID@", uid.decode()), ("@GID@", gid.decode())):
    conf = conf.replace(k, v)
with open(os.path.join(srv, "dovecot.conf"), "w") as f:
    f.write(conf)
for m in [] if pop else sorted(os.listdir(os.path.join(ROOT, "tests", "mail"))):
    shutil.copy(os.path.join(ROOT, "tests", "mail", m), os.path.join(MD, "new", m))
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110, IMAPS)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

os.makedirs(os.path.join(hd, "AUTO"))
em = hd
rig.install_prg(hd)
shutil.copy(os.environ.get("CHK_ACC", os.path.join(ROOT, "binary", "EMAILCHK.ACC")),
            os.path.join(hd, "EMAILCHK.ACC"))
rig.fakesting(os.path.join(hd, "AUTO"))
if direct:
    # the full Mozilla list, the test certificate with it, decoded here
    with open(os.path.join(hd, "CACERT.PEM"), "wb") as f:
        f.write(open(os.path.join(ROOT, "binary", "CACERT.PEM"), "rb").read() + open(cert, "rb").read())
    subprocess.run([os.path.join(ROOT, "build", "mkroots"), os.path.join(hd, "CACERT.PEM"),
                    os.path.join(hd, "ROOTS.DAT")], check=True)
with open(os.path.join(em, "EMAIL.INF"), "w", newline="") as f:
    # tz=0 for a Direct connection: the certificate's dates against the
    # emulator's clock, this computer's (UTC)
    f.write("[options]\r\ntz=%d\r\ncheck=1\r\nlog=1\r\nfalcon=%d\r\n[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
            "email=dana@test.local\r\nin=%s\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
            "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n"
            "dhost=127.0.0.1\r\ndport=%d\r\ndsec=1\r\n"
            % (0 if direct else 180, direct, "pop3" if pop else "imap", 10110 if pop else IMAP, SMTP, IMAPS))
with open(os.path.join(hd, "EMAILCHK.INF"), "w", newline="") as f:
    f.write("FOLDER=C:\\\r\nSTART=now\r\n")

fifo_out, fifo_in = os.path.join(work, "st_out"), os.path.join(work, "st_in")
for p in (fifo_out, fifo_in):
    if os.path.exists(p):
        os.remove(p)
    os.mkfifo(p)
bridge = rig.bridge(work, fifo_out, fifo_in)
shutil.rmtree(os.path.join(work, "shots"), ignore_errors=True)
fails = []


def check(what, good, detail=""):
    print("  %s %s%s" % ("ok  " if good else "FAIL", what, ("  (" + detail + ")") if detail else ""), flush=True)
    if not good:
        fails.append(what)


def console():
    return open(os.path.join(work, "hatari.log"), errors="replace").read()


def checks():
    return [l for l in console().splitlines() if l.startswith("EMAILCHK checked")]


def wait_check(n, most):
    """until the console has n check lines (or most seconds): the last"""
    end = time.time() + most
    while time.time() < end:
        c = checks()
        if len(c) >= n:
            return c[n - 1]
        time.sleep(2)
    c = checks()
    return c[-1] if c else ""


def deliver(*msgs):
    m = imaplib.IMAP4("127.0.0.1", IMAP)
    m.login("dana", "secret")
    for flags, subject in msgs:
        m.append("INBOX", flags, None, ("From: Avi <avi@example.com>\r\nTo: dana@test.local\r\n"
                                        "Subject: %s\r\n\r\nHello.\r\n" % subject).encode())
    m.logout()


def unseen():
    m = imaplib.IMAP4("127.0.0.1", IMAP)
    m.login("dana", "secret")
    m.select("INBOX", readonly=True)
    n = len(m.search(None, "UNSEEN")[1][0].split())
    m.logout()
    return n


r = rig.Rig(work, machine, serial=(fifo_out, fifo_in), auto=None, extra=traced(1))
try:
    # 1. the first look: what is there was there before
    line = wait_check(1, 240)
    check("the first check, at boot: nothing new", re.search(r": 0 new$", line) is not None, line)
    r.shot("c1-desktop")
    before = unseen()

    # 2. mail comes: found at the next check (a minute), and told
    deliver(("()", "First news"), ("(\\Seen)", "Read on the phone"), ("()", "Second news"))
    line = wait_check(2, 150)
    check("the next check finds the %d new" % NEW2, re.search(r": %d new, fresh$" % NEW2, line) is not None, line)
    time.sleep(3)
    r.shot("c2-told")
    check("... and the server's flags are as they were", unseen() == before + 2, "%d unread" % unseen())
    r.key(K["RETURN"])               # OK
    time.sleep(2)

    # 3. nothing more: not told again; one more: told
    line = wait_check(3, 150)
    check("checked again with nothing more: not told again", re.search(r": %d new$" % NEW2, line) is not None, line)
    deliver(("()", "Third news"))
    line = wait_check(4, 150)
    check("one more: told again, %d new" % NEW3, re.search(r": %d new, fresh$" % NEW3, line) is not None, line)
    time.sleep(3)
    r.shot("c3-told-again")
    r.key(K["ESC"])
    time.sleep(2)

    # 4. from the Desk menu: when it last looked; Check now
    r.click(40, 8)
    time.sleep(1.5)
    r.shot("c4-desk-menu")
    # the third row: Desktop Info..., the line, then the accessory (ours
    # is the only one), rows of the system font's height
    from PIL import Image
    row = 16 if Image.open(os.path.join(work, "shots", "c4-desk-menu.png")).size[1] >= 400 else 8
    target = row + 2 * row + row // 2 + 3
    r.click(60, target)
    time.sleep(3)
    r.shot("c5-opened")
    check("the Desk menu entry opens it", "EMAILCHK opened" in console(), "clicked y=%d" % target)
    n = len(checks())
    r.key(K["RETURN"])               # Check now
    line = wait_check(n + 1, 60)
    time.sleep(2)
    r.shot("c6-check-now")
    check("Check now checks at once, and tells", len(checks()) == n + 1 and ": %d new" % NEW3 in line, line)
    r.key(K["RETURN"])
    time.sleep(2)
finally:
    r.stop()

# 5. EMail running: the accessory waits; after it, the next message
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in), extra=traced(2))
try:
    r.settle(45, 240)
    r.shot("e1-email")
    check("EMail runs (started by Hatari)", "EMail: v_opnvwk" in console())
    n = len(checks())
    deliver(("()", "While EMail runs"))
    time.sleep(90)
    check("while EMail runs, the accessory leaves the checking to it", len(checks()) == n,
          "%d checks, then %d" % (n, len(checks())))
    r.ctrl("q")
    time.sleep(8)
    r.shot("e2-quit")
    deliver(("()", "Fourth news"))
    line = wait_check(n + 1, 150)
    if "TLS error 10" in line or "Logging out" in line:
        # FAKESTNG, not the accessory: the first connection after EMail
        # quit can be handed the answer to EMail's own last LOGOUT ("* BYE
        # Logging out", or its TLS record: 17 03 03 00 54 was read first),
        # which real STinG never does; the next check, a minute on, is a
        # connection of its own
        print("  note: the first check after EMail met FAKESTNG's leftover bytes:", line, flush=True)
        n += 1
        line = wait_check(n + 1, 150)
    check("after EMail: the check finds the one that came since", re.search(r": 1 new, fresh$", line) is not None,
          line)
    time.sleep(3)
    r.shot("e3-told")
    print("\n".join(checks()))
finally:
    r.stop()
bridge.terminate()
smtp.terminate()
subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
shutil.rmtree(run_dir, ignore_errors=True)
if trace:
    for n in (1, 2):
        out = subprocess.run([sys.executable, os.path.join(HERE, "acc_trace.py"),
                              os.path.join(work, "trace%d.txt" % n)], capture_output=True, text=True).stdout
        print(out, end="")
        check("trace %d: no file open across an event call, no memory taken after the first" % n,
              "\nOK:" in "\n" + out)
print("screenshots in", os.path.join(work, "shots"))
print("EMail check test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
