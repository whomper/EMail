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
The console lines ("EMAILCHK checked ...") say what each check found; the
bell rings with each piece of news told ("BELL ym" on an ST, "BELL dma"
on the TT and Falcon), and not when nothing new is told.

With "direct" after the machine, the account is a Direct connection
(FALCON=1, IMAPS with the test certificate the root it trusts): the
accessory loads TLS.OVL into its own memory and does TLS itself. That
memory is taken only in TT RAM (Falcon, TT).

With "pop3", the account is POP3 (Dovecot's, port 10110), the accessory's
own POP3 path: new is what EMail hasn't downloaded (UIDL.DAT), so the
Inbox starts empty, and the message read on the phone counts too (POP3
has no flags): 3 new where IMAP finds 2, then 4 where it finds 3.

With "trace", Hatari writes its GEMDOS and AES trace (trace1.txt, trace2.txt)
and acc_trace.py checks the accessory against them: no file of its open
across an event call, no GEMDOS memory taken after its first one (the
guidelines, 2.2). CHK_ACC names another build of the accessory to test,
such as an AES_PROFILE one (its stretches go to hatari.log as AESGAP).

    emailchk_test.py WORKDIR [MACHINE] [direct] [pop3] [trace] [held]

With "held" (guidelines 2.2, decided 26 Nov 2026: an accessory's GEMDOS
memory is its own on plain TOS; ECal's held boot case), FILLMEM.PRG
(tests/atari/fillmem.c, without the fast-load bit) is auto-started at
boot twice: alone, the reference, filling all it is given; then holding
the front in an event call for 30 s, ending, and running again to fill.
That run gets no more than the reference, within 4 KB; mail comes, and
the next two checks find it, without a crash (make
build/fillmem.prg first).

MACHINE is ST-2, TT or Falcon: the accessories are not tested on the
4 MB machines, ST and Falcon-4 (decided 4 Nov 2026). On ST-1, a 1 MB ST,
no family accessory runs (guidelines 2.1, decided 26 Nov 2026), and the
test is that instead: booted with the GEMDOS trace, EMAILCHK.ACC takes no
memory, opens no file and checks nothing; its Desk entry opens the alert
that says it needs 2 MB, and OK takes it away.
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
if machine.partition(":")[0] in ("ST", "Falcon-4"):
    sys.exit("emailchk_test.py: no accessory runs on ST or Falcon-4 (decided 4 Nov 2026): use ST-2, TT or Falcon")
small = machine.partition(":")[0] == "ST-1"
held = "held" in more
busy = "busy" in more            # a player holding the sound beside it
bell_off = "belloff" in more     # BELL=0 in EMAIL.INF
direct = "direct" in more
trace = "trace" in more
pop = "pop3" in more
NEW2, NEW3 = (3, 4) if pop else (2, 3)


def traced(n):
    return ["--trace", "gemdos,aes", "--trace-file", os.path.join(work, "trace%d.txt" % n)] if trace else []
IMAPS = 10993
os.makedirs(work, exist_ok=True)


def small_machine():
    """ST-1: the accessory stands down (guidelines 2.1)"""
    from PIL import Image, ImageChops
    hd = os.path.join(work, "hd")
    shutil.rmtree(hd, ignore_errors=True)
    shutil.rmtree(os.path.join(work, "shots"), ignore_errors=True)
    os.makedirs(hd)
    shutil.copy(os.environ.get("CHK_ACC", os.path.join(ROOT, "binary", "emailchk.acc")),
                os.path.join(hd, "emailchk.acc"))
    with open(os.path.join(hd, "emailchk.inf"), "w", newline="") as f:
        f.write("FOLDER=C:\\\r\nSTART=now\r\nNATFEATS=1\r\n")
    tr = os.path.join(work, "trace1.txt")
    bad = []

    def check(what, good, detail=""):
        print("  %s %s%s" % ("ok  " if good else "FAIL", what, ("  (" + detail + ")") if detail else ""), flush=True)
        if not good:
            bad.append(what)

    r = rig.Rig(work, machine, auto=None, extra=["--trace", "gemdos,aes", "--trace-file", tr])
    try:
        time.sleep(40)               # booted, and past the moment START=now would check
        row = 16 if Image.open(r.shot("s0-boot")).size[1] >= 400 else 8
        y = row + 2 * row + row // 2 + 3
        r.move(60, y)                # where the pointer will be after the alert
        time.sleep(1)
        desk = r.shot("s1-desktop")
        r.click(40, 8)
        time.sleep(1.5)
        r.shot("s2-desk-menu")
        r.click(60, y)
        time.sleep(3)
        told = r.shot("s3-alert")
        box = ImageChops.difference(Image.open(desk).convert("RGB"), Image.open(told).convert("RGB")).getbbox()
        w, h = Image.open(desk).size
        check("the Desk entry opens an alert", box is not None and box[2] - box[0] > w // 4
              and box[3] - box[1] > h // 6, "changed %s" % (box,))
        r.key(K["RETURN"])           # OK
        time.sleep(2)
        gone = r.shot("s4-ok")
        left = ImageChops.difference(Image.open(desk).convert("RGB"), Image.open(gone).convert("RGB")).getbbox()
        check("OK takes it away", left is None, "left %s" % (left,))
    finally:
        r.stop()
    log = open(os.path.join(work, "hatari.log"), errors="replace").read()
    check("nothing checked", "EMAILCHK" not in log)
    out = subprocess.run([sys.executable, os.path.join(HERE, "acc_trace.py"), "--none", tr],
                         capture_output=True, text=True).stdout
    print(out, end="")
    check("no memory taken, no file opened", "\nNONE:" in "\n" + out)
    print("screenshots in", os.path.join(work, "shots"))
    print("EMail check test (1 MB):", "all passed" if not bad else "%d failed" % len(bad))
    sys.exit(1 if bad else 0)


if small:
    small_machine()
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
shutil.copy(os.environ.get("CHK_ACC", os.path.join(ROOT, "binary", "emailchk.acc")),
            os.path.join(hd, "emailchk.acc"))
rig.fakesting(os.path.join(hd, "AUTO"))
if busy:
    shutil.copy(os.path.join(ROOT, "build", "fakeplay.acc"), os.path.join(hd, "fakeplay.acc"))
if direct:
    # the full Mozilla list, the test certificate with it, decoded here
    with open(os.path.join(hd, "cacert.pem"), "wb") as f:
        f.write(open(os.path.join(ROOT, "binary", "cacert.pem"), "rb").read() + open(cert, "rb").read())
    subprocess.run([os.path.join(ROOT, "build", "mkroots"), os.path.join(hd, "cacert.pem"),
                    os.path.join(hd, "roots.dat")], check=True)
with open(os.path.join(em, "email.inf"), "w", newline="") as f:
    # tz=0 for a Direct connection: the certificate's dates against the
    # emulator's clock, this computer's (UTC)
    f.write("[options]\r\nnatfeats=1\r\ntz=%d\r\ncheck=1\r\nlog=1\r\nfalcon=%d\r\n" % (0 if direct else 180, direct)
            + ("bell=0\r\n" if bell_off else "") + "[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
            "email=dana@test.local\r\nin=%s\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
            "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n"
            "dhost=127.0.0.1\r\ndport=%d\r\ndsec=1\r\n"
            % ("pop3" if pop else "imap", 10110 if pop else IMAP, SMTP, IMAPS))
with open(os.path.join(hd, "emailchk.inf"), "w", newline="") as f:
    f.write("FOLDER=C:\\\r\nSTART=now\r\nNATFEATS=1\r\n")

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


if held:
    # the held case (guidelines 2.2; ECal's held boot case): the
    # accessory's memory is its own on plain TOS. FILLMEM.PRG (no fast-load
    # bit) auto-started at boot: first alone, the reference, filling what
    # it is given; then holding the front in an event call for 30 s (the
    # accessory's first turns come while it is the current process), and
    # after it ends, again, filling. The second gets no more than the
    # reference within 4 KB; mail comes, and the next two checks find it
    shutil.copy(os.path.join(ROOT, "build", "fillmem.prg"), os.path.join(hd, "FILLMEM.PRG"))
    flog = os.path.join(hd, "FILLMEM.LOG")

    def boot(hold, n):
        for f in (flog, os.path.join(hd, "HOLD.ON")):
            if os.path.exists(f):
                os.remove(f)
        if hold:
            open(os.path.join(hd, "HOLD.ON"), "w").close()
        r = rig.Rig(work, machine, serial=(fifo_out, fifo_in), auto="C:\\FILLMEM.PRG", extra=traced(n))
        end = time.time() + 300
        while time.time() < end and "filled" not in (open(flog, errors="replace").read() if os.path.exists(flog) else ""):
            time.sleep(2)
        log = open(flog, errors="replace").read().splitlines() if os.path.exists(flog) else []
        got = [int(l.split()[1]) for l in log if l.startswith("filled ")]
        return r, log, got[0] if got else 0

    r, log, ref = boot(False, 1)
    r.stop()
    check("the reference boot: FILLMEM.PRG alone filled what it was given", ref > 0, "; ".join(log))
    r, log, got = boot(True, 2)
    try:
        check("held: FILLMEM.PRG held the front, ended, and ran again", len(log) == 2 and log[0] == "held, then itself",
              "; ".join(log))
        check("... and was given no more than the reference (within 4 KB)", 0 < got <= ref + 4096,
              "%d against %d, %+d" % (got, ref, got - ref))
        time.sleep(5)
        n = len(checks())
        deliver(("()", "After the programs"))
        line = wait_check(n + 1, 150)
        stopped = [l for l in console().splitlines() if "Bus Error" in l or "Address Error" in l or "Halt" in l]
        if stopped:
            # the machine stopped (2.30's accessory: its heap filled with
            # 0xA5, then used): a halted Hatari answers no more commands
            check("the machine runs on", False, stopped[0])
            r.proc.kill()
            raise SystemExit
        check("after them, the next check still finds the new one",
              re.search(r": 1 new, fresh$", line) is not None, line)
        time.sleep(3)
        r.shot("h1-told")
        r.key(K["RETURN"])
        time.sleep(2)
        n = len(checks())
        deliver(("()", "And one more"))
        line = wait_check(n + 1, 150)
        check("... and the one after it", re.search(r": 2 new, fresh$", line) is not None, line)
        r.shot("h2-told")
    except SystemExit:
        pass
    finally:
        r.stop()
    crash = [f for f in os.listdir(hd) if f.upper() == "CRASH.LOG"]
    check("no crash", not crash)
    print("\n".join(checks()))
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
    print("screenshots in", os.path.join(work, "shots"))
    print("EMail check test (held):", "all passed" if not fails else "%d failed" % len(fails))
    sys.exit(1 if fails else 0)

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
    # the bell with the news (decided 27 Nov 2026): DMA where the machine
    # has it (TT, Falcon), the YM chip on an ST
    rang = [l for l in console().splitlines() if l.startswith("BELL ")]
    want = "BELL ym" if machine.partition(":")[0].startswith("ST") else "BELL dma"
    if busy:
        want = "BELL silent: busy"
    elif bell_off:
        want = "BELL silent: off"
    check("... with the bell (%s)" % want, rang == [want], "; ".join(rang) or "none")
    r.shot("c2-told")
    check("... and the server's flags are as they were", unseen() == before + 2, "%d unread" % unseen())
    r.key(K["RETURN"])               # OK
    time.sleep(2)

    # 3. nothing more: not told again; one more: told
    line = wait_check(3, 150)
    check("checked again with nothing more: not told again", re.search(r": %d new$" % NEW2, line) is not None, line)
    check("... and no bell", len([l for l in console().splitlines() if l.startswith("BELL ")]) == 1)
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
    # on a 1 MB ST with TOS 1.04 and the accessory, EMail starts with its
    # "Only 256 KB of memory is free" alert, as it should: OK, so that
    # Ctrl+Q below reaches EMail (Return does nothing in EMail's window)
    r.key(K["RETURN"])
    time.sleep(2)
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
