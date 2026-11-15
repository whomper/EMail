| SPDX-License-Identifier: GPL-2.0-or-later
| Copyright (C) 2026 AERA | Erez Yaary
|
| Linked right after main's object: 2.20's change to main.c (NATFEATS= from store_init)
| made its code +36 bytes in the program as linked, and everything after it
| moved; with the 68030's cache the start went 760 -> 770 ms on Erez's
| configuration (Falcon-4:256, TOS 4.04, NVDI) in every pair against 2.19,
| though none of the changed code runs there. The pad puts the code after
| it back at 2.19's addresses mod 256 (+36 + 220 = 0 mod 256), as ECal's
| pads (ECal ff6fc7c; the hub, 15 Nov 2026). Kept by -Wl,-u,pad_main (the
| link drops unused sections). When that file changes size again, resize
| it: its growth since 2.19, taken from 256, mod 256.
	.section .text.pad_main,"ax"
	.globl	pad_main
pad_main:
	.skip	220
