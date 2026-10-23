#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Links in a message open in the web browser (EWeb).

As invite_test.py (Dovecot, FAKESTNG.PRG over the serial port). INBOX's
newest message is plain text with web addresses written in it (one in
a Hebrew sentence, drawn right to left), the next
an HTML message with an <a href> on "the news". The links are drawn in
the accent colour and underlined: the test finds a link by its
underline in the reader, and clicks it.

1. With build/fakeweb.acc as C:\\EWEB.ACC, a stand-in for a browser that
   is running (it writes what VA_START gave it into C:\\EWEB.LOG): each
   click hands it the link's address.
2. Without it, as on plain TOS, with EWeb's latest release in C:\\EWEB\\
   (EMAIL.INF's BROWSER names it): a click asks, Return chooses "Open in
   EWeb", EMail quits and EWeb starts on the link.
3. The same with a link too long for a command line (150 characters):
   EWeb starts on EMail's LINK.HTM, which sends it on to the link.

    link_test.py WORKDIR [MACHINE]  (make build/fakeweb.acc first)
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
PLAIN = "https://example.com/show?a=1&b=2"
HTML = "http://news.example/today?id=42"
LONG = "http://track.example/c?u=" + "0123456789" * 15     # past a command line's 125

work, machine, more = rig.cli("Falcon")
if machine.partition(":")[0] not in ("Falcon", "Falcon-4", "TT") or ":16" not in machine:
    print("SKIP on %s: the clicks are placed for a 640x480 screen in 16 colours" % machine)
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
with open(os.path.join(MD, "new", "5-plain.eml"), "wb") as f:
    f.write(("From: Avi <avi@example.com>\r\nTo: dana@test.local\r\nSubject: The show\r\n"
             "Date: Thu, 22 Oct 2026 09:00:00 +0300\r\nMessage-ID: <show-1@example.com>\r\nMIME-Version: 1.0\r\n"
             "Content-Type: text/plain; charset=utf-8\r\n\r\n"
             "Hello,\r\n\r\nThe pictures: %s for now.\r\n\r\n"
             "\u05d4\u05ea\u05de\u05d5\u05e0\u05d5\u05ea \u05d1-www.atari.org \u05e2\u05db\u05e9\u05d9\u05d5.\r\n"
             % PLAIN).encode("utf-8"))
with open(os.path.join(MD, "new", "6-html.eml"), "wb") as f:
    f.write(("From: News <news@example.com>\r\nTo: dana@test.local\r\nSubject: Today\r\n"
             "Date: Wed, 21 Oct 2026 09:00:00 +0300\r\nMessage-ID: <news-1@example.com>\r\nMIME-Version: 1.0\r\n"
             "Content-Type: text/html; charset=utf-8\r\n\r\n"
             "<html><body><p>Hello,</p><p>Read <a href=\"%s\">the news</a> today.</p></body></html>\r\n"
             % HTML.replace("&", "&amp;")).encode())
with open(os.path.join(MD, "new", "7-long.eml"), "wb") as f:
    f.write(("From: Shop <shop@example.com>\r\nTo: dana@test.local\r\nSubject: Your order\r\n"
             "Date: Tue, 20 Oct 2026 20:00:00 +0300\r\nMessage-ID: <order-1@example.com>\r\nMIME-Version: 1.0\r\n"
             "Content-Type: text/html; charset=utf-8\r\n\r\n"
             "<p>Your order is on its way: <a href=\"%s\">track it</a>.</p>\r\n" % LONG).encode())
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd)
shutil.copy(os.path.join(ROOT, "build", "fakeweb.acc"), os.path.join(hd, "EWEB.ACC"))
rig.fakesting(os.path.join(hd, "AUTO"))
eweb = os.path.join(hd, "EWEB")
print("EWeb", rig.release("EWeb", eweb, "EWEB.PRG", "EWEB.FNT"), flush=True)
with open(os.path.join(hd, "EMAIL.INF"), "w", newline="") as f:
    f.write("[options]\r\ntz=180\r\nbrowser=C:\\EWEB\\EWEB.PRG\r\n[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
            "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
            "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=%d\r\n" % (IMAP, SMTP))
LOG = os.path.join(hd, "EWEB.LOG")

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


def underline(shot):
    """A link's underline in the reader's text: the middle of the first
    level run of one colour, neither black nor white, 24 to 400 pixels
    long (the rule across the pane is longer), and its row"""
    from PIL import Image
    im = Image.open(shot).convert("RGB")
    w, h = im.size
    px = im.load()
    for y in range(330, h - 20):
        x = 0
        while x < w:
            c = px[x, y]
            e = x
            while e < w and px[e, y] == c:
                e += 1
            if 24 <= e - x <= 400 and c not in ((0, 0, 0), (255, 255, 255)):
                return (x + e) // 2, y - 3
            x = e
    return None


def log():
    return open(LOG, errors="replace").read() if os.path.exists(LOG) else ""


# 1. a browser running: each link goes to it with VA_START
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
try:
    r.settle(45, 240)                    # boot, log in, mirror the inbox
    r.shot("l1-inbox")
    for row, (name, url) in enumerate((("plain", PLAIN), ("html", HTML))):
        r.click(330, 108 + 16 * row)     # the newest, then the next
        time.sleep(10)
        shot = r.shot("l2-%s" % name)
        at = underline(shot)
        check("the %s message's link is drawn underlined in colour" % name, at is not None, str(at))
        if at:
            r.click(*at)
            time.sleep(4)
            r.shot("l3-%s-clicked" % name)
            check("... a click hands %s to the browser" % url, ("start " + url) in log(),
                  log().strip().replace("\r\n", " | "))
    r.ctrl("q")
    time.sleep(5)
finally:
    r.stop()

# 2. and 3. no browser running (plain TOS): EMail quits, and EWeb starts
# on the link, a long one by LINK.HTM
os.remove(os.path.join(hd, "EWEB.ACC"))
for part, (row, url, look) in enumerate(((0, PLAIN, "example.com/show"), (2, LONG, "track.example/c"))):
    r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
    try:
        r.settle(45, 240)
        r.click(330, 108 + 16 * row)
        time.sleep(10)
        at = underline(r.shot("m%d-read" % part))
        if at:
            r.click(*at)
            time.sleep(4)
            r.shot("m%d-asked" % part)
            r.key(K["RETURN"])           # Open in EWeb
            time.sleep(40)               # EMail logs out and quits; EWeb starts
            r.shot("m%d-eweb" % part)
    finally:
        r.stop()
    hlog = open(os.path.join(work, "hatari.log"), errors="replace").read()
    eweb_lines = [l for l in hlog.splitlines() if l.startswith("EWEB ")]
    what = "a link of %d characters" % len(url)
    check("%s, no browser: EWeb started after EMail" % what, any(l.startswith("EWEB ready") for l in eweb_lines),
          " | ".join(eweb_lines[:2]))
    check("... on the link", any(look in l for l in eweb_lines), " | ".join(eweb_lines[-3:]))
    if url == LONG:
        page = open(os.path.join(hd, "LINK.HTM"), errors="replace").read() if os.path.exists(
            os.path.join(hd, "LINK.HTM")) else ""
        check("... by LINK.HTM, which names it", ("url=" + url) in page, page[:80])
bridge.terminate()
smtp.terminate()
subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
shutil.rmtree(run_dir, ignore_errors=True)
print("screenshots in", os.path.join(work, "shots"))
print("link test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
