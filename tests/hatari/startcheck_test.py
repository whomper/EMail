#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Settings > Mail, "Check for new mail at start" (STARTCHECK= in
EMAIL.INF; Erez, 25 Oct 2026: off, EMail starts fast).

As net_test.py (Dovecot, FAKESTNG.PRG over the serial port), with a check
a minute (CHECK=1) and the protocol log on, so EMAIL.LOG says when EMail
connects:
1. STARTCHECK=0: EMail starts, opens the Inbox from disk and doesn't
   connect; the timed check, a minute later, does.
2. STARTCHECK=1 (as before): EMail connects as it starts.

    startcheck_test.py WORKDIR [MACHINE]
"""
import os
import shutil
import subprocess
import sys
import time

import rig

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

os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd)
rig.fakesting(os.path.join(hd, "AUTO"))
log = os.path.join(hd, "EMAIL.LOG")
fails = []


def check(what, good, detail=""):
    print("  %s %s%s" % ("ok  " if good else "FAIL", what, ("  (" + detail + ")") if detail else ""), flush=True)
    if not good:
        fails.append(what)


def inf(startcheck):
    with open(os.path.join(hd, "EMAIL.INF"), "w", newline="") as f:
        f.write("[options]\r\ntz=180\r\ncheck=1\r\nlog=1\r\n%s[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
                "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
                "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n"
                % ("" if startcheck is None else "startcheck=%d\r\n" % startcheck, IMAP, SMTP))


def connects():
    if not os.path.exists(log):
        return 0
    return open(log, errors="replace").read().count("IMAP -- connecting")


def wait_connect(n, most):
    end = time.time() + most
    while time.time() < end:
        if connects() >= n:
            return True
        time.sleep(2)
    return connects() >= n


fifo_out, fifo_in = os.path.join(work, "st_out"), os.path.join(work, "st_in")
for p in (fifo_out, fifo_in):
    if os.path.exists(p):
        os.remove(p)
    os.mkfifo(p)
bridge = rig.bridge(work, fifo_out, fifo_in)
shutil.rmtree(os.path.join(work, "shots"), ignore_errors=True)

# 1. off: no connection at the start; the timed check a minute later
inf(0)
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
try:
    r.settle(30, 120)
    started = time.time()
    r.shot("s1-started")
    check("STARTCHECK=0: EMail starts without connecting", connects() == 0, "%d connections" % connects())
    check("... the timed check (a minute) connects", wait_connect(1, 150),
          "after %d s" % (time.time() - started))
    r.shot("s2-timed")
finally:
    r.stop()

# 2. the default: EMail checks as it starts
os.remove(log)
inf(None)
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
try:
    check("STARTCHECK not given (as before): EMail connects as it starts", wait_connect(1, 90))
    r.shot("s3-default")
finally:
    r.stop()
bridge.terminate()
smtp.terminate()
print("screenshots in %s/shots" % work)
print("%d failed" % len(fails) if fails else "all passed")
sys.exit(1 if fails else 0)
