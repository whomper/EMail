#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""File > Print... (^P) through the GDOS (Erez, 21 Nov 2026).

Dovecot holds two messages: a long letter (a hundred numbered lines and a
Hebrew one: two pages) and a short note with an attachment (one page).
EMail (Falcon-4:256 with Erez's NVDI by default) shows the long one; ^P
prints it; then ^A selects both and ^P prints each, from a new page, in
one job. Erez's NVDI with ASSIGN.SYS's printer (21) pointed at NVDI's
picture driver (IMG.SYS) in this run's C: only, and no WDIALOG, so the
job goes to the default printer without the dialog: each page an IMG
file on C:, checked for its size and its ink. On a machine without a
GDOS, ^P gives the alert and prints nothing (PRINT_NOGDOS=1: the shot).

    print_test.py WORKDIR [MACHINE]     (needs Xvfb, xdotool, dovecot)
"""
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

work, machine, more = rig.cli("Falcon-4:256")
nogdos = bool(os.environ.get("PRINT_NOGDOS"))
if not nogdos:
    os.environ["AERA_WITH_NVDI"] = "1"
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

new = os.path.join(srv, "home/dana/Maildir/new")
with open(os.path.join(new, "1-note.eml"), "w", newline="") as f:
    f.write("From: Avi <avi@test.local>\r\nTo: dana@test.local\r\nSubject: Short note\r\n"
            "Date: Thu, 20 Nov 2026 10:01:00 +0200\r\nMIME-Version: 1.0\r\n"
            "Content-Type: multipart/mixed; boundary=\"b\"\r\n\r\n"
            "--b\r\nContent-Type: text/plain\r\n\r\nThe plan is attached.\r\n"
            "--b\r\nContent-Type: text/plain; name=\"plan.txt\"\r\nContent-Disposition: attachment; filename=\"plan.txt\"\r\n\r\n"
            "one\r\ntwo\r\n--b--\r\n")
body = "".join("Line %d of the long letter, which goes on for a page and more.\r\n" % i for i in range(1, 101))
with open(os.path.join(new, "2-long.eml"), "wb") as f:
    f.write(("From: Ruth <ruth@test.local>\r\nTo: dana@test.local\r\nCc: avi@test.local\r\n"
             "Subject: Long letter\r\nDate: Thu, 20 Nov 2026 10:02:00 +0200\r\nMIME-Version: 1.0\r\n"
             "Content-Type: text/plain; charset=utf-8\r\nContent-Transfer-Encoding: 8bit\r\n\r\n"
             "שלום עולם\r\n" + body).encode("utf-8"))
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
rig.stop_at_exit(srv)

os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd, os.environ.get("PRINT_PRG"))
rig.fakesting(os.path.join(hd, "AUTO"))
with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
    f.write("[options]\r\nnatfeats=1\r\ntz=180\r\nlog=1\r\n[account]\r\nname=Dana\r\nfullname=Dana\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
            "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n" % (IMAP, SMTP))

# the printer (21) is NVDI's picture driver, in this run's ASSIGN.SYS only
# (Erez's own is copied in by machines.py and taken out again at the end)
_nvdi = rig.machines.Machine.nvdi


def nvdi_img(self):
    ok = _nvdi(self)
    a = os.path.join(self.hd, "ASSIGN.SYS")
    if ok and os.path.exists(a):
        t = open(a, "rb").read().replace(b"21  PCL.SYS", b"21  IMG.SYS")
        open(a, "wb").write(t)
    return ok


rig.machines.Machine.nvdi = nvdi_img

fifo_out, fifo_in = os.path.join(work, "st_out"), os.path.join(work, "st_in")
for p in (fifo_out, fifo_in):
    if os.path.exists(p):
        os.remove(p)
    os.mkfifo(p)
bridge = rig.bridge(work, fifo_out, fifo_in)
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
checks = []


def check(what, good):
    checks.append(good)
    print("  %s   %s" % ("ok" if good else "FAIL", what), flush=True)


def images():
    return sorted(os.path.join(dp, f) for dp, _, fs in os.walk(hd) for f in fs
                  if f.lower().endswith(".img") and "gemsys" not in dp.lower())


def wait_pages(had, secs=300):
    """the files the driver writes, until they stop coming"""
    last, still, t0 = -1, 0, time.time()
    while time.time() - t0 < secs:
        time.sleep(5)
        n = len(images())
        still = still + 1 if n == last and n > had else 0
        last = n
        if still >= 3:
            break
    return images()


def img_ink(path):
    """a GEM IMG file's width, height and how many of its bytes have ink"""
    d = open(path, "rb").read()
    hl, pat, w, h = (int.from_bytes(d[o:o + 2], "big") for o in (2, 6, 12, 14))
    bpl, i, rows, ink = (w + 7) // 8, hl * 2, 0, 0
    while rows < h and i < len(d):
        line, rep = bytearray(), 1
        while len(line) < bpl and i < len(d):
            c = d[i]
            if c == 0 and d[i + 1] == 0:
                rep, i = d[i + 3], i + 4
            elif c == 0:
                line += d[i + 2:i + 2 + pat] * d[i + 1]
                i += 2 + pat
            elif c == 0x80:
                line += d[i + 2:i + 2 + d[i + 1]]
                i += 2 + d[i + 1]
            else:
                line += (b"\xff" if c & 0x80 else b"\x00") * (c & 0x7f)
                i += 1
        ink += rep * sum(1 for b in line[:bpl] if b)
        rows += rep
    return w, h, ink


try:
    time.sleep(45)                       # boot, log in, mirror the inbox
    r.shot("p1-inbox")
    r.click(330, 108)                    # the newest: the long letter
    r.settle(10, 60)
    r.shot("p2-read")
    if nogdos:
        r.ctrl("p")
        time.sleep(4)
        r.shot("p3-alert")               # "Printing needs NVDI (or another GDOS)..."
        r.key(K["RETURN"])
        time.sleep(2)
        check("no GDOS: nothing printed", not images())
    else:
        r.ctrl("p")
        one = wait_pages(0)
        r.shot("p3-printed")
        print("  one message:", [os.path.relpath(p, hd) for p in one])
        check("the long letter: two pages (%d)" % len(one), len(one) == 2)
        r.ctrl("a")
        time.sleep(3)
        r.ctrl("p")
        both = wait_pages(len(one))
        r.shot("p4-printed-both")
        print("  both:", [os.path.relpath(p, hd) for p in both])
        check("both selected: three pages more, each message from a new page (%d)" % (len(both) - len(one)),
              len(both) - len(one) == 3)
        for p in both:
            w, h, ink = img_ink(p)
            print("  %s %dx%d, %d bytes of ink" % (os.path.relpath(p, hd), w, h, ink))
            check("%s has a page on it" % os.path.basename(p), w > 500 and h > 700 and ink > 1000)
        shutil.copytree(os.path.dirname(both[0]) if both else hd, os.path.join(work, "pages"),
                        ignore=lambda d, fs: [f for f in fs if not f.lower().endswith(".img")],
                        dirs_exist_ok=True)
    check("no CRASH.LOG", not any(n.lower() == "crash.log" for _, _, fs in os.walk(hd) for n in fs))
finally:
    r.stop()
    bridge.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
