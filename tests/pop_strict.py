#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""POP3 servers that refuse a wrong password the strict way, for make itest.

The Providers workflow (25 Oct 2026) found that GMX and web.de hang up
after a refused PASS, and Yahoo and AOL stop answering. EMail must say
the server refused the login, at once, not try again on that connection.
One port each:

    hangup   PASS: -ERR, then closes the connection (GMX, web.de)
    silent   PASS: -ERR, then answers nothing more (Yahoo, AOL)
    sasl     USER: -ERR, AUTH PLAIN: -ERR with its own words (a server
             that takes only SASL PLAIN, where EMail must still try it)
    slow     PASS: -ERR 61 s on, past EMail's usual minute (Yahoo, AOL:
             "-ERR [SYS/TEMP] Server error - Please try again later." 60.2 s
             on, for a login no one has)

    pop_strict.py HANGUP_PORT SILENT_PORT SASL_PORT SLOW_PORT
"""
import socketserver
import sys
import threading
import time


def pop3(mode):
    class Pop3(socketserver.StreamRequestHandler):
        def handle(self):
            w = self.wfile.write
            w(b"+OK POP server ready\r\n")
            refused = False
            for line in self.rfile:
                if refused and mode == "silent":
                    continue
                cmd = line.decode("latin-1").strip().split(" ")[0].upper()
                if cmd == "USER":
                    w(b"-ERR plain login disabled, use AUTH PLAIN\r\n" if mode == "sasl" else b"+OK\r\n")
                elif cmd == "PASS" and mode == "slow":
                    time.sleep(61)
                    w(b"-ERR [SYS/TEMP] Server error - Please try again later.\r\n")
                elif cmd == "PASS":
                    w(b"-ERR authentication failed\r\n")
                    if mode == "hangup":
                        return
                    refused = True
                elif cmd == "AUTH":
                    w(b"-ERR AUTH PLAIN refused too\r\n")
                elif cmd == "QUIT":
                    w(b"+OK bye\r\n")
                    return
                else:
                    w(b"-ERR unknown command\r\n")
    return Pop3


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


servers = [Server(("127.0.0.1", int(p)), pop3(m)) for p, m in zip(sys.argv[1:5], ("hangup", "silent", "sasl", "slow"))]
for s in servers[1:]:
    threading.Thread(target=s.serve_forever, daemon=True).start()
servers[0].serve_forever()
