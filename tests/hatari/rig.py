# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Drive EMail in the Hatari emulator: an X display (Xvfb), Hatari's
command FIFO for keys and screenshots, xdotool for the mouse.
Adapted from Claude ST's recording rig (whomper/atari_claude).
"""
import atexit
import os
import shutil
import socket
import stat
import subprocess
import sys
import time

# ST scancodes (US layout)
SCAN = {c: i for i, c in enumerate("1234567890-=", 2)}
SCAN.update({c: i for i, c in enumerate("qwertyuiop[]", 16)})
SCAN.update({c: i for i, c in enumerate("asdfghjkl;'`", 30)})
SCAN.update({c: i for i, c in enumerate("zxcvbnm,./", 44)})
SCAN[" "] = 57
SCAN["\\"] = 43
SHIFTED = {"!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6", "&": "7", "*": "8",
           "(": "9", ")": "0", "_": "-", "+": "=", ":": ";", '"': "'", "<": ",", ">": ".",
           "?": "/", "{": "[", "}": "]", "|": "\\", "~": "`"}

COMMON_TOOLS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "common", "tools")
sys.path.insert(0, COMMON_TOOLS)
import machines  # noqa: E402  the family's test machines (guidelines, The machines)

# FAKESTNG.PRG and its bridge are common's: make -C common/tools/fakesting
FAKESTING = os.path.join(COMMON_TOOLS, "fakesting")


def cli(default, usage=None):
    """A rig's arguments, WORKDIR [MACHINE] [MORE...]: the work folder, the
    machine (one of machines.py's, NAME or NAME:SCREEN; default if left out)
    and what follows."""
    a = sys.argv[1:]
    if not a:
        sys.exit(usage or "usage: %s WORKDIR [MACHINE]  (machines: %s)"
                 % (os.path.basename(sys.argv[0]), ", ".join(machines.MACHINES)))
    rest = a[1:]
    machine = rest.pop(0) if rest and rest[0].partition(":")[0] in machines.MACHINES else default
    return os.path.abspath(a[0]), machine, rest


def fakesting(auto):
    """Put FAKESTNG.PRG in drive C:'s AUTO folder."""
    shutil.copy(os.path.join(FAKESTING, "FAKESTNG.PRG"), auto)


def release(app, d, *files):
    """Another AERA program's files from its release (guidelines 1.2), as
    Erez has them: APP for the latest, APP@vX.Y for that one. Returns the
    release's tag (also in d/APP.VERSION) for the report."""
    getrelease = os.path.join(COMMON_TOOLS, "getrelease.py")
    subprocess.run([sys.executable, getrelease, app, d] + list(files), check=True)
    return open(os.path.join(d, app.split("@")[0].upper() + ".VERSION")).read().strip()


def servers_ready(work, *ports):
    """Before Dovecot and the SMTP stand-in start: Dovecot runs as its own
    user, so every folder above work must let others through, and a server
    left by an earlier run must not hold the ports (the new run would talk
    to it, with its old certificate)."""
    d = os.path.abspath(work)
    while d != "/":
        d = os.path.dirname(d)
        if not os.stat(d).st_mode & stat.S_IXOTH:
            sys.exit("Dovecot can't reach %s: %s is closed to other users (mode %o); use a work folder "
                     "like /tmp/mail-test" % (work, d, os.stat(d).st_mode & 0o777))
    for p in ports:
        with socket.socket() as s:
            if s.connect_ex(("127.0.0.1", p)) == 0:
                sys.exit("port %d is taken, by a server from an earlier run? Stop it first "
                         "(doveadm stop, or kill tests/smtp_server.py)" % p)


def stop_at_exit(srv, *procs):
    """Stop Dovecot (srv/dovecot.conf) and the servers in procs when the rig
    ends, however it ends."""
    def stop():
        for p in procs:
            if p.poll() is None:
                p.terminate()
        subprocess.run(["doveadm", "-c", os.path.join(srv, "dovecot.conf"), "stop"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    atexit.register(stop)


def bridge(work, fifo_out, fifo_in):
    """Start FAKESTNG's other end on Hatari's serial FIFOs, logging to work/bridge.log."""
    return subprocess.Popen([sys.executable, os.path.join(FAKESTING, "serial_bridge.py"), fifo_out, fifo_in],
                            stderr=open(os.path.join(work, "bridge.log"), "w"))


K = dict(RETURN=0x1c, ESC=0x01, TAB=0x0f, BACKSPACE=0x0e, DELETE=0x53, UP=0x48, DOWN=0x50,
         LEFT=0x4b, RIGHT=0x4d, HOME=0x47, HELP=0x62, F10=0x44, CTRL=0x1d, SHIFT=0x2a, ALT=0x38)


class Rig:
    def __init__(self, work, machine="Falcon", display=":99", serial=None, extra=(),
                 auto="C:\\EMAIL.PRG"):
        """machine: one of machines.py's (Falcon, ST:mono...), drive C: is work/hd"""
        self.work, self.display = work, display
        self.machine = machines.Machine(machine, os.path.join(work, "hd"))
        print("machine:", self.machine.describe(), flush=True)
        self.env = self.machine.env(dict(os.environ, DISPLAY=display, SDL_VIDEODRIVER="x11",
                                         SDL_AUDIODRIVER="dummy"))
        self.fifo = os.path.join(work, "cmd.fifo")
        self.shots = os.path.join(work, "shots")
        os.makedirs(self.shots, exist_ok=True)
        self.win = None
        self.xvfb = machines.xvfb(display)  # None when a display already answers
        args = (["hatari", "-w", "--statusbar", "no", "--borders", "no", "--zoom", "1"] + self.machine.args() +
                ["--fast-boot", "yes", "--natfeats", "yes", "--cmd-fifo", self.fifo,
                 "--screenshot-dir", self.shots, "--log-level", "warn"])
        if auto:
            args += ["--auto", auto]
        if serial:
            args += ["--rs232-out", serial[0], "--rs232-in", serial[1]]
        args += list(extra)
        self.log = open(os.path.join(work, "hatari.log"), "w")
        self.proc = subprocess.Popen(args, env=self.env, stdout=self.log, stderr=subprocess.STDOUT)

    def cmd(self, *args):
        for _ in range(200):
            if os.path.exists(self.fifo) and stat.S_ISFIFO(os.stat(self.fifo).st_mode):
                break
            time.sleep(0.1)
        else:
            raise RuntimeError("Hatari's command FIFO never appeared")
        with open(self.fifo, "w") as f:
            f.write(" ".join(args) + "\n")
        time.sleep(0.05)

    def key(self, scancode, shift=False, ctrl=False, alt=False):
        for on, k in ((ctrl, K["CTRL"]), (shift, K["SHIFT"]), (alt, K["ALT"])):
            if on:
                self.cmd("hatari-event", "keydown", str(k))
        self.cmd("hatari-event", "keypress", "0x%02x" % scancode)
        for on, k in ((alt, K["ALT"]), (shift, K["SHIFT"]), (ctrl, K["CTRL"])):
            if on:
                self.cmd("hatari-event", "keyup", str(k))
        time.sleep(0.08)

    def ctrl(self, letter):
        self.key(SCAN[letter], ctrl=True)

    def type(self, text, pace=0.08):
        for ch in text:
            if ch in SHIFTED:
                self.key(SCAN[SHIFTED[ch]], shift=True)
            elif ch.isupper():
                self.key(SCAN[ch.lower()], shift=True)
            elif ch == "\n":
                self.key(K["RETURN"])
            else:
                self.key(SCAN[ch])
            time.sleep(pace)

    def shot(self, name, wait=0.6):
        """Save a screenshot as shots/NAME.png and return its path."""
        before = set(os.listdir(self.shots))
        self.cmd("hatari-shortcut", "screenshot")
        for _ in range(100):
            new = [f for f in set(os.listdir(self.shots)) - before if f.startswith("grab")]
            if new:
                time.sleep(wait)
                dst = os.path.join(self.shots, name + ".png")
                shutil.move(os.path.join(self.shots, new[0]), dst)
                return dst
            time.sleep(0.1)
        raise RuntimeError("no screenshot")

    def settle(self, least, most):
        """wait at least `least` seconds, then until EMail's windows are up
        (some ink, mostly white: not a blank screen, not the desktop) and
        the same in two shots 3 s apart, up to `most`:
        a real TOS 1.04 on an 8 MHz ST boots slower than EmuTOS, and a
        job counted too soon catches EMail's own start"""
        from PIL import Image, ImageChops
        time.sleep(least)
        end, last = time.time() + most - least, None
        while True:
            cur = self.shot("settle-%d" % int(time.time() * 10), 0.3)
            im = Image.open(cur).convert("RGB")
            w, h = im.size
            ink = sum(n for n, c in im.getcolors(w * h) if c != (255, 255, 255))
            drawn = 0.03 * w * h < ink < 0.4 * w * h     # EMail's white windows, not the desktop
            same = last is not None and not ImageChops.difference(Image.open(last).convert("RGB"), im).getbbox()
            if last:
                os.remove(last)
            last = cur
            if (drawn and same) or time.time() > end:
                os.remove(cur)
                return drawn and same
            time.sleep(3)

    # ---- mouse ----
    def xdo(self, *args):
        return subprocess.run(["xdotool", *args], env=self.env, capture_output=True, text=True).stdout

    def window(self):
        for _ in range(100):
            if self.win:
                break
            found = (self.xdo("search", "--name", "Hatari") or self.xdo("search", "--class", "hatari")).split()
            if found:
                self.win = found[0]
            else:
                time.sleep(0.2)
        if not self.win:
            raise RuntimeError("no Hatari window")
        geo = dict(l.split("=") for l in self.xdo("getwindowgeometry", "--shell", self.win).split())
        return int(geo["X"]), int(geo["Y"])

    def move(self, x, y, secs=0.4):
        wx, wy = self.window()
        loc = dict(l.split("=") for l in self.xdo("getmouselocation", "--shell").split())
        x0, y0, x1, y1 = int(loc["X"]), int(loc["Y"]), wx + x, wy + y
        steps = max(8, int(secs / 0.02))
        for i in range(1, steps + 1):
            t = i / steps
            self.xdo("mousemove", str(int(x0 + (x1 - x0) * t)), str(int(y0 + (y1 - y0) * t)))
            time.sleep(secs / steps)
        time.sleep(0.15)

    def click(self, x=None, y=None, double=False):
        if x is not None:
            self.move(x, y)
        self.xdo("click", "--repeat", "2" if double else "1", "--delay", "120", "1")
        time.sleep(0.3)

    def drag(self, x1, y1, x2, y2, secs=0.6):
        """press at (x1, y1), move to (x2, y2), release"""
        self.move(x1, y1)
        self.xdo("mousedown", "1")
        time.sleep(0.2)
        self.move(x2, y2, secs)
        time.sleep(0.2)
        self.xdo("mouseup", "1")
        time.sleep(0.4)

    def rclick(self, x=None, y=None):
        if x is not None:
            self.move(x, y)
        self.xdo("click", "3")
        time.sleep(0.5)

    def stop(self):
        self.proc.terminate()
        try:
            self.proc.wait(5)
        except Exception:
            self.proc.kill()
        if self.xvfb:
            self.xvfb.terminate()
