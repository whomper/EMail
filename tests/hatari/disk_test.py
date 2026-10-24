#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EMail's disk use per job (the guidelines, 6.5), from Hatari's GEMDOS trace.

    disk_test.py WORKDIR [MACHINE]       (needs Xvfb, xdotool, dovecot)

Dovecot as in net_test.py: INBOX starts with the three sample messages,
Big holds 500. Jobs, each between two marks in the trace:
  A1  Check mail (^K) after 50 new messages arrive in INBOX: their headers
  A2  Folder > Download all messages on INBOX: the 50 (and 3) bodies
  B1  open Big for the first time (sync: the newest 100 headers)
  B2  open Big again from disk (Inbox in between)
Each job's opens and lookups (Fopen, Fcreate, Fsfirst, Fdelete, Frename,
Dcreate: each walks the FAT and the folder) are counted from EMail's own
calls and checked against its budget. Then a reset in the middle of an
append: the Inbox's pack is cut inside its last record while EMail runs,
the message is opened, and it must come from the server again, whole.
"""
import collections
import email.utils
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rig  # noqa: E402
from rig import K  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
IMAP, SMTP = 10143, 10025

work, machine, more = rig.cli("Falcon")
os.makedirs(work, exist_ok=True)
srv = os.path.join(work, "srv")
hd = os.path.join(work, "hd")
for d in (srv, hd):
    shutil.rmtree(d, ignore_errors=True)
run_dir = subprocess.run(["mktemp", "-d", "/tmp/tdv.XXXXXX"], capture_output=True, text=True).stdout.strip()
MD = os.path.join(srv, "home/dana/Maildir")
for d in ("state", "home/dana/Maildir/new", "home/dana/Maildir/cur", "home/dana/Maildir/tmp",
          "home/dana/Maildir/.Big/new", "home/dana/Maildir/.Big/cur", "home/dana/Maildir/.Big/tmp"):
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


def message(i, what):
    body = ("Line %d of a test message, long enough to look like mail.\r\n" % i) * 30
    date = email.utils.formatdate(1760000000 + i * 600)
    return ("From: Sender %d <s%d@example.com>\r\nTo: dana@test.local\r\nSubject: %s %d\r\n"
            "Date: %s\r\nMessage-ID: <%s%d@example.com>\r\nContent-Type: text/plain; charset=us-ascii\r\n\r\n%s"
            % (i, i, what, i, date, what, i, body))


for i in range(500):
    with open(os.path.join(MD, ".Big/cur/%d.big%d:2,S" % (1700000000 + i, i)), "w", newline="") as f:
        f.write(message(i, "Big"))
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd)
rig.fakesting(os.path.join(hd, "AUTO"))
with open(os.path.join(hd, "EMAIL.INF"), "w", newline="") as f:
    f.write("[options]\r\ntz=180\r\nlog=0\r\n[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
            "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n" % (IMAP, SMTP))

fifo_out, fifo_in = os.path.join(work, "st_out"), os.path.join(work, "st_in")
for p in (fifo_out, fifo_in):
    if os.path.exists(p):
        os.remove(p)
    os.mkfifo(p)
bridge = rig.bridge(work, fifo_out, fifo_in)
shutil.rmtree(os.path.join(work, "shots"), ignore_errors=True)
TR = os.path.join(work, "gemdos.trace")
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in), extra=["--trace", "gemdos", "--trace-file", TR])
marks = []


def index_records(folder):
    """{uid: (offset, length)} of the bodies INDEX.DAT has in the pack"""
    out = {}
    try:
        lines = open(os.path.join(folder, "INDEX.DAT"), encoding="latin-1").read().split("\n")[1:]
    except OSError:
        return out
    for line in lines:
        f = line.split("\t")
        if len(f) >= 9 and int(f[7]) >= 0:
            out[int(f[0])] = (int(f[7]), int(f[8]))
    return out


HLOG = os.path.join(work, "hatari.log")
logmarks = {}


def mark(name):
    time.sleep(1)
    marks.append((name, os.path.getsize(TR)))
    logmarks[name] = os.path.getsize(HLOG) if os.path.exists(HLOG) else 0


try:
    r.settle(45, 300)                    # boot, log in, mirror the inbox
    mark("start")
    r.shot("a0-start")
    for i in range(50):
        p = os.path.join(MD, "new", "%d.new%d" % (1760000000 + i, i))
        with open(p, "w", newline="") as f:
            f.write(message(i, "New"))
        os.chown(p, int(uid), int(gid))
    mark("A1 begin")
    r.ctrl("k")                          # check mail: 50 new headers
    time.sleep(40)
    r.shot("a1-checked")
    mark("A1 end")
    r.click(272, 9)                      # menu: Folder
    r.shot("a2-menu")
    mark("A2 begin")
    r.click(300, 125)                    # Download all messages
    time.sleep(2)
    r.shot("a2-ask")
    r.key(K["RETURN"])                   # Download
    time.sleep(90)
    r.shot("a2-downloaded")
    r.key(K["RETURN"])                   # "All 53 ... are on the Atari"
    mark("A2 end")
    mark("B1 begin")
    r.click(40, 201)                     # Big
    time.sleep(60)
    r.shot("b1-big")
    mark("B1 end")
    r.click(40, 105)                     # Inbox
    time.sleep(10)
    mark("B2 begin")
    r.click(40, 201)                     # Big again, from disk
    time.sleep(20)
    r.shot("b2-big-again")
    mark("B2 end")

    # a reset in the middle of an append: the Inbox's pack ends inside
    # its last record (the newest message's), as if the power went
    r.click(40, 105)                     # Inbox
    time.sleep(10)
    inbox = os.path.join(hd, "EMAIL", "ACCT1", "FD53E5C5")
    recs = index_records(inbox)
    last = max(recs.values(), key=lambda v: v[0]) if recs else None
    cut_uid = [u for u, v in recs.items() if v == last][0] if last else None
    if last:
        with open(os.path.join(inbox, "MESSAGES.DAT"), "r+b") as f:
            f.truncate(last[0] + 20)
    r.click(330, 156)                    # "New 49", the last downloaded: its record is the cut one
    time.sleep(15)
    r.shot("c1-read-again")
    r.ctrl("q")                          # quit: what came is saved
    time.sleep(8)
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
json.dump(marks, open(os.path.join(work, "marks.json"), "w"))

# ---- the counts ----
OPS = {"3D": "Fopen", "3C": "Fcreate", "4E": "Fsfirst", "41": "Fdelete", "56": "Frename", "39": "Dcreate"}
BUDGET = {"A1": 6, "A2": 5, "B1": 8, "B2": 1}
WHAT = {"A1": "Check mail, 50 new in the open Inbox (headers)",
        "A2": "Download all messages, 53 bodies",
        "B1": "open a 500-message folder the first time (100 headers)",
        "B2": "open it again, from disk"}
trace = open(TR, "rb").read()
at = dict(marks)
fails = 0


def pack_writes(job):
    """the bytes the job wrote to the packs"""
    fd, n = {}, 0
    last = None
    for line in trace[at[job + " begin"]:at[job + " end"]].decode("latin-1").splitlines():
        m = re.match(r'GEMDOS 0x(3D|3C) \w+\("([^"]*)"', line)
        if m:
            last = m.group(2)
            continue
        m = re.match(r"-> FD (\d+)", line)
        if m and last:
            fd[m.group(1)] = last
            last = None
            continue
        m = re.match(r"GEMDOS 0x40 Fwrite\((\d+), (\d+),", line)
        if m and fd.get(m.group(1), "").endswith("MESSAGES.DAT"):
            n += int(m.group(2))
    return n


# A2 goes to disk a buffer at a time: as many appends as its bytes need
# buffers, the buffer's size as EMail says it (a quarter of the free ST
# RAM on a 1 MB ST, 256 KB of TT RAM)
log = open(HLOG, "rb").read()[logmarks["A2 begin"]:logmarks["A2 end"]].decode("latin-1") if os.path.exists(HLOG) else ""
bufs = [int(b) for b in re.findall(r"EMail: pack buffer (\d+) bytes", log)]
wrote = pack_writes("A2")
if bufs and bufs[0] > 0:
    pieces = -(-wrote // bufs[0])
    BUDGET["A2"] += max(pieces, 1) - 1
    print("  A2 wrote %d bytes to the pack through a %d-byte buffer: %d piece(s)" % (wrote, bufs[0], max(pieces, 1)))
else:
    print("  A2: EMail didn't say its buffer size (%d bytes written)" % wrote)
OTHERS = re.compile(r"^[A-Z]:\\(ECAL|ECONTACT|EHYPVIEW|EFTP)(\\|\.|$)", re.I)
for job in ("A1", "A2", "B1", "B2"):
    count = collections.Counter()
    files = collections.Counter()
    for line in trace[at[job + " begin"]:at[job + " end"]].decode("latin-1").splitlines():
        m = re.match(r"GEMDOS 0x([0-9A-F]{2}) \w+\((.*)\) at PC[= ]0x([0-9A-F]+)", line)
        if not m or m.group(1) not in OPS or 0xE00000 <= int(m.group(3), 16) < 0x1000000:
            continue                     # TOS's own, from ROM
        path = re.findall(r'"([^"]*)"', m.group(2))
        if path and path[0].endswith("FASTCLK"):
            continue                     # FAKESTNG.PRG's, the test's network
        if path and OTHERS.match(path[0]):
            continue                     # the family's accessories (AERA_WITH_ACCS): theirs, not EMail's
        count[OPS[m.group(1)]] += 1
        files[re.sub(r"^C:\\EMAIL\\ACCT1\\", "", path[0]) if path else "?"] += 1
    total = sum(count.values())
    ok = total <= BUDGET[job]
    fails += not ok
    print("  %s %s: %d opens and lookups (budget %d): %s" % ("ok  " if ok else "FAIL", WHAT[job], total, BUDGET[job],
          ", ".join("%s %d" % kv for kv in sorted(count.items())) or "none"))
    for f, n in sorted(files.items(), key=lambda kv: -kv[1]):
        print("         %3d  %s" % (n, f))

# ---- the reset ----
recs = index_records(os.path.join(hd, "EMAIL", "ACCT1", "FD53E5C5"))
pack = open(os.path.join(hd, "EMAIL", "ACCT1", "FD53E5C5", "MESSAGES.DAT"), "rb").read()
rec = recs.get(cut_uid) if cut_uid else None
whole = bool(rec) and pack[rec[0]:rec[0] + 2] == b"EM" and \
    struct.unpack(">II", pack[rec[0] + 2:rec[0] + 10]) == (cut_uid, rec[1]) and \
    (rec[0] + 10 + rec[1] == len(pack) or pack[rec[0] + 10 + rec[1]:rec[0] + 12 + rec[1]] == b"EM")
body = pack[rec[0] + 10:rec[0] + 10 + rec[1]] if rec else b""
ok = whole and b"Subject: New 49" in body
fails += not ok
print("  %s a pack cut inside a record: the message came from the server again, whole" % ("ok  " if ok else "FAIL"))
print("screenshots in", os.path.join(work, "shots"))
print("PASS" if not fails else "FAIL")
sys.exit(1 if fails else 0)
