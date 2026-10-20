#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The screenshots in docs/, on an emulated Falcon in 16 colours.

An inbox of everyday English mail (the Hebrew test messages stay with the
tests), the time zone Europe/London, a few folders. Saves the main window
with a message open and Settings > Connection to
WORKDIR/shots; copy the ones that look right into docs/.

    screenshots.py WORKDIR [MACHINE]     (needs Xvfb, xdotool, dovecot)
"""
import imaplib
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

work, machine, more = rig.cli("Falcon")
os.makedirs(work, exist_ok=True)
srv = os.path.join(work, "srv")
hd = os.path.join(work, "hd")
for d in (srv, hd):
    shutil.rmtree(d, ignore_errors=True)
for p in ("cmd.fifo",):
    if os.path.exists(os.path.join(work, p)):
        os.remove(os.path.join(work, p))
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
subprocess.run(["chown", "-R", "nobody:", os.path.join(srv, "home")], check=True)
rig.servers_ready(work, IMAP, SMTP, 10110)
subprocess.run(["dovecot", "-c", os.path.join(srv, "dovecot.conf")], check=True)


def plain(frm, subject, date, body):
    return ("From: %s\r\nTo: Dana <dana@test.local>\r\nSubject: %s\r\nDate: %s\r\n"
            "Content-Type: text/plain; charset=us-ascii\r\n\r\n%s" % (frm, subject, date, body)).encode()


def with_file(frm, subject, date, body, name):
    return ("From: %s\r\nTo: Dana <dana@test.local>\r\nSubject: %s\r\nDate: %s\r\nMIME-Version: 1.0\r\n"
            "Content-Type: multipart/mixed; boundary=b1\r\n\r\n--b1\r\nContent-Type: text/plain\r\n\r\n%s\r\n"
            "--b1\r\nContent-Type: application/octet-stream; name=%s\r\nContent-Transfer-Encoding: base64\r\n\r\n"
            "AAECAwQFBgcICQ==\r\n--b1--\r\n" % (frm, subject, date, body, name)).encode()


# oldest first; (flags, message)
MAIL = [
    ("(\\Seen)", plain("Atari Forum <noreply@atari-forum.example>", "New reply: STinG receive window",
                       "Mon, 05 Oct 2026 09:12:00 +0100", "Someone replied to your topic.\r\n")),
    ("(\\Seen)", plain("Northside Bank <statements@bank.example>", "Your October statement is ready",
                       "Tue, 06 Oct 2026 07:30:00 +0100", "Your statement is ready to view online.\r\n")),
    ("(\\Seen)", with_file("Anna Schmidt <anna@example.de>", "Disk images for the Mega STE",
                           "Tue, 06 Oct 2026 16:45:00 +0100", "Here are the images you asked for.", "MEGASTE.ST")),
    ("(\\Seen)", plain("GitHub <notifications@github.example>", "[EMail] New release 1.24",
                       "Wed, 07 Oct 2026 11:02:00 +0100", "A new release is out.\r\n")),
    ("(\\Seen)", plain("Retro Gamer Weekly <news@retrogamer.example>", "Issue 42: the STE at 37",
                       "Thu, 08 Oct 2026 08:00:00 +0100", "This week: the STE turns 37.\r\n")),
    ("()", plain("Mike Turner <mike@example.com>", "Lunch on Friday?",
                 "Thu, 08 Oct 2026 12:20:00 +0100", "Are you free on Friday at one?\r\n")),
    ("()", with_file("Tom Baker <tom@example.co.uk>", "Photos from the show",
                     "Fri, 09 Oct 2026 10:15:00 +0100", "A few photos from Saturday.", "STAND.JPG")),
    ("(\\Flagged)", plain("Sarah Miller <sarah@example.org>", "Re: Falcon meetup",
                          "Fri, 09 Oct 2026 18:20:00 +0100",
                          "Sounds good, see you on Sunday!\r\n\r\nOn Fri, Dana wrote:\r\n"
                          "> Shall we bring the Falcon?\r\n> I can set up the DSP demo.\r\n"
                          ">> Who is coming to the meetup?\r\n>> We need a projector.\r\n"
                          ">>> Saturday or Sunday?\r\n")),
]
m0 = imaplib.IMAP4("127.0.0.1", IMAP)
m0.login("dana", "secret")
for name in ["Archive", "Lists", "Receipts", "Travel", "Work"]:
    m0.create(name)
for flags, msg in MAIL:
    m0.append("INBOX", flags, None, msg)
m0.logout()
smtp = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "smtp_server.py"), str(SMTP), srv, "dana", "secret"])
rig.stop_at_exit(srv, smtp)

# drive C:
os.makedirs(os.path.join(hd, "AUTO"))
rig.install_prg(hd)
rig.fakesting(os.path.join(hd, "AUTO"))
with open(os.path.join(hd, "EMAIL.INF"), "w", newline="") as f:
    f.write("[OPTIONS]\r\nTIMEZONE=Europe/London\r\nHEBREW=0\r\nSIGNATURE=Sent from my Falcon\r\n[ACCOUNT]\r\nNAME=Home\r\n"
            "FULLNAME=Dana Falcon\r\nEMAIL=dana@test.local\r\nIN=imap\r\nHOST=127.0.0.1\r\nPORT=%d\r\n"
            "USER=dana\r\nPASS=secret\r\nSMTPHOST=127.0.0.1\r\nSMTPPORT=%d\r\n" % (IMAP, SMTP))

fifo_out, fifo_in = os.path.join(work, "st_out"), os.path.join(work, "st_in")
for p in (fifo_out, fifo_in):
    if os.path.exists(p):
        os.remove(p)
    os.mkfifo(p)
bridge = rig.bridge(work, fifo_out, fifo_in)
shots = os.path.join(work, "shots")
shutil.rmtree(shots, ignore_errors=True)
r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
ok = False
try:
    time.sleep(45)                       # boot, log in, mirror the inbox
    r.drag(209, 300, 150, 300)           # a narrower folder pane, room for names
    time.sleep(3)
    r.shot("s0-inbox")
    r.click(330, 108)                    # the newest: the reply with quotes
    time.sleep(10)
    r.move(630, 470)                     # the pointer out of the picture's way
    time.sleep(1)
    r.shot("s1-read")
    r.click(340, 8)                      # Options
    time.sleep(0.5)
    r.click(348, 40)                     # Settings...
    time.sleep(3)
    r.click(150, 281)                    # Connection
    time.sleep(2)
    r.shot("s2-settings")
    r.click(395, 353)                    # Cancel
    time.sleep(2)
    ok = True
finally:
    r.stop()
    bridge.terminate()
    smtp.terminate()
    subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"])
    shutil.rmtree(run_dir, ignore_errors=True)
print("PASS" if ok else "FAIL")
