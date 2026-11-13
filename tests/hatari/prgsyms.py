#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A TOS program with its ELF's function names in it (a DRI symbol table,
GST's long names), so Hatari's debugger ("symbols prg") and its profiler
name EMail's functions. For the profiling rig only (gemprof.py): never
released.

    prgsyms.py ELF PRG OUT
"""
import struct
import subprocess
import sys


def main(elf, prg, out):
    data = open(prg, "rb").read()
    magic, tlen, dlen, blen, slen = struct.unpack_from(">HIIII", data, 0)
    assert magic == 0x601A and slen == 0, "a program without symbols"
    nm = subprocess.run(["m68k-linux-gnu-nm", "-n", "--defined-only", elf], capture_output=True, text=True).stdout
    text0 = None
    table = b""
    for l in nm.splitlines():
        f = l.split()
        if len(f) != 3:
            continue
        a, t, n = int(f[0], 16), f[1], f[2]
        if t in "tT" and text0 is None:
            text0 = a
        if t not in "tT" or text0 is None:
            continue
        name = n.encode()[:22]
        if len(name) <= 8:
            table += struct.pack(">8sHI", name, 0xA200, a - text0)
        else:   # GST: the name goes on into the next entry
            table += struct.pack(">8sHI", name[:8], 0xA248, a - text0) + name[8:22].ljust(14, b"\0")
    head = data[:28]
    head = head[:14] + struct.pack(">I", len(table)) + head[18:]
    body = 28 + tlen + dlen
    open(out, "wb").write(head + data[28:body] + table + data[body:])


if __name__ == "__main__":
    main(*sys.argv[1:4])
