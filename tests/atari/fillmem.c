/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fillmem.c - the programs that come after an accessory on plain TOS, for
 * tests/hatari/emailchk_test.py's held case (guidelines 2.2: an
 * accessory's GEMDOS memory is its own). Never released; the sequence is
 * ECal's held boot case (small_acc.py --held).
 *
 * Built without the fast-load bit, so TOS clears its whole TPA when it
 * starts. Started at boot (Hatari's --auto) with no tail:
 * - with C:\HOLD.ON there, it holds the front: appl_init and an event
 *   call from the start (evnt_timer, 30 s), so the accessories have their
 *   first turns while it is the current process and what they take then
 *   would be its own; then it asks for itself again with "2"
 *   (shel_write(1): after this one ends) and quits;
 * - without it (the reference boot), or with "2", it fills: Malloc(-1),
 *   that block taken, filled with 0xA5 and freed, and the bytes written
 *   into C:\FILLMEM.LOG.
 */
#include <string.h>
#include <stdio.h>
#include "gem.h"
#include "tos.h"

const char *cmd_line(void);	/* tos.c */

static void note(const char *s)
{
	long h = Fopen("C:\\FILLMEM.LOG", 1);
	if (h < 0)
		h = Fcreate("C:\\FILLMEM.LOG", 0);
	if (h < 0)
		return;
	Fseek(0, (short)h, 2);
	Fwrite((short)h, strlen(s), s);
	Fwrite((short)h, 2, "\r\n");
	Fclose((short)h);
}

int main(void)
{
	char line[80];
	const char *c = cmd_line();
	long free;
	char *p;
	if (!strchr(c, '2') && Fsfirst("C:\\HOLD.ON", 0) == 0) {
		char tail[4] = { 2, ' ', '2', 0 };
		appl_init();
		evnt_timer_(30000);
		note(shel_write(1, 1, 0, "C:\\FILLMEM.PRG", tail) ? "held, then itself" : "held, shel_write failed");
		appl_exit();
		return 0;
	}
	free = (long)Malloc(-1L);
	p = Malloc(free);
	if ((long)p > 0) {
		memset(p, 0xA5, free);
		Mfree(p);
	}
	sprintf(line, "filled %ld", (long)p > 0 ? free : 0L);
	note(line);
	return 0;
}
