#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A server that refuses logins the way Gmail does, for make itest.

Gmail lets programs like EMail in only with an app password; the normal
Google password is refused. EMail knows Google's servers by what they say
(through the gateway too, where the server's name is the Pi's) and then
says that Gmail takes an app password. This answers as Gmail's servers do,
plain, on three ports, and refuses every login:

    IMAP  "* OK Gimap ready ..."; LOGIN: NO [AUTHENTICATIONFAILED], or with
          "appspecific" NO [ALERT] Application-specific password required
    POP3  "+OK Gpop ready ..."; PASS: -ERR [AUTH] Username and password not accepted.
    SMTP  "220 smtp.gmail.com ESMTP ... gsmtp"; AUTH: 535 5.7.8 ... BadCredentials

    gmail_like.py IMAP_PORT POP3_PORT SMTP_PORT [appspecific]
"""
import socketserver
import sys
import threading

APPSPECIFIC = "appspecific" in sys.argv[4:]


class Imap(socketserver.StreamRequestHandler):
    def handle(self):
        w = self.wfile.write
        w(b"* OK Gimap ready for requests from 127.0.0.1 a1b2c3d4e5f6\r\n")
        for line in self.rfile:
            tag, _, rest = line.decode("latin-1").strip().partition(" ")
            cmd = rest.split(" ")[0].upper()
            if cmd == "CAPABILITY":
                w(b"* CAPABILITY IMAP4rev1 UNSELECT IDLE NAMESPACE QUOTA ID XLIST CHILDREN X-GM-EXT-1 "
                  b"XYZZY SASL-IR AUTH=XOAUTH2 AUTH=PLAIN AUTH=PLAIN-CLIENTTOKEN AUTH=OAUTHBEARER\r\n")
                w(("%s OK Thats all she wrote! a1b2c3d4e5f6\r\n" % tag).encode())
            elif cmd in ("LOGIN", "AUTHENTICATE"):
                if APPSPECIFIC:
                    w(("%s NO [ALERT] Application-specific password required: "
                       "https://support.google.com/accounts/answer/185833 (Failure)\r\n" % tag).encode())
                else:
                    w(("%s NO [AUTHENTICATIONFAILED] Invalid credentials (Failure)\r\n" % tag).encode())
            elif cmd == "LOGOUT":
                w(b"* BYE LOGOUT Requested\r\n")
                w(("%s OK 73 good day (Success)\r\n" % tag).encode())
                return
            else:
                w(("%s BAD Unknown command a1b2c3d4e5f6\r\n" % tag).encode())


class Pop3(socketserver.StreamRequestHandler):
    def handle(self):
        w = self.wfile.write
        w(b"+OK Gpop ready for requests from 127.0.0.1 a1b2c3d4e5f6\r\n")
        for line in self.rfile:
            cmd = line.decode("latin-1").strip().split(" ")[0].upper()
            if cmd == "USER":
                w(b"+OK send PASS\r\n")
            elif cmd in ("PASS", "AUTH"):
                w(b"-ERR [AUTH] Username and password not accepted.\r\n")
            elif cmd == "QUIT":
                w(b"+OK Farewell.\r\n")
                return
            else:
                w(b"-ERR bad command a1b2c3d4e5f6\r\n")


class Smtp(socketserver.StreamRequestHandler):
    def handle(self):
        w = self.wfile.write
        w(b"220 smtp.gmail.com ESMTP a1b2c3d4e5f6 - gsmtp\r\n")
        for line in self.rfile:
            cmd = line.decode("latin-1").strip().split(" ")[0].upper()
            if cmd in ("EHLO", "HELO"):
                w(b"250-smtp.gmail.com at your service, [127.0.0.1]\r\n250-SIZE 35882577\r\n250-8BITMIME\r\n"
                  b"250-AUTH LOGIN PLAIN XOAUTH2 PLAIN-CLIENTTOKEN OAUTHBEARER XOAUTH\r\n"
                  b"250-ENHANCEDSTATUSCODES\r\n250-PIPELINING\r\n250 SMTPUTF8\r\n")
            elif cmd == "AUTH":
                w(b"535-5.7.8 Username and Password not accepted. For more information, go to\r\n"
                  b"535 5.7.8  https://support.google.com/mail/?p=BadCredentials a1b2c3d4e5f6 - gsmtp\r\n")
            elif cmd == "QUIT":
                w(b"221 2.0.0 closing connection a1b2c3d4e5f6 - gsmtp\r\n")
                return
            else:
                w(b"502 5.5.1 Unrecognized command. a1b2c3d4e5f6 - gsmtp\r\n")


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


servers = [Server(("127.0.0.1", int(p)), h) for p, h in zip(sys.argv[1:4], (Imap, Pop3, Smtp))]
for s in servers[1:]:
    threading.Thread(target=s.serve_forever, daemon=True).start()
servers[0].serve_forever()
