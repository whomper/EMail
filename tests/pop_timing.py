#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""What a POP3 server answers to a login no one has, and after how long.

Yahoo's and AOL's POP3 servers left EMail waiting past its minute after
the login (the Providers workflow, 25 Oct 2026). This reaches each server
over TLS (port 995), says USER and PASS for aera-probe-q7x2k9@ its
domain, and prints each answer with the seconds it took, waiting up to
WAIT seconds for each.

    pop_timing.py [WAIT] [HOST...]     (180, Yahoo's and AOL's without)
"""
import socket
import ssl
import sys
import threading
import time

args = sys.argv[1:]
wait = int(args.pop(0)) if args and args[0].isdigit() else 180
hosts = args or ["pop.mail.yahoo.com", "pop.aol.com"]
lines = {}


def probe(host):
    out = lines.setdefault(host, [])
    user = "aera-probe-q7x2k9@" + host.split(".", 1)[1].replace("mail.", "")
    try:
        s = ssl.create_default_context().wrap_socket(socket.create_connection((host, 995), timeout=30),
                                                     server_hostname=host)
        f = s.makefile("rb")
        for say in (None, "USER " + user, "PASS not-the-password", "QUIT"):
            if say:
                s.sendall(say.encode() + b"\r\n")
            s.settimeout(wait)
            t0 = time.time()
            try:
                answer = f.readline().decode("latin-1").strip() or "(closed)"
            except socket.timeout:
                answer = "(nothing)"
            out.append("%-24s %-28s %6.1f s  %s" % (host, say.split(" ")[0] if say else "(greeting)",
                                                     time.time() - t0, answer[:90]))
            if answer in ("(closed)", "(nothing)"):
                break
        s.close()
    except (OSError, ssl.SSLError) as e:
        out.append("%-24s error: %s" % (host, e))


threads = [threading.Thread(target=probe, args=(h,)) for h in hosts]
for t in threads:
    t.start()
for t in threads:
    t.join()
for h in hosts:
    print("\n".join(lines[h]))
