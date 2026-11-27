/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fakeplay.c - a stand-in for a player holding the sound (EPlay drives the
 * DMA and the DSP from interrupts), so tests/hatari/emailchk_test.py can
 * show the bell staying silent beside it (the family's rule: never take
 * sound from someone else). Never released.
 *
 * As an accessory, from its start: on a Falcon (_SND bit 2) it takes
 * Locksnd() and keeps it; on an STE or TT (bit 1 alone) it plays a buffer
 * of silence in a loop by DMA, so the play bit stays set. "FAKEPLAY holds
 * the sound (lock|dma)" goes to the emulator's console.
 */
#include "gem.h"
#include "tos.h"

static signed char quiet[1024];

static void loop_quiet(void)
{
	volatile unsigned char *r = (volatile unsigned char *)0xFFFF8900L;
	unsigned long a = (unsigned long)quiet, e = a + sizeof(quiet);
	r[0x03] = (unsigned char)(a >> 16);
	r[0x05] = (unsigned char)(a >> 8);
	r[0x07] = (unsigned char)a;
	r[0x0F] = (unsigned char)(e >> 16);
	r[0x11] = (unsigned char)(e >> 8);
	r[0x13] = (unsigned char)e;
	r[0x21] = 0x81;
	r[0x01] = 3;		/* play, and again: a loop */
}

int main(void)
{
	short msg[8];
	AEVENT ev;
	long snd = get_cookie(0x5F534E44L);
	appl_init();
	nf_on = 1;		/* a test stand-in: its console lines always */
	/* no Desk entry: the rig's clicks find EMail check's where it was */
	if (snd & 4) {
		trap14_w(128);		/* Locksnd, kept */
		nf_debug("FAKEPLAY holds the sound (lock)");
	} else if (snd & 2) {
		Supexec(loop_quiet);
		nf_debug("FAKEPLAY holds the sound (dma)");
	}
	for (;;)
		evnt_multi_(MU_MESAG, 0, 0, 0, 0, msg, &ev);
}
