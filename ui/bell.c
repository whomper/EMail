/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * bell.c - the new-mail bell (decided 27 Nov 2026: a soothing bell, not a
 * harsh one, in EMail and EMail check, on by default; BELL= in EMAIL.INF,
 * "Ring a bell for new mail" in Settings > Mail).
 *
 * Where the machine has DMA sound (the STE, TT and Falcon: bit 1 of the
 * _SND cookie) it plays a sampled bell, made at build time by
 * tools/mkbell.py (bell_snd: 8-bit signed mono, 12517 Hz, 1.7 s, with a
 * small room's reverb). DMA reads
 * ST RAM only, so where the program's image is in TT RAM the sample is
 * copied once into a block of ST RAM taken by bell_init, before an
 * accessory's first event (guidelines 2.2). The hardware plays it alone:
 * nothing waits.
 *
 * A plain ST has only its YM chip: a soft chime of two square tones a fifth
 * apart (E5 and B5), started at volume 10 of 15 and stepped down to
 * silence over a second and a half by Dosound's own timer, with a third,
 * quieter voice a moment later for the room around it (about 2.2 s). Elsewhere (no
 * sound at all) it is silent.
 *
 * Each ring writes "BELL dma" or "BELL ym" to the emulator's console, for
 * the rigs.
 */
#include <string.h>
#include <stdlib.h>
#include "tos.h"
#include "plat.h"
#include "bell.h"

extern const signed char bell_snd[];	/* build/bell_snd.c (tools/mkbell.py) */
extern const long bell_len;

#define Dosound(p) trap14_wl(32, (long)(p))

static short how;		/* 0 none, 1 DMA, 2 YM */
static const signed char *dma_from;

/* the YM chime: registers and values, 0xFF n a wait of n 50 Hz ticks
   (Dosound's table), 0xFF 0 the end. A and B a fifth apart; C the room
   around them (decided 27 Nov 2026: "a bit of reverb"): E5 again, a hair
   flat, coming in a tenth of a second later, quieter, and fading more
   slowly. Mixer 0xF8: the three tones, no noise, both ports as TOS has
   them */
static const unsigned char ym_chime[] = {
	0, 190, 1, 0,		/* A: 125000 / 190 = 658 Hz, E5 */
	2, 127, 3, 0,		/* B: 984 Hz, B5, a fifth above */
	4, 192, 5, 0,		/* C: 651 Hz, the echo */
	10, 0, 7, 0xF8,
	8, 10, 9, 7, 0xFF, 5,
	8, 9, 9, 6, 10, 6, 0xFF, 6,
	8, 8, 9, 5, 0xFF, 7,
	8, 7, 9, 4, 10, 5, 0xFF, 8,
	8, 6, 9, 3, 0xFF, 9,
	8, 5, 9, 2, 10, 4, 0xFF, 10,
	8, 4, 9, 1, 0xFF, 11,
	8, 3, 9, 0, 10, 3, 0xFF, 12,
	8, 2, 0xFF, 10,
	8, 1, 10, 2, 0xFF, 10,
	8, 0, 0xFF, 10,
	10, 1, 0xFF, 12,
	10, 0,
	7, 0xFF,		/* the tones off again */
	0xFF, 0
};

void bell_init(void)
{
	long snd = get_cookie(0x5F534E44L);	/* _SND */
	how = 0;
	/* TOS 1.00-1.04 sets no cookies (a jar there is a driver's, STinG's):
	   with no _MCH either, an ST, which has the YM */
	if (!snd && !get_cookie(0x5F4D4348L))	/* _MCH */
		snd = 1;
	if (snd & 2) {
		dma_from = bell_snd;
		if ((unsigned long)bell_snd >= 0x01000000UL) {
			/* in TT RAM: a copy where DMA can read it */
			signed char *p = acc_core(bell_len, 0);
			if ((long)p > 0) {
				memcpy(p, bell_snd, bell_len);
				dma_from = p;
				how = 1;
			}
		} else {
			how = 1;
		}
	}
	if (!how && (snd & 1))
		how = 2;
}

static void dma_play(void)
{
	volatile unsigned char *r = (volatile unsigned char *)0xFFFF8900L;
	unsigned long a = (unsigned long)dma_from, e = a + (unsigned long)bell_len;
	r[0x01] = 0;			/* stopped */
	r[0x03] = (unsigned char)(a >> 16);
	r[0x05] = (unsigned char)(a >> 8);
	r[0x07] = (unsigned char)a;
	r[0x0F] = (unsigned char)(e >> 16);
	r[0x11] = (unsigned char)(e >> 8);
	r[0x13] = (unsigned char)e;
	r[0x21] = 0x81;			/* mono, 12517 Hz */
	r[0x01] = 1;			/* play once */
}

void bell_ring(void)
{
	if (how == 1) {
		Supexec(dma_play);
		pf_debug("BELL dma");
	} else if (how == 2) {
		Dosound(ym_chime);
		pf_debug("BELL ym");
	}
}
