#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""File > Print... (^P) through the GDOS (Erez, 21 Nov 2026).

Dovecot holds two messages: a long letter (a hundred numbered lines and a
Hebrew one: two pages) and a short note with an attachment (one page).
EMail (Falcon-4:256 with Erez's NVDI by default) shows the long one; ^P
prints it; then ^A selects both and ^P prints each, from a new page, in
one job. Erez's NVDI with ASSIGN.SYS's printer (21) pointed at NVDI's
metafile driver (META.SYS) in this run's C: only, and no WDIALOG, so the
job goes to the default printer without the dialog: C:\\GEMFILE.GEM holds
each page and each text drawn, read back here (the pages, the header
with its labels apart, the Hebrew line to the right, "Page n of m", the
attachment line). On a machine without a GDOS, ^P gives the alert and
prints nothing (PRINT_NOGDOS=1). PRINT_WDIALOG=1 adds Erez's WDIALOG.PRG
after NVDI: ^P then shows its printer dialog, and Return prints.

    print_test.py WORKDIR [MACHINE]     (needs Xvfb, xdotool, dovecot)
"""
import atexit
import os
import shutil
import struct
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
wdialog = bool(os.environ.get("PRINT_WDIALOG"))     # Erez's WDIALOG.PRG too: its dialog, Return
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

# the printer (21) is NVDI's metafile driver, in this run's ASSIGN.SYS only:
# it writes C:\\GEMFILE.GEM, each text and each page in it
# (Erez's own is copied in by machines.py and taken out again at the end)
_nvdi = rig.machines.Machine.nvdi


def nvdi_img(self):
    ok = _nvdi(self)
    a = os.path.join(self.hd, "ASSIGN.SYS")
    if ok and os.path.exists(a):
        t = open(a, "rb").read().replace(b"21  PCL.SYS", b"21  META.SYS")
        open(a, "wb").write(t)
    wd = os.path.join(os.environ.get("AERA_NVDI", os.path.expanduser("~/.aera/nvdi/x")), "auto", "wdialog.prg")
    if ok and wdialog and os.path.exists(wd):
        to = os.path.join(self.hd, "AUTO", "WDIALOG.PRG")
        shutil.copy(wd, to)                     # after NVDI.PRG; Erez's copy, never left in a kept hd
        atexit.register(lambda: os.path.exists(to) and os.remove(to))
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


def gemfile():
    return rig.ci_path(hd, "GEMFILE.GEM")


def wait_job(was, secs=300):
    """GEMFILE.GEM written again (its time) and no longer growing"""
    f, last, still, t0 = gemfile(), -1, 0, time.time()
    while time.time() - t0 < secs:
        time.sleep(5)
        if not os.path.exists(f) or os.path.getmtime(f) == was:
            continue
        n = os.path.getsize(f)
        still = still + 1 if n == last else 0
        last = n
        if still >= 2:
            return True
    return False


def metafile(path):
    """a GEM metafile's pages (each v_clrwk starts one) and its texts: (page, x, y, bytes) for each
    v_gtext; the words are PC GEM's, little-endian"""
    d = open(path, "rb").read()
    w = lambda i: struct.unpack_from("<h", d, i)[0]
    i, page, texts = w(2) * 2, 1, []
    while i + 8 <= len(d):
        op, np_, ni, _ = struct.unpack_from("<hhhh", d, i)
        i += 8
        pts = struct.unpack_from("<%dh" % (2 * np_), d, i)
        i += 4 * np_
        ints = struct.unpack_from("<%dh" % ni, d, i)
        i += 2 * ni
        if op == -1:
            break
        if op == 3:
            page += 1
        elif op == 8:
            texts.append((page, pts[0], pts[1], bytes(c & 0xff for c in ints)))
    return page, texts


def has(texts, page, words):
    return any(p == page and words in t for p, _, _, t in texts)


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
        check("no GDOS: nothing printed", not os.path.exists(gemfile()))
    else:
        r.ctrl("p")
        if wdialog:
            time.sleep(6)
            r.shot("p3-dialog")              # WDIALOG's: the printer, the pages, the copies
            r.key(K["RETURN"])
        ok1 = wait_job(None)
        r.shot("p3-printed")
        check("the long letter printed (GEMFILE.GEM)", ok1)
        if ok1:
            shutil.copy(gemfile(), os.path.join(work, "one.gem"))
            pages, t = metafile(gemfile())
            print("  one message: %d pages, %d texts" % (pages, len(t)))
            check("two pages", pages == 2)
            check("its header, the labels apart: From:, Cc:, Subject: Long letter",
                  has(t, 1, b"From:") and has(t, 1, b"Cc:") and has(t, 1, b"Subject:") and has(t, 1, b" Long letter"))
            heb = [x for p, x, _, s in t if p == 1 and any(0xC2 <= c <= 0xDC for c in s)]
            left = min(x for p, x, _, _ in t if p == 1)
            check("the Hebrew line, to the right", bool(heb) and heb[0] > left)
            check("its text on both pages, the last line on the second",
                  has(t, 1, b"Line 1 of the long") and has(t, 2, b"Line 100 of the long"))
            check("Page 1 of 2, Page 2 of 2", has(t, 1, b"Page 1 of 2") and has(t, 2, b"Page 2 of 2"))
        was = os.path.getmtime(gemfile()) if ok1 else None
        r.ctrl("a")
        time.sleep(3)
        r.ctrl("p")
        if wdialog:
            time.sleep(6)
            r.shot("p4-dialog")
            r.key(K["RETURN"])
        ok2 = wait_job(was)
        r.shot("p4-printed-both")
        check("both selected printed", ok2)
        if ok2:
            shutil.copy(gemfile(), os.path.join(work, "both.gem"))
            pages, t = metafile(gemfile())
            print("  both: %d pages, %d texts" % (pages, len(t)))
            check("three pages, each message from a new one", pages == 3)
            check("the long letter first, then the note on a page of its own",
                  has(t, 1, b" Long letter") and has(t, 3, b" Short note") and not has(t, 2, b"Short note"))
            check("the note's attachment by name and size",
                  has(t, 3, b"plan.txt") and has(t, 3, b"(text/plain, 8 bytes)"))
            check("its page counted on its own: Page 1 of 1", has(t, 3, b"Page 1 of 1"))
    check("no CRASH.LOG", not any(n.lower() == "crash.log" for _, _, fs in os.walk(hd) for n in fs))
finally:
    r.stop()
    bridge.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
ok = checks and all(checks)
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
