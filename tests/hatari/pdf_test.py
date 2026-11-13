#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A PDF attachment: a click offers Open with EPDF, Save to disk or Cancel (Erez,
13 Nov 2026).

As invite_test.py (Dovecot, FAKESTNG.PRG over the serial port). The newest
message in INBOX has a PDF attached (REPORT.PDF).

1. With build/fakeepdf.acc as C:\\EPDF.ACC, a stand-in for a running EPDF
   that writes what VA_START gave it into C:\\EPDF.LOG: a click on the
   attachment's line asks, Return chooses Open with EPDF; EMail writes
   C:\\pdf\\report.pdf, byte for byte, and names it to "EPDF".
2. Without it, as on plain TOS, with build/fakeepdf.prg as C:\\EPDF\\EPDF.PRG
   (EMAIL.INF's EPDF= names it; it writes its command line into
   C:\\EPDF.LOG): the click, Return, then Return on "EPDF isn't running":
   EMail quits and EPDF starts on the document.

    pdf_test.py WORKDIR [MACHINE]  (make build/fakeepdf.acc build/fakeepdf.prg first)

EMail runs alone (guidelines 1.2): the real EPDF is the hub's family check.
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

work, machine, _ = rig.cli("Falcon")
if machine.partition(":")[0] not in ("Falcon", "Falcon-4", "TT"):
    print("SKIPPED: the whole test on %s: the clicks are placed for a 640x480 screen" % machine)
    print("%s: 0 passed, 1 skipped" % os.path.basename(__file__))
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
PDF = b"%PDF-1.4\n1 0 obj << /Type /Catalog >> endobj\ntrailer << /Root 1 0 R >>\n%%EOF\n" + bytes(range(256))
with open(os.path.join(MD, "new", "4-pdf.eml"), "wb") as f:
    f.write(b"From: Avi <avi@example.com>\r\nTo: dana@test.local\r\nSubject: The report\r\n"
            b"Date: Tue, 20 Oct 2026 09:00:00 +0300\r\nMessage-ID: <report-1@example.com>\r\nMIME-Version: 1.0\r\n"
            b"Content-Type: multipart/mixed; boundary=MM\r\n\r\n--MM\r\nContent-Type: text/plain\r\n\r\n"
            b"The report is attached.\r\n--MM\r\nContent-Type: application/pdf; name=\"Report.pdf\"\r\n"
            b"Content-Disposition: attachment; filename=\"Report.pdf\"\r\n"
            b"Content-Transfer-Encoding: base64\r\n\r\n" + base64.encodebytes(PDF).replace(b"\n", b"\r\n") +
            b"--MM--\r\n")
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

# drive C:, with the stand-in EPDF
os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd)
shutil.copy(os.path.join(ROOT, "build", "fakeepdf.acc"), os.path.join(hd, "EPDF.ACC"))
os.makedirs(os.path.join(hd, "EPDF"))
shutil.copy(os.path.join(ROOT, "build", "fakeepdf.prg"), os.path.join(hd, "EPDF", "EPDF.PRG"))
rig.fakesting(os.path.join(hd, "AUTO"))
with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
    f.write("[options]\r\ntz=180\r\nepdf=C:\\EPDF\\EPDF.PRG\r\n[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
            "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n" % (IMAP, SMTP))
SAVED = os.path.join(hd, "pdf", "report.pdf")      # the names EMail gives are lowercase (6.6)
LOG = os.path.join(hd, "EPDF.LOG")
ATTACH = (330, int(os.environ.get("PDF_ATTACH_Y", "324")))    # the attachment's line in the reader

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


def log():
    return open(LOG, errors="replace").read() if os.path.exists(LOG) else ""


# 1. EPDF running: the document goes to it
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
try:
    r.settle(45, 240)                    # boot, log in, mirror the inbox
    r.click(330, 108)                    # the newest: the report
    time.sleep(12)
    before = r.shot("p1-read")
    r.click(*ATTACH)
    time.sleep(4)
    asked = r.shot("p2-asked")
    check("a click on the PDF asks", changed(before, asked) > 0.02, "%.1f%% of the screen" % (100 * changed(before, asked)))
    r.key(K["RETURN"])                   # Open with EPDF (the default)
    time.sleep(6)
    r.shot("p3-opened")
    r.ctrl("q")
    time.sleep(5)
finally:
    r.stop()
data = open(SAVED, "rb").read() if os.path.exists(SAVED) else b""
check("C:\\pdf\\report.pdf is the attachment, byte for byte", data == PDF, "%d bytes, %d sent" % (len(data), len(PDF)))
check("EPDF was named it with VA_START", "start C:\\pdf\\report.pdf" in log(), log().strip().replace("\r\n", " | "))

# 2. EPDF not running, plain TOS: EMail quits and starts it on the document
os.remove(os.path.join(hd, "EPDF.ACC"))
for p in (SAVED, LOG):
    if os.path.exists(p):
        os.remove(p)
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
try:
    r.settle(45, 240)
    r.click(330, 108)
    time.sleep(8)
    r.click(*ATTACH)
    time.sleep(4)
    r.key(K["RETURN"])                   # Open with EPDF (the default)
    time.sleep(4)
    r.shot("q1-not-running")
    r.key(K["RETURN"])                   # Start EPDF: EMail quits
    time.sleep(15)
    r.shot("q2-started")
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
check("... written again", os.path.exists(SAVED) and open(SAVED, "rb").read() == PDF)
check("EPDF.PRG started on it", "args C:\\pdf\\report.pdf" in log(), log().strip().replace("\r\n", " | "))
print("screenshots in", os.path.join(work, "shots"))
print("pdf test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
