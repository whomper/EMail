#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Save attachment as through the family's file dialog (guidelines 5.12,
decided 25 Nov 2026), as net_test.py: Dovecot, FAKESTNG.PRG over the
serial port, a message with NOTES.DAT attached.

1. A click on the attachment's line opens the family's dialog ("Save
   attachment as", NOTES.DAT proposed, the type DAT files): the screen
   changes; Save writes C:\\NOTES.DAT, the attachment byte for byte.
2. The same again: Save asks whether to replace it (the screen changes),
   Return is Cancel there and the dialog stays; Cancel closes it, the file
   as it was.
3. With Settings' "Use the system file selector" (SYSFSEL=1 in EMAIL.INF):
   the AES's own selector opens in its place (its screen unlike the
   family dialog's), and Return there writes C:\\NOTES.DAT again.

The clicks are placed for a 640x480 screen (Falcon, TT).

    save_test.py WORKDIR [MACHINE]     (needs Xvfb, xdotool, dovecot)
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
uid = subprocess.check_output(["id", "-u", "nobody"]).strip().decode()
gid = subprocess.check_output(["id", "-g", "nobody"]).strip().decode()
conf = open(os.path.join(ROOT, "tests", "dovecot.conf.in")).read()
for k, v in (("@RUN@", run_dir), ("@DIR@", srv), ("@IMAP@", str(IMAP)), ("@POP3@", "10110"),
             ("@UID@", uid), ("@GID@", gid)):
    conf = conf.replace(k, v)
with open(os.path.join(srv, "dovecot.conf"), "w") as f:
    f.write(conf)
for m in sorted(os.listdir(os.path.join(ROOT, "tests", "mail"))):
    shutil.copy(os.path.join(ROOT, "tests", "mail", m), os.path.join(MD, "new", m))
DATA = bytes(range(256)) * 3 + b"the end\r\n"
with open(os.path.join(MD, "new", "4-notes.eml"), "wb") as f:
    f.write(b"From: Avi <avi@example.com>\r\nTo: dana@test.local\r\nSubject: The notes\r\n"
            b"Date: Tue, 20 Oct 2026 09:00:00 +0300\r\nMessage-ID: <notes-1@example.com>\r\nMIME-Version: 1.0\r\n"
            b"Content-Type: multipart/mixed; boundary=MM\r\n\r\n--MM\r\nContent-Type: text/plain\r\n\r\n"
            b"The notes are attached.\r\n--MM\r\nContent-Type: application/octet-stream; name=\"Notes.dat\"\r\n"
            b"Content-Disposition: attachment; filename=\"Notes.dat\"\r\n"
            b"Content-Transfer-Encoding: base64\r\n\r\n" + base64.encodebytes(DATA).replace(b"\n", b"\r\n") +
            b"--MM--\r\n")
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd)
rig.fakesting(os.path.join(hd, "AUTO"))
INF = ("[options]\r\nnatfeats=1\r\ntz=180\r\n%s[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
       "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
       "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n")
ATTACH = (330, int(os.environ.get("SAVE_ATTACH_Y", "324")))   # the attachment's line in the reader
SAVE, CANCEL = (440, 401), (330, 401)                          # the dialog's buttons

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


def saved():
    for f in os.listdir(hd):
        if f.lower() == "notes.dat":
            p = os.path.join(hd, f)
            return p, open(p, "rb").read(), os.path.getmtime(p)
    return None, b"", 0


def boot(sysfsel):
    with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
        f.write(INF % ("sysfsel=1\r\n" if sysfsel else "", IMAP, SMTP))
    r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
    r.settle(45, 240)                    # boot, log in, mirror the inbox
    r.click(330, 108)                    # the newest: the notes
    time.sleep(12)
    return r


# 1 and 2: the family's dialog
r = boot(False)
try:
    before = r.shot("s1-read")
    r.click(*ATTACH)
    time.sleep(4)
    dlg = r.shot("s2-dialog")
    check("a click on the attachment opens the family's dialog", changed(before, dlg) > 0.05,
          "%.1f%% of the screen" % (100 * changed(before, dlg)))
    r.click(*SAVE)
    time.sleep(8)
    r.shot("s3-saved")
    path, data, t1 = saved()
    check("Save writes C:\\NOTES.DAT, the attachment byte for byte", data == DATA,
          "%s, %d bytes of %d" % (path, len(data), len(DATA)))
    r.click(*ATTACH)
    time.sleep(4)
    again = r.shot("s4-again")
    r.click(*SAVE)
    time.sleep(3)
    ask = r.shot("s5-replace")
    check("... Save again asks to replace it", changed(again, ask) > 0.01,
          "%.1f%% of the screen" % (100 * changed(again, ask)))
    r.key(K["RETURN"])                   # Cancel there: the dialog stays
    time.sleep(3)
    stays = r.shot("s6-stays")
    check("... Return there is Cancel: the dialog stays", changed(again, stays) < 0.02,
          "%.1f%% unlike the dialog" % (100 * changed(again, stays)))
    r.click(*CANCEL)
    time.sleep(4)
    r.shot("s7-closed")
    _, data2, t2 = saved()
    check("... Cancel closes it, the file as it was", data2 == DATA and t2 == t1)
    r.ctrl("q")
    time.sleep(5)
finally:
    r.stop()

# 3: the system's selector
for f in os.listdir(hd):
    if f.lower() == "notes.dat":
        os.remove(os.path.join(hd, f))
r = boot(True)
try:
    before = r.shot("s8-read")
    r.click(*ATTACH)
    time.sleep(4)
    sel = r.shot("s9-selector")
    check("with the setting on, a selector opens", changed(before, sel) > 0.05,
          "%.1f%% of the screen" % (100 * changed(before, sel)))
    check("... the AES's own, not the family's dialog", changed(dlg, sel) > 0.05,
          "%.1f%% unlike the family's" % (100 * changed(dlg, sel)))
    r.key(K["RETURN"])
    time.sleep(8)
    r.shot("s10-saved")
    path, data, _ = saved()
    check("... Return there writes C:\\NOTES.DAT, byte for byte", data == DATA,
          "%s, %d bytes of %d" % (path, len(data), len(DATA)))
    r.ctrl("q")
    time.sleep(5)
finally:
    r.stop()
crash = any(f.lower() == "crash.log" for f in os.listdir(hd))
check("no CRASH.LOG", not crash)
print("screenshots in", os.path.join(work, "shots"))
print("save test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
