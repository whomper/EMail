#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Esc stops the network (the AERA guidelines, 7.1): EMAIL.PRG checks its
mail at start on an IMAP "server" that takes the connection and never
says a word. Esc while the busy bee shows must end the wait at once, not
after the 60 s timeout; the alert says it was stopped; and a key typed
meanwhile (^N, a new message) is not lost.

Hatari has no network card: FAKESTNG.PRG carries EMail's TCP connections
over the emulated serial port to serial_bridge.py, as in net_test.py.

    stop_test.py WORKDIR [MACHINE]     (needs Xvfb, xdotool)
"""
import os
import shutil
import socket
import subprocess
import sys
import threading
import time

import rig
from rig import K, SCAN

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
IMAP = 10144


def silent_server(stop):
    """takes connections and says nothing"""
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", IMAP))
    s.listen(4)
    s.settimeout(0.5)
    held = []
    while not stop.is_set():
        try:
            c, _ = s.accept()
            held.append(c)
        except socket.timeout:
            pass
    for c in held:
        c.close()
    s.close()


def main():
    work, machine, more = rig.cli("ST")
    shutil.rmtree(work, ignore_errors=True)
    hd = os.path.join(work, "hd")
    os.makedirs(os.path.join(hd, "AUTO"))
    fails = []

    def check(what, ok):
        print(("  ok   " if ok else "  FAIL ") + what)
        if not ok:
            fails.append(what)

    stop = threading.Event()
    threading.Thread(target=silent_server, args=(stop,), daemon=True).start()
    rig.install_prg(hd)
    rig.fakesting(os.path.join(hd, "AUTO"))
    with open(os.path.join(hd, "EMAIL.INF"), "w", newline="") as f:
        f.write("[options]\r\ntz=180\r\nlog=1\r\n[account]\r\nname=Dana\r\nfullname=Dana Falcon\r\n"
                "email=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=%d\r\nuser=dana\r\n"
                "pass=secret\r\nsmtphost=127.0.0.1\r\nsmtpport=10026\r\n" % IMAP)
    fifo_out, fifo_in = os.path.join(work, "st_out"), os.path.join(work, "st_in")
    for p in (fifo_out, fifo_in):
        os.mkfifo(p)
    bridge = rig.bridge(work, fifo_out, fifo_in)
    r = rig.Rig(work, machine, serial=(fifo_out, fifo_in))
    log = os.path.join(hd, "EMAIL.LOG")
    try:
        time.sleep(30)                  # boot; connected, waiting for the greeting
        r.shot("s1-waiting")
        r.ctrl("n")                     # typed while busy: kept for afterwards
        time.sleep(0.5)
        r.key(K["ESC"])
        pressed = time.time()
        stopped = None
        while time.time() - pressed < 50 and stopped is None:
            time.sleep(1)
            text = open(log, encoding="latin-1").read() if os.path.exists(log) else ""
            if "!! stopped" in text:
                stopped = time.time() - pressed
        time.sleep(2)
        r.shot("s2-stopped")
        check("Esc ended the wait (%s)" % ("%.0f s" % stopped if stopped is not None else "not at all"),
              stopped is not None and stopped < 15)
        r.key(K["RETURN"])              # the alert that says so
        time.sleep(4)
        r.shot("s3-new-message")
    finally:
        r.stop()
        stop.set()
        bridge.terminate()
    text = open(log, encoding="latin-1").read() if os.path.exists(log) else ""
    check("no timeout", "timed out" not in text)
    print("screenshots in", os.path.join(work, "shots"))
    print("stop test:", "all passed" if not fails else "%d failed" % len(fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
