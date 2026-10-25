#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""How long each provider waits for a program that says nothing after the
TLS handshake, before its login.

TLS.OVL 1.1 checks the server's certificates after the handshake, before
EMail says anything: on a TT without a DSP that is about 17 s for Gmail's
chain (a tester's TT, 25 Oct 2026). Here each server (IMAP 993, POP3 995,
SMTP 465) is reached over TLS, its greeting read, and then nothing is said
for WAIT seconds; then a harmless command (CAPABILITY, CAPA, EHLO). "kept"
means the server still answered, "closed" that it had hung up. All the
servers and waits run at once.

    idle_probe.py [WAIT...]          (20 40 60 without)
"""
import socket
import ssl
import sys
import threading
import time

SERVERS = [
    ("gmail", "imap.gmail.com", 993, "imap"), ("gmail", "pop.gmail.com", 995, "pop"),
    ("gmail", "smtp.gmail.com", 465, "smtp"),
    ("icloud", "imap.mail.me.com", 993, "imap"),
    ("yahoo", "imap.mail.yahoo.com", 993, "imap"), ("yahoo", "pop.mail.yahoo.com", 995, "pop"),
    ("yahoo", "smtp.mail.yahoo.com", 465, "smtp"),
    ("gmx", "imap.gmx.net", 993, "imap"), ("gmx", "pop.gmx.net", 995, "pop"), ("gmx", "mail.gmx.net", 465, "smtp"),
    ("webde", "imap.web.de", 993, "imap"), ("webde", "pop3.web.de", 995, "pop"),
    ("aol", "imap.aol.com", 993, "imap"), ("aol", "pop.aol.com", 995, "pop"), ("aol", "smtp.aol.com", 465, "smtp"),
    ("fastmail", "imap.fastmail.com", 993, "imap"), ("fastmail", "pop.fastmail.com", 995, "pop"),
    ("fastmail", "smtp.fastmail.com", 465, "smtp"),
    ("zoho", "imap.zoho.com", 993, "imap"), ("zoho", "pop.zoho.com", 995, "pop"), ("zoho", "smtp.zoho.com", 465, "smtp"),
]
COMMAND = {"imap": b"a1 CAPABILITY\r\n", "pop": b"CAPA\r\n", "smtp": b"EHLO probe.example\r\n"}
waits = [int(w) for w in sys.argv[1:]] or [20, 40, 60]
results = {}


def probe(host, port, kind, wait):
    t0 = time.time()
    try:
        raw = socket.create_connection((host, port), timeout=30)
        s = ssl.create_default_context().wrap_socket(raw, server_hostname=host)
        s.settimeout(30)
        s.recv(4096)                    # the greeting
        time.sleep(wait)
        s.sendall(COMMAND[kind])
        s.settimeout(15)
        answer = s.recv(4096)
        r = "kept" if answer else "closed"
        s.close()
    except (OSError, ssl.SSLError) as e:
        r = "closed" if time.time() - t0 >= wait else "error: %s" % e
    results[(host, port, wait)] = r


threads = [threading.Thread(target=probe, args=(h, p, k, w)) for _, h, p, k in SERVERS for w in waits]
for t in threads:
    t.start()
for t in threads:
    t.join()
print("%-9s %-22s %5s  %s" % ("provider", "server", "port", "  ".join("%3d s " % w for w in waits)))
for name, h, p, k in SERVERS:
    print("%-9s %-22s %5d  %s" % (name, h, p, "  ".join("%-6s" % results[(h, p, w)][:6] for w in waits)))
errors = [r for r in results.values() if r.startswith("error")]
for (h, p, w), r in sorted(results.items()):
    if r.startswith("error"):
        print("%s:%d after %d s: %s" % (h, p, w, r))
sys.exit(0)
