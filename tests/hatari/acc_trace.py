#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""acc_trace.py TRACE: EMAILCHK.ACC's GEMDOS files and memory against the AES's event
calls (guideline 2.2), from Hatari's --trace gemdos,aes. Its calls are those whose PC
is in its image (from the Mshrink after it is loaded). A file of its is open across
an event call when any AES call comes between its open and its close: under plain
TOS another program runs only when the accessory has called the AES. GEMDOS memory
it takes after its first event call (the first evnt_ after its menu_register)."""
import re, sys
lines = open(sys.argv[1], "rb").read().decode("latin-1").splitlines()
lo = hi = None
reg = first_evnt = None
for i, l in enumerate(lines):
    if lo is None and "now loading 'EMAILCHK.ACC'" in l:
        for l2 in lines[i:i + 6]:
            m = re.search(r"Mshrink\(0x([0-9A-F]+), 0x([0-9A-F]+)\)", l2)
            if m:
                lo = int(m.group(1), 16); hi = lo + int(m.group(2), 16)
                break
    if reg is None and 'menu_register(addrin: "  EMail check"' in l:
        reg = i
    if reg is not None and first_evnt is None and i > reg and re.match(r"AES 0x\w+ evnt_", l):
        first_evnt = i
def ours(l):
    m = re.search(r"at PC[= ]0x([0-9A-F]+)", l)
    return bool(m) and lo <= int(m.group(1), 16) < hi
openfd, pending, bad, nopen, before = {}, None, [], 0, []
for i, l in enumerate(lines):
    if l.startswith("AES "):
        for st in openfd.values():
            st[2] += 1
        continue
    if pending and l.startswith("-> FD"):
        openfd[l.split()[2]] = [pending[0], pending[1], 0]
        pending = None
        continue
    if not l.startswith("GEMDOS") or not ours(l):
        continue
    m = re.match(r'GEMDOS 0x(3D|3C) \w+\("([^"]*)"', l)
    if m:
        pending = (i, m.group(2)); nopen += 1
        continue
    m = re.match(r"GEMDOS 0x3E Fclose\((\d+)\)", l)
    if m and m.group(1) in openfd:
        st = openfd.pop(m.group(1))
        if st[2]:
            bad.append("%s open across %d AES calls (lines %d-%d)" % (st[1], st[2], st[0], i))
        continue
    if re.match(r"GEMDOS 0x(48|44) (Malloc|Mxalloc)", l):
        (bad if first_evnt is not None and i > first_evnt else before).append("%d: %s" % (i, l.strip()))
print("accessory image 0x%X-0x%X, first event call at line %s" % (lo or 0, hi or 0, first_evnt))
print("its file opens: %d; open at the end of the trace: %s" % (nopen, [v[1] for v in openfd.values()]))
print("its GEMDOS memory before the first event call: %s" % "; ".join(before))
print("\n".join(bad) if bad else "OK: no file of its open across an AES call, no memory taken after its first event call")
