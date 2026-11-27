#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The round trip (guidelines 4.6, decided 26 Nov 2026): on plain TOS,
EMail starting EWeb for a link passes --back= and its own path, keeps its
place in EMAIL.INF and quits; EWeb brings it back with --resume when it
quits, and EMail opens at the same message, the next link clickable.

As link_test.py (Dovecot, FAKESTNG.PRG over the serial port, the same
messages), on plain TOS with build/fakeweb.prg as C:\\EWEB\\EWEB.PRG: a
stand-in that writes its command line into C:\\EWEB.LOG and, given
--back=, waits a moment and starts that program with " --resume", as
EWeb does on its Quit (guidelines 1.2: EMail runs alone).

1. The second message (HTML, "the news") opened; its link clicked, Return
   ("Open in EWeb"): the stand-in starts on the link with --back= and
   EMAIL.PRG's path; EMAIL.INF keeps RESUMEACCOUNT=, RESUMEFOLDER= and
   RESUMEMESSAGE=.
2. The stand-in quits and starts EMail with --resume: EMail is back with
   the same message shown (the reader and the list as before the click,
   the status line aside).
3. Its link clicked again: EMail asks as before (the next link works).

    resume_test.py WORKDIR [MACHINE]  (make build/fakeweb.prg first)

The link is a plain page (no scripts, no stylesheets) from a web server
on this computer, port 18080, reached through FAKESTNG. With EWEB_DIR=FOLDER, every file there (EWEB.PRG, EWEB.FNT, the
overlays) goes to C:\\EWEB in place of the stand-in: the real EWeb, for the
hub's family check (EMail's own runs never fetch it, guidelines 1.2). The
test then waits for EWeb's page, quits it with ^Q and checks what it can
see; the stand-in's log checks are left out.
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
# a plain page (no scripts, no stylesheets) from a web server here, which
# the Atari reaches through FAKESTNG as it does the mail servers
WEB = 18080
HTML = "http://127.0.0.1:%d/news.htm" % WEB
EWEB_DIR = os.environ.get("EWEB_DIR")
LONG = "http://track.example/c?u=" + "0123456789" * 15     # past a command line's 125
ALT = "https://www.atari-forum.com/viewtopic.php?t=12345&start=20"

work, machine, more = rig.cli("Falcon")
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
with open(os.path.join(MD, "new", "8-alt.eml"), "wb") as f:   # plain and HTML, as Gmail sends: the HTML's shown
    f.write(("From: Dana <dana@example.com>\r\nTo: dana@test.local\r\nSubject: Look at this\r\n"
             "Date: Wed, 21 Oct 2026 08:00:00 +0300\r\nMessage-ID: <alt-1@example.com>\r\nMIME-Version: 1.0\r\n"
             "Content-Type: multipart/alternative; boundary=A1\r\n\r\n--A1\r\nContent-Type: text/plain\r\n\r\n"
             "The new Falcon page: %s\r\n--A1\r\nContent-Type: text/html\r\n\r\n"
             "<div>The new <a href=\"%s\">Falcon page</a>.</div>\r\n--A1--\r\n"
             % (ALT, ALT.replace("&", "&amp;"))).encode())
with open(os.path.join(MD, "new", "7-long.eml"), "wb") as f:
    f.write(("From: Shop <shop@example.com>\r\nTo: dana@test.local\r\nSubject: Your order\r\n"
             "Date: Tue, 20 Oct 2026 20:00:00 +0300\r\nMessage-ID: <order-1@example.com>\r\nMIME-Version: 1.0\r\n"
             "Content-Type: text/html; charset=utf-8\r\n\r\n"
             "<p>Your order is on its way: <a href=\"%s\">track it</a>.</p>\r\n" % LONG).encode())
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110, WEB)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd)
rig.fakesting(os.path.join(hd, "AUTO"))
os.makedirs(os.path.join(hd, "EWEB"))
if EWEB_DIR:
    # the real EWeb (the hub's family check, never EMail's own runs:
    # guidelines 1.2): every file of the folder, EWEB.PRG, EWEB.FNT and its
    # overlays
    for n in os.listdir(EWEB_DIR):
        if os.path.isfile(os.path.join(EWEB_DIR, n)):
            shutil.copy(os.path.join(EWEB_DIR, n), os.path.join(hd, "EWEB", n.upper()))
else:
    shutil.copy(os.path.join(ROOT, "build", "fakeweb.prg"), os.path.join(hd, "EWEB", "EWEB.PRG"))
site = os.path.join(work, "site")
os.makedirs(site, exist_ok=True)
with open(os.path.join(site, "news.htm"), "w", newline="") as f:
    f.write("<html><head><title>The news</title></head><body><h1>The news</h1>\r\n"
            "<p>Today's page, plain HTML.</p></body></html>\r\n")
web = subprocess.Popen([sys.executable, "-m", "http.server", str(WEB), "--bind", "127.0.0.1", "--directory", site],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
rig.stop_at_exit(srv, web)
with open(os.path.join(hd, "email.inf"), "w", newline="") as f:
    f.write("[options]\r\nnatfeats=1\r\ntz=180\r\nbrowser=C:\\EWEB\\EWEB.PRG\r\n[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
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
    """A link's underline in the reader's text, below the header's rule: the
    middle of the first level run of one colour other than the background,
    24 to 400 pixels long, and the row above it (any screen: in colour the
    link's, in mono black)"""
    from PIL import Image
    im = Image.open(shot).convert("RGB")
    w, h = im.size
    px = im.load()
    x0 = w // 3                          # the right panes
    rule = None
    for y in range(150, h - 20):         # the reader's header rule: a run across the pane
        c = px[x0 + 40, y]
        n = 0
        while x0 + 40 + n < w - 30 and px[x0 + 40 + n, y] == c:
            n += 1
        if n > 300 and c != (255, 255, 255):
            rule = y
    if rule is None:
        return None
    for y in range(rule + 4, h - 20):
        bg = px[w - 40, y]
        x = x0
        while x < w - 30:
            c = px[x, y]
            e = x
            while e < w - 30 and px[e, y] == c:
                e += 1
            if 24 <= e - x <= 400 and c != bg and c != (255, 255, 255):
                return (x + e) // 2, y - 3
            x = e
    return None


def log():
    return open(LOG, errors="replace").read() if os.path.exists(LOG) else ""


def changed(a, b, box=None):
    from PIL import Image, ImageChops
    x, y = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    if box:
        x, y = x.crop(box), y.crop(box)
    d = ImageChops.difference(x, y).convert("L").point(lambda v: 255 if v else 0)
    return d.histogram()[255] / float(x.size[0] * x.size[1])


r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
try:
    r.settle(45, 240)                    # boot, log in, mirror the inbox
    r.click(330, 124)                    # the second message: the HTML one, "the news"
    time.sleep(10)
    before = r.shot("r1-read")
    at = underline(before)
    check("the message's link found", at is not None, str(at))
    if at:
        r.click(*at)
        time.sleep(4)
        r.shot("r2-asked")
        r.key(K["RETURN"])               # Open in EWeb
        # EMail logs out and quits, the stand-in starts, waits, quits and
        # starts EMail with --resume, which opens from the disk
        t0 = time.time()
        if EWEB_DIR:
            # EWeb up on the page, then its Quit (^Q, the family's)
            time.sleep(40)
            r.shot("r2b-eweb")
            r.ctrl("q")
        else:
            while time.time() - t0 < 240 and "back " not in log():
                time.sleep(5)
        r.settle(30, 200)
        back = r.shot("r3-back")
        args = [l[5:] for l in log().splitlines() if l.startswith("args ")]
        if not EWEB_DIR:
            check("the browser started on the link, with --back= and EMAIL.PRG's path",
                  any(a.strip().startswith(HTML + " --back=") and a.rstrip().upper().endswith("EMAIL.PRG")
                      for a in args), " | ".join(args))
        inf = open(rig.ci_path(hd, "email.inf"), errors="replace").read()
        check("EMAIL.INF keeps the place", "RESUMEACCOUNT=Dana" in inf and "RESUMEFOLDER=INBOX" in inf and
              "RESUMEMESSAGE=" in inf, " ".join(l for l in inf.splitlines() if l.startswith("RESUME")))
        if not EWEB_DIR:
            check("the stand-in started EMail again with --resume", "back " in log(),
                  log().strip().replace("\r\n", " | "))
        from PIL import Image
        W, H = Image.open(before).size
        panes = (W // 3, 60, W - 20, H - 30)          # the list and the reader, not the status line
        d = changed(before, back, panes)
        check("EMail is back at the same message (the list and the reader as before)", d < 0.02,
              "%.2f%% of them differ" % (100 * d))
        at2 = underline(back)
        check("... its link where it was", at2 is not None and abs(at2[1] - at[1]) <= 2, "%s, %s" % (at, at2))
        if at2:
            r.click(*at2)
            time.sleep(4)
            asked = r.shot("r4-asked-again")
            check("... and a click on it asks again", changed(back, asked) > 0.02,
                  "%.1f%% of the screen" % (100 * changed(back, asked)))
            r.key(K["ESC"])
            time.sleep(2)
    r.ctrl("q")
    time.sleep(5)
finally:
    r.stop()
crash = any(f.lower() == "crash.log" for f in os.listdir(hd))
check("no CRASH.LOG", not crash)
bridge.terminate()
smtp.terminate()
subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
shutil.rmtree(run_dir, ignore_errors=True)
print("screenshots in", os.path.join(work, "shots"))
print("resume test:", "all passed" if not fails else "%d failed" % len(fails))
sys.exit(1 if fails else 0)
