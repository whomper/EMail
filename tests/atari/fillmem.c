/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fillmem.c - the programs that come after an accessory on plain TOS, for
 * tests/hatari/emailchk_test.py's held case (guidelines 2.2: an
 * accessory's GEMDOS memory is its own). Never released.
 *
 * Built without the fast-load bit, so TOS clears its whole TPA when it
 * starts; it also fills every block it can take with 0xA5 and gives them
 * back. Started at boot (Hatari's --auto) with no tail, it fills, gives the
 * memory back and starts itself once more with "2" (Pexec), which does the
 * same in what is free then, and ends; then the first ends. Each run writes a
 * line into C:\FILLMEM.LOG with the bytes it filled.
 */
#include <string.h>
#include <stdio.h>
#include "gem.h"
#include "tos.h"

/* not in tos.h: a test program only */
#define Pexec(m, p, t, e) trap1_wwlll(0x4b, (m), (long)(p), (long)(t), (long)(e))

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
	void *got[64];
	short n = 0, i;
	long all = 0;
	char line[80];
	const char *c = cmd_line();
	int second = strchr(c, '2') != 0;
	while (n < 64) {
		long free = (long)Malloc(-1L);
		char *p;
		if (free < 16)
			break;
		p = Malloc(free);
		if ((long)p <= 0)
			break;
		memset(p, 0xA5, free);
		got[n++] = p;
		all += free;
	}
	for (i = 0; i < n; i++)
		Mfree(got[i]);
	sprintf(line, "run %d filled %ld", second ? 2 : 1, all);
	note(line);
	if (!second) {
		char tail[4] = { 2, ' ', '2', 0 };
		long rc = Pexec(0, "C:\\FILLMEM.PRG", tail, 0);
		sprintf(line, "run 2 ended %ld", rc);
		note(line);
	}
	return 0;
}
