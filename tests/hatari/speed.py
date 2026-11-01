#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EMail's speed on the family's stock machines (the guidelines' Speed:
the stock machines set it; Erez, 26 Oct 2026), with common's speed rig
(common/tests/speed/speed.py): the longest stretch between event calls of
each action, from build/speed/EMAIL.PRG (-DAES_PROFILE=20), against the
guidelines' times.

The mail is on disk, as a user's is: this computer's mail-cli mirrors an
Inbox of 500 short messages, a long letter and a long Hebrew one from a
Dovecot of its own, with every body, into C:\\EMAIL\\EMAIL; EMail starts
offline (STARTOFFLINE=1), so no network is needed. The actions, by keys
(an ST has no Page Down: Shift+arrow):

    the program started            (the Inbox shown)
    the long letter shown          (Down in the list)
    the long Hebrew letter shown   (Down)
    a short message shown          (Down)
    a page down the list           (Shift+Down)
    the long letter again          (Home)
    a row pressed, released        (the button held over the third row,
                                    then let go: lit, then shown; again
                                    for the first, the long letter; the
                                    row checked lit by its pixels while
                                    the button is still down)
    Find opened, a letter typed,   (Ctrl+F; e, then x: the 500 messages
    another, Find left              looked through after each; Undo)
    a line down the letter         (Tab to it, Down)
    a page down the letter         (Shift+Down)
    Settings opened, closed        (Ctrl+, then Undo: the window uncovered)
    About EMail opened, closed     (the EMail menu's first item, then Esc)
    a folder's row pressed, let    (Outbox held, checked lit by its pixels,
    go elsewhere                    then let go below the pane: Inbox again)
    a new message, a letter typed  (Ctrl+N; a sentence typed in the
                                    text, then one more key timed)

    speed.py WORK [MACHINE] [nvdi]     (Falcon-4:16 without; ST:mono,
                                        ST:med, ST-1, Falcon-4:16 ...)

With ACC=1, EMail check (build/speed/EMAILCHK.ACC) is opened from the
Desk menu instead.
"""
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "common", "tests", "speed"))
import speed  # noqa: E402

# SPEED_PRG: another build in the same place (build/trace/EMAIL.PRG says
# what each pane drew and how long showing a message took, step by step)
PRG = os.environ.get("SPEED_PRG") or os.path.join(ROOT, "build", "speed", "EMAIL.PRG")
ACC = os.path.join(ROOT, "build", "speed", "EMAILCHK.ACC")
CLI = os.path.join(ROOT, "build", "mail-cli")
IMAP = 11143                     # not the other rigs' 10143

a = sys.argv[1:]
if not a:
    sys.exit(__doc__)
work = os.path.abspath(a[0])
machine = a[1] if len(a) > 1 else "Falcon-4:16"
nvdi = "nvdi" in a[2:]
for f in (PRG, CLI):
    if not os.path.exists(f):
        sys.exit("first: make %s" % os.path.relpath(f, ROOT))


def mirror(dest):
    """the Inbox mirrored with every body, by this computer's mail-cli"""
    srv = os.path.join(dest, "srv")
    host = os.path.join(dest, "host")
    for d in ("state", "home/dana/Maildir/new", "home/dana/Maildir/cur", "home/dana/Maildir/tmp"):
        os.makedirs(os.path.join(srv, d))
    os.makedirs(host)
    os.chmod(dest, 0o755)
    os.chmod(srv, 0o755)
    with open(os.path.join(srv, "users"), "w") as f:
        f.write("dana:{PLAIN}secret\n")
    run = subprocess.run(["mktemp", "-d", "/tmp/tdv.XXXXXX"], capture_output=True, text=True).stdout.strip()
    uid = subprocess.check_output(["id", "-u", "nobody"]).decode().strip()
    gid = subprocess.check_output(["id", "-g", "nobody"]).decode().strip()
    conf = open(os.path.join(ROOT, "tests", "dovecot.conf.in")).read()
    for k, v in (("@RUN@", run), ("@DIR@", srv), ("@IMAP@", str(IMAP)), ("@POP3@", str(IMAP + 1)),
                 ("@UID@", uid), ("@GID@", gid)):
        conf = conf.replace(k, v)
    with open(os.path.join(srv, "dovecot.conf"), "w") as f:
        f.write(conf)
    new = os.path.join(srv, "home/dana/Maildir/new")
    names = ["Avi Cohen", "Dana Levi", "Grace Hopper", "Maya Katz", "Noam Bar", "Tal Peretz",
             "Yael Mizrahi", "Omer Shani", "Shira Golan", "Eitan Ben-David"]
    for i in range(500):
        day, hour = 1 + i // 24 % 28, i % 24
        with open(os.path.join(new, "%03d.eml" % i), "w", newline="") as f:
            f.write("From: %s <p%03d@example.org>\r\nTo: dana@test.local\r\n"
                    "Subject: Order %d of the parts for the Falcon, with a longer subject\r\n"
                    "Date: %02d Sep 2026 %02d:00:00 +0300\r\nMessage-ID: <s%03d@example.org>\r\n\r\n"
                    "The text of message %d.\r\n" % (names[i % 10], i, 1000 + i, day, hour, i, i))
    with open(os.path.join(new, "998.eml"), "w", newline="") as f:
        f.write("From: Ruth Long <ruth@example.org>\r\nTo: dana@test.local\r\nSubject: A long letter\r\n"
                "Date: 20 Oct 2026 10:00:00 +0300\r\nMessage-ID: <long@example.org>\r\n\r\n" +
                "".join("Paragraph %d. Search Console has identified that some pages on your site are not "
                        "being indexed due to the following new reason, and here is what to do about it.\r\n\r\n"
                        % k for k in range(1, 61)))
    heb = ("\u05d4\u05d2\u05d9\u05e2 \u05d4\u05d6\u05de\u05df \u05dc\u05e8\u05d0\u05e9 \u05de\u05de\u05e9\u05dc\u05d4 "
           "\u05de\u05d6\u05e8\u05d7\u05d9, \u05e2\u05dd Atari Falcon \u05d5\u05e2\u05dd \u05d4\u05e8\u05d1\u05d4 "
           "\u05de\u05d9\u05dc\u05d9\u05dd \u05e2\u05dc \u05d4\u05e9\u05d5\u05e8\u05d4 \u05d4\u05d6\u05d0\u05ea.")
    with open(os.path.join(new, "999.eml"), "wb") as f:
        f.write(("From: Avi <avi@example.org>\r\nTo: dana@test.local\r\nSubject: =?UTF-8?B?" +
                 __import__("base64").b64encode(heb[:20].encode()).decode() + "?=\r\n"
                 "Date: 19 Oct 2026 10:00:00 +0300\r\nMessage-ID: <heb@example.org>\r\n"
                 "MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\n"
                 "Content-Transfer-Encoding: 8bit\r\n\r\n").encode() +
                "".join("%d. %s\r\n\r\n" % (k, heb) for k in range(1, 61)).encode())
    subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
    subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)
    try:
        inf = ("[options]\r\ntz=180\r\n[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
               "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\npass=secret\r\n"
               "smtphost=127.0.0.1\r\nsmtpport=10025\r\n" % IMAP)
        with open(os.path.join(host, "EMAIL.INF"), "w", newline="") as f:
            f.write(inf)
        env = dict(os.environ, TZ="UTC", ASAN_OPTIONS="detect_leaks=0")
        for args in (["check", "1"], ["getall", "1", "INBOX"]):
            out = subprocess.run([CLI, host] + args, env=env, capture_output=True, text=True)
            if out.returncode:
                sys.exit("speed: mail-cli %s: %s" % (" ".join(args), out.stdout + out.stderr))
    finally:
        subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
        shutil.rmtree(run, ignore_errors=True)
    return host, inf


# Undo through Hatari's command FIFO (xdotool has no key Hatari takes for
# it): Settings closed as a user cancels it (the guidelines, 3.5: Esc
# empties a field)
fifo = os.path.join(work, "cmd.fifo")
r = speed.Rig(work, machine, nvdi=nvdi, display=os.environ.get("SPEED_DISPLAY", ":91"),
              extra=["--cmd-fifo", fifo])


def press(x, y):
    """the left button pressed over (x, y) of the screen, and held"""
    g = dict(l.split("=") for l in r.xdo("getwindowgeometry", "--shell", r.win()).split())
    r.xdo("mousemove", str(int(g["X"]) + x), str(int(g["Y"]) + y))
    time.sleep(0.4)
    r.xdo("mousedown", "1")


def release():
    r.xdo("mouseup", "1")


held_bad = []


def held_lit(shot, before, y_new, y_old, what, x0=0.4, x1=0.9):
    """a press held still (5.8): the row pressed shows selected and the one
    it left doesn't, by the screen's pixels while the button is still down
    (TOS's AES holds BEG_UPDATE back while the left button is held: a row
    drawn under it showed only at the release, and AESGAP can't see it,
    the wait being inside an AES call; the hub, 1 Nov 2026). The
    selected and the plain row's ground are taken from the screen before
    the press, where y_old was the selected row and y_new a plain one"""
    from PIL import Image
    from collections import Counter

    def ground(img, y):
        w = img.size[0]
        band = [img.getpixel((x, yy)) for yy in range(y - 4, y + 5) for x in range(int(w * x0), int(w * x1))]
        return Counter(band).most_common(1)[0][0]
    a, b = Image.open(before).convert("RGB"), Image.open(shot).convert("RGB")
    sel, plain = ground(a, y_old), ground(a, y_new)
    ok = sel != plain and ground(b, y_new) == sel and ground(b, y_old) == plain
    print("%s  %s: held 1 s or more, the row %s" % ("ok  " if ok else "FAIL", what,
          "shown selected" if ok else "not shown selected (%s, %s; selected %s)" %
          (ground(b, y_new), ground(b, y_old), sel)), flush=True)
    if not ok:
        held_bad.append(what)


def undo():
    with open(fifo, "w") as f:
        f.write("hatari-event keypress 0x61\n")
host, inf = mirror(os.path.join(r.work, "fixture"))
shutil.copytree(os.path.join(host, "EMAIL"), os.path.join(r.hd, "EMAIL", "EMAIL"))
r.put(PRG, "EMAIL")
# FONT=ID,PT: the text in that font (a GDOS font under NVDI, as Erez's
# EMAIL.INF has it: font_test.py with AERA_WITH_NVDI=1 writes the first
# monospaced one's FONT= there)
font = os.environ.get("FONT")
r.write("EMAIL\\EMAIL.INF".replace("\\", os.sep),
        inf.replace("\r\n", "\n").replace("tz=180\n", "tz=180\nstartoffline=1\n" +
                                            ("font=%s\n" % font if font else "")))
acc = os.environ.get("ACC") == "1"
try:
    if acc:
        r.put(ACC)
        r.write("EMAILCHK.INF", "FOLDER=C:\\EMAIL\\\n")
        r.start(ready="AESSTART")       # the accessory's own start, then the desktop
        time.sleep(15)
        r.still()
        from PIL import Image
        row = 16 if Image.open(r.shot("desk")).size[1] >= 400 else 8
        # the Desk menu's third row: Desktop Info, a line, the accessory
        r.act("EMail check opened", speed.ACC, lambda: r.menu(40, row + 2 * row + row // 2 + 3))
        r.act("EMail check closed", speed.WINDOW, lambda: r.key("Escape"))
    else:
        r.start("C:\\EMAIL\\EMAIL.PRG", ready="AESSTART")
        r.act("the long letter shown (Down)", speed.PAGE, lambda: r.key("Down"))
        r.act("the long Hebrew letter shown (Down)", speed.PAGE, lambda: r.key("Down"))
        r.act("a short message shown (Down)", speed.PAGE, lambda: r.key("Down"))
        r.act("a page down the list (Shift+Down)", speed.PAGE, lambda: r.key("shift+Down"))
        r.act("the long letter again (Home)", speed.PAGE, lambda: r.key("Home"))
        # guidelines 5.8: a row lit on the press (as a key or a tick, 50
        # ms), the message on the release; then back to the long letter
        from PIL import Image
        rows = r.shot("rows")
        row0 = 103 if Image.open(rows).size[1] < 450 else 108
        r.act("a message row pressed: lit at once", speed.KEY, lambda: press(400, row0 + 32))
        held = r.shot("held-1")       # the button still down, the screen still
        held_lit(held, rows, row0 + 32, row0, "a message row pressed")
        r.act("released: that message shown", speed.PAGE, release)
        rows = r.shot("rows-2")
        r.act("the long letter's row pressed", speed.KEY, lambda: press(400, row0))
        held_lit(r.shot("held-2"), rows, row0, row0 + 32, "the long letter's row pressed")
        r.act("released: the long letter shown", speed.PAGE, release)
        r.act("Find opened (Ctrl+F)", speed.PAGE, lambda: r.key("ctrl+f"))
        r.act("Find: e typed, the list follows", speed.PAGE, lambda: r.key("e"))
        r.act("Find: x typed, the list follows", speed.PAGE, lambda: r.key("x"))
        r.act("Find left (Undo)", speed.PAGE, undo)
        r.key("Tab")
        time.sleep(2)
        r.act("a line down the letter (Down)", speed.LINE, lambda: r.key("Down"))
        r.act("a page down the letter (Shift+Down)", speed.PAGE, lambda: r.key("shift+Down"))
        r.act("Settings opened (Ctrl+,)", speed.DIALOG, lambda: r.key("ctrl+comma"))
        r.act("Settings closed (Undo): the window uncovered", speed.WINDOW, undo)
        r.act("About EMail opened (the EMail menu)", speed.DIALOG, lambda: r.menu(40, 28))
        r.act("About closed (Esc): the window uncovered", speed.WINDOW, lambda: r.key("Escape"))
        # the folder pane (5.8): Outbox lit on the press, then let go
        # elsewhere, Inbox chosen again (last: the keyboard stays with it)
        rows = r.shot("rows-3")
        r.act("a folder's row pressed: lit at once", speed.KEY, lambda: press(60, row0 + 16))
        held_lit(r.shot("held-3"), rows, row0 + 16, row0, "a folder's row pressed", 0.03, 0.25)
        r.act("let go elsewhere: the folder shown again", speed.KEY,
              lambda: (r.xdo("mousemove_relative", "--", "0", "200"), time.sleep(0.4), release()))
        r.act("a new message (Ctrl+N)", speed.WINDOW, lambda: r.key("ctrl+n"))
        r.key("Down", "Down", "Down", "Down", "Down")     # past To, Cc, Subject: the text
        r.type("Thanks for the parts, the Falcon works again ")
        time.sleep(2)
        r.still()
        r.act("a letter typed at a line's end", speed.KEY, lambda: r.key("h"))
    code = r.report()
    if held_bad:
        print("FAIL  a held press not shown: " + ", ".join(held_bad))
        code = 1
finally:
    r.stop()
sys.exit(code)
