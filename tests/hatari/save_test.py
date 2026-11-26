#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Save attachment as through the family's file dialog (guidelines 5.12,
decided 25 Nov 2026), as net_test.py: Dovecot, FAKESTNG.PRG over the
serial port, a message with NOTES.DAT attached.

1. A click on the attachment's line opens the family's dialog ("Save the
   attachment as", NOTES.DAT proposed, the type DAT files): the screen
   changes; Return, the name typed, saves C:\\NOTES.DAT, the attachment
   byte for byte, and three clicks in EMail's window after it each act (a
   mouse button left held after the dialog would lose them: EWeb 1.27). Open again: a click on the list's first row, the folder
   ATTACH, gives the list the keys, and Return goes into it (the folder
   line changes, the name stays); Save writes C:\\ATTACH\\NOTES.DAT, byte
   for byte.
2. The same again (the dialog opens in ATTACH, where it was left): Save
   asks whether to replace it (the screen changes),
   Return is Cancel there and the dialog stays; Cancel closes it, the file
   as it was; once more, Replace there writes it again.
3. With Settings' "Use the system file selector" (SYSFSEL=1 in EMAIL.INF):
   the AES's own selector opens in its place (its screen unlike the
   family dialog's), and Return there writes C:\\NOTES.DAT (a new start
   begins in EMail's folder; the first part's C:\\NOTES.DAT is taken away
   before it).

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
os.makedirs(os.path.join(hd, "ATTACH"))         # the folder chosen in the dialog, the list's first
rig.install_prg(hd)
rig.fakesting(os.path.join(hd, "AUTO"))
INF = ("[options]\r\nnatfeats=1\r\ntz=180\r\n%s[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
       "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
       "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n")
# the attachment's line in the reader: a TT's window sits 10 pixels higher
ATTACH = (330, int(os.environ.get("SAVE_ATTACH_Y", "314" if machine.startswith("TT") else "324")))
SAVE, CANCEL, REPLACE = (440, 401), (330, 401), (288, 272)    # the dialog's buttons, the question's Replace
FIRST_ROW = (230, 206)                                         # the list's first row

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


def saved(sub=""):
    d = rig.ci_path(hd, sub) if sub else hd
    if not os.path.isdir(d):
        return None, b"", 0
    for f in os.listdir(d):
        if f.lower() == "notes.dat":
            p = os.path.join(d, f)
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
    r.key(K["RETURN"])                   # the name typed: Return saves
    time.sleep(8)
    r.shot("s2a-saved-root")
    path, data, t0 = saved()
    check("Return saves C:\\NOTES.DAT, the attachment byte for byte", data == DATA,
          "%s, %d bytes of %d" % (path, len(data), len(DATA)))
    # after the dialog closed by Return, three clicks in EMail's window, each
    # acting (a button TOS 4.04 kept held would lose them: EWeb 1.27's gate)
    prev = r.shot("s2a1-before-clicks")
    for k, (y, what) in enumerate(((124, "the second row"), (140, "the third row"), (108, "the first row again"))):
        r.click(330, y)
        time.sleep(4)
        now = r.shot("s2a%d-click" % (k + 2))
        check("... then a click on %s acts" % what, changed(prev, now) > 0.005,
              "%.1f%% of the screen" % (100 * changed(prev, now)))
        prev = now
    r.click(*ATTACH)
    time.sleep(4)
    dlg = r.shot("s2b-dialog-again")
    r.click(*FIRST_ROW)                  # the folder ATTACH: the list has the keys
    time.sleep(2)
    r.key(K["RETURN"])                   # into it
    time.sleep(3)
    into = r.shot("s2c-into-attach")
    check("... a click on the first row, a folder, then Return goes into it", 0.001 < changed(dlg, into) < 0.2,
          "%.1f%% of the screen" % (100 * changed(dlg, into)))
    r.click(*SAVE)
    time.sleep(8)
    r.shot("s3-saved")
    path, data, t1 = saved("ATTACH")
    check("Save writes C:\\ATTACH\\NOTES.DAT, the attachment byte for byte", data == DATA,
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
    _, data2, t2 = saved("ATTACH")
    check("... Cancel closes it, the file as it was", data2 == DATA and t2 == t1)
    check("... C:\\NOTES.DAT left as it was meanwhile", saved()[2] == t0)
    time.sleep(2)
    r.click(*ATTACH)                     # and once more: Save, then Replace
    time.sleep(4)
    r.click(*SAVE)
    time.sleep(3)
    r.click(*REPLACE)
    time.sleep(8)
    r.shot("s7b-replaced")
    _, data3, t3 = saved("ATTACH")
    check("... Replace writes it again", data3 == DATA and t3 > t2, "%.0f s later" % (t3 - t2))
    r.ctrl("q")
    time.sleep(5)
finally:
    r.stop()

# 3: the system's selector, with no C:\\NOTES.DAT to replace
if saved()[0]:
    os.remove(saved()[0])
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
