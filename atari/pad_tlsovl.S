| SPDX-License-Identifier: GPL-2.0-or-later
| Copyright (C) 2026 AERA | Erez Yaary
|
| Linked right after tlsovl's object: 2.20's change to tls_ovl.c (TLSFB.OVL on a FireBee, AERA-Atari a99ea65)
| made its code +134 bytes in the program as linked, and everything after it
| moved; with the 68030's cache the start went 760 -> 770 ms on the owner's
| configuration (Falcon-4:256, TOS 4.04, NVDI) in every pair against 2.19,
| though none of the changed code runs there. The pad puts the code after
| it back at 2.19's addresses mod 256 (+134 + 122 = 0 mod 256), as ECal's
| pads (ECal ff6fc7c; the hub, 15 Nov 2026). Kept by -Wl,-u,pad_tlsovl (the
| link drops unused sections). When that file changes size again, resize
| it: its growth since 2.19, taken from 256, mod 256.
	.section .text.pad_tlsovl,"ax"
	.globl	pad_tlsovl
pad_tlsovl:
	.skip	122
| its strings and tables +92 bytes: these put the ones after them back (+92 + 164)
	.section .rodata.pad_tlsovl,"a"
	.globl	pad_tlsovl_ro
pad_tlsovl_ro:
	.skip	164
