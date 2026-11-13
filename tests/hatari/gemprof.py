#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Where EMail's time goes, by function (common/tests/speed/gemprof.py):
speed.py's Inbox on disk, EMail built for the speed rig with its function
names put in (prgsyms.py), and Hatari's profiler from a key to EMail's
next event call. Never released.

    gemprof.py WORK [MACHINE]     (ST-1:mono by default; make build/speed/email.prg first)

The actions: the long letter shown, the long Hebrew letter shown, a new
message opened.
"""
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "common", "tests", "speed"))
import gemprof  # noqa: E402

# SPEED_PRG: another build in the same place (build/trace/email.prg says
# what each pane drew and how long showing a message took, step by step)
PRG = os.environ.get("SPEED_PRG") or os.path.join(ROOT, "build", "speed", "email.prg")
ACC = os.path.join(ROOT, "build", "speed", "emailchk.acc")
CLI = os.path.join(ROOT, "build", "mail-cli")
IMAP = 11143                     # not the other rigs' 10143

a = sys.argv[1:]
if not a:
    sys.exit(__doc__)
work = os.path.abspath(a[0])
acc = os.environ.get("ACC") == "1"
machine = a[1] if len(a) > 1 else "ST-1:mono"
if acc and machine.partition(":")[0] in ("ST", "Falcon-4"):
    sys.exit("speed.py: no accessory runs on ST or Falcon-4 (Erez, 4 Nov 2026): use ST-1, TT or Falcon")
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
        with open(os.path.join(host, "email.inf"), "w", newline="") as f:
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


ELF = os.path.join(ROOT, "build", "speed", "mail.elf")
p = gemprof.Profiler(work, machine, ELF, top=int(os.environ.get("TOP", "15")))
r = p.rig
symprg = os.path.join(p.work, "email.prg")
subprocess.run([sys.executable, os.path.join(HERE, "prgsyms.py"), ELF, PRG, symprg], check=True)
host, inf = mirror(os.path.join(r.work, "fixture"))
shutil.copytree(os.path.join(host, "email"), os.path.join(r.hd, "EMAIL", "email"))
r.put(symprg, "EMAIL")
r.write("EMAIL\\email.inf".replace("\\", os.sep), inf.replace("\r\n", "\n").replace("tz=180\n", "tz=180\nstartoffline=1\n"))
try:
    r.start("C:\\EMAIL\\email.prg", ready="AESSTART")
    from PIL import Image
    row = 16 if Image.open(r.shot("start")).size[1] >= 400 else 8
    r.click(400, 103 + 2 * row if row == 16 else 51 + 2 * row)
    time.sleep(3)
    r.key("Home")
    time.sleep(3)
    r.still()
    p.begin()
    # each from the function that starts the work (after_event), to the next
    # event call: the turn that shows a message, the one that opens the editor
    if os.environ.get("ONLY_PAGES"):
        # a page down the list, then the letter's (Tab to it), each from
        # win_scroll_to to the next event call
        p.after_event = "win_scroll_to"
        print(p.action("a page down the list (Shift+Down)", lambda: r.key("shift+Down")), flush=True)
        r.key("Home")
        time.sleep(3)
        r.key("Tab")
        time.sleep(2)
        r.still()
        print(p.action("a page down the letter (Shift+Down)", lambda: r.key("shift+Down")), flush=True)
        os.environ["ONLY_NEW"] = os.environ["ONLY_LETTER"] = "1"
    if os.environ.get("ONLY_INBOX"):
        # the Inbox's index read again from disk, as at the start: Outbox,
        # then back to the Inbox in the folder pane
        r.click(40, 121)
        time.sleep(4)
        r.still()
        p.after_event = "folders_select"
        print(p.action("the Inbox opened (500 headers from disk)", lambda: r.click(40, 105), settle=12), flush=True)
        os.environ["ONLY_NEW"] = os.environ["ONLY_LETTER"] = "1"
    if not os.environ.get("ONLY_NEW"):
        p.after_event = "reader_show_begin"
        print(p.action("the long letter shown (Down)", lambda: r.key("Down")), flush=True)
        print(p.action("the long Hebrew letter shown (Down)", lambda: r.key("Down")), flush=True)
    if not os.environ.get("ONLY_LETTER"):
        p.after_event = "editor_open"
        print(p.action("a new message (Ctrl+N)", lambda: r.key("ctrl+n")), flush=True)
finally:
    p.stop()
