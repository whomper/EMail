#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EMail and ECal (the AERA guidelines, 4.1): an invitation, added to ECal.

As net_test.py (Dovecot, FAKESTNG.PRG over the serial port), with
build/fakecal.acc as C:\\ECAL.ACC: a stand-in for ECal that writes what
VA_START named into C:\\ECAL.LOG. The newest message in INBOX carries an
invitation as a text/calendar alternative. EMail opens it, and a
double-click on its "Invitation: ..." line writes INVITE.ICS next to
EMAIL.PRG and names it to ECal. The test checks the file, byte for byte, and what
"ECal" got. Then EMail starts again without the accessory: the same
double-click says ECal isn't running, and nothing is written.

    invite_test.py WORKDIR [MACHINE]  (make build/fakecal.acc first)
    invite_test.py WORKDIR [MACHINE] ECal|vX.Y   (ECal's latest release, or that one)

With ECal (or a version) after the machine, ECal's own ECAL.ACC from its
release stands in place of the stand-in: its New Event dialog must open
filled in, and Return must save the event, with the invitation's UID,
into ECal's folder.
"""
import base64
import os
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
real = bool(more)                       # ECal's release, not the stand-in
if machine.partition(":")[0] not in ("Falcon", "Falcon-4", "TT"):
    print("SKIP on %s: the clicks are placed for a 640x480 screen" % machine)
    sys.exit(0)
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
for k, v in (("@RUN@", run_dir), ("@DIR@", srv), ("@IMAP@", str(IMAP)), ("@POP3@", "10110"),
             ("@UID@", uid.decode()), ("@GID@", gid.decode())):
    conf = conf.replace(k, v)
with open(os.path.join(srv, "dovecot.conf"), "w") as f:
    f.write(conf)
for m in sorted(os.listdir(os.path.join(ROOT, "tests", "mail"))):
    shutil.copy(os.path.join(ROOT, "tests", "mail", m), os.path.join(MD, "new", m))
ICS = ("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nMETHOD:REQUEST\r\nBEGIN:VEVENT\r\nUID:lunch-1@example.com\r\n"
       "DTSTART:20261023T120000Z\r\nDTEND:20261023T130000Z\r\nSUMMARY:Team lunch\r\n"
       "ORGANIZER;CN=Dana:mailto:dana@example.com\r\n"
       "ATTENDEE;CN=Dana Falcon;RSVP=TRUE:mailto:dana@test.local\r\n"
       "LOCATION:Tel Aviv\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n").encode()
with open(os.path.join(MD, "new", "4-invite.eml"), "wb") as f:
    f.write(b"From: Dana <dana@example.com>\r\nTo: dana@test.local\r\nSubject: Lunch\r\n"
            b"Date: Tue, 20 Oct 2026 09:00:00 +0300\r\nMessage-ID: <lunch-1@example.com>\r\nMIME-Version: 1.0\r\n"
            b"Content-Type: multipart/alternative; boundary=AA\r\n\r\n--AA\r\nContent-Type: text/plain\r\n\r\n"
            b"Lunch on Friday?\r\n--AA\r\nContent-Type: text/calendar; method=REQUEST; charset=utf-8\r\n"
            b"Content-Transfer-Encoding: base64\r\n\r\n" + base64.encodebytes(ICS).replace(b"\n", b"\r\n") +
            b"--AA--\r\n")
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

# drive C:, with the stand-in ECal
os.makedirs(os.path.join(hd, "AUTO"))
shutil.copy(os.path.join(ROOT, "binary", "EMAIL.PRG"), hd)
if real:
    ec_dir = os.path.join(work, "ecal")
    shutil.rmtree(ec_dir, ignore_errors=True)
    spec = "ECal" if more[0].lower() == "ecal" else "ECal@" + more[0]
    print("ECal", rig.release(spec, ec_dir, "ECAL.ACC"), flush=True)
    shutil.copy(os.path.join(ec_dir, "ECAL.ACC"), os.path.join(hd, "ECAL.ACC"))
    # an On My Atari calendar, made by cal-cli from the same release's
    # source, as ECal's own rigs do: no first-time questions
    src = os.path.join(ec_dir, "src")
    subprocess.run(["git", "-c", "advice.detachedHead=false", "clone", "-q", "--depth", "1", "--branch",
                    open(os.path.join(ec_dir, "ECAL.VERSION")).read().strip(), "https://github.com/whomper/ECal", src],
                   check=True)
    subprocess.run(["make", "-s", "-C", src, "build/cal-cli"], check=True, stdout=subprocess.DEVNULL)
    seed = os.path.join(ec_dir, "seed")
    for a in (("zone", "Asia/Jerusalem"), ("local", "Home")):
        subprocess.run([os.path.join(src, "build", "cal-cli"), seed] + list(a), check=True, stdout=subprocess.DEVNULL)
    folder = os.path.join(hd, "ECAL")
    os.makedirs(os.path.join(folder, "CACHE"))
    for n in ("ECAL.INF", "ECAL.KEY"):
        shutil.copy(os.path.join(seed, n), os.path.join(folder, n))
    for n in os.listdir(os.path.join(seed, "CACHE")):
        shutil.copy(os.path.join(seed, "CACHE", n), os.path.join(folder, "CACHE", n.upper()))
    with open(os.path.join(folder, "ECAL.INF"), "a") as f:
        f.write("sync_open=0\r\n")       # no network for it here (the accessory finds C:\\ECAL\\ itself)
else:
    shutil.copy(os.path.join(ROOT, "build", "fakecal.acc"), os.path.join(hd, "ECAL.ACC"))
rig.fakesting(os.path.join(hd, "AUTO"))
with open(os.path.join(hd, "EMAIL.INF"), "w", newline="") as f:
    f.write("[options]\r\ntz=180\r\n[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
            "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n" % (IMAP, SMTP))
INVITE = os.path.join(hd, "INVITE.ICS")         # next to EMAIL.PRG, as ADDRESS.TXT is
LOG = os.path.join(hd, "ECAL.LOG")

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


def changed(a, b):
    from PIL import Image, ImageChops
    x, y = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    d = ImageChops.difference(x, y).convert("L").point(lambda v: 255 if v else 0)
    return d.histogram()[255] / float(x.size[0] * x.size[1])


# 1. with ECal: the invitation goes to it
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
try:
    r.settle(45, 240)                    # boot, log in, mirror the inbox
    r.shot("i1-inbox")
    r.click(330, 108)                    # the newest: the invitation
    time.sleep(12)
    read = r.shot("i2-read")
    r.click(330, 324, double=True)       # "» Invitation: Team lunch"
    time.sleep(10 if real else 6)
    sent = r.shot("i3-to-ecal")
    if real:                             # ECal's New Event, filled in from it: Return adds it
        check("ECal's New Event dialog opened", changed(read, sent) > 0.05,
              "%.0f%% of the screen" % (100 * changed(read, sent)))
        r.key(K["RETURN"])
        time.sleep(6)
        r.shot("i4-added")
        r.key(K["ESC"])                  # ECal's window, if it stayed up
        time.sleep(2)
        r.click(320, 200)                # back to EMail
        time.sleep(2)
    r.ctrl("q")
    time.sleep(5)
finally:
    r.stop()
data = open(INVITE, "rb").read() if os.path.exists(INVITE) else b""
check("INVITE.ICS is the invitation, byte for byte", data == ICS, "%d bytes, %d sent" % (len(data), len(ICS)))
if real:
    saved = []
    for d, _, fs in os.walk(os.path.join(hd, "ECAL")):
        for n in fs:
            t = open(os.path.join(d, n), "rb").read()
            if b"lunch-1@example.com" in t:
                saved.append((n, b"METHOD:" in t))
    check("ECal saved the event with the invitation's UID", bool(saved), ", ".join(n for n, _ in saved) or "not found")
    check("... without the invitation's METHOD", saved and not any(m for _, m in saved))
else:
    log = open(LOG, errors="replace").read() if os.path.exists(LOG) else ""
    check("ECal was named it with VA_START", "start C:\\INVITE.ICS, %d bytes, SUMMARY:Team lunch" % len(ICS) in log,
          log.strip().replace("\r\n", " | ").replace("\r", ""))

# 2. without ECal: it says so, and writes nothing
os.remove(os.path.join(hd, "ECAL.ACC"))
if os.path.exists(INVITE):
    os.remove(INVITE)
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
try:
    r.settle(45, 240)
    r.click(330, 108)
    time.sleep(8)
    before = r.shot("j1-read")
    r.click(330, 324, double=True)
    time.sleep(4)
    alert = r.shot("j2-not-running")
    check("without ECal, an alert says so", changed(before, alert) > 0.02, "%.1f%% of the screen" % (100 * changed(before, alert)))
    r.key(K["RETURN"])
    time.sleep(2)
    r.ctrl("q")
    time.sleep(4)
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
check("... and no INVITE.ICS was written", not os.path.exists(INVITE))
print("screenshots in", os.path.join(work, "shots"))
print("invite test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
