/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fakeweb.c - a stand-in for a web browser's receiving side (EWeb's
 * VA_START), for tests/hatari/link_test.py: an accessory that answers to
 * the name EWEB (as EWEB.ACC). A VA_START is answered with AV_STARTED
 * (av_message does it), and its text goes into C:\EWEB.LOG, for the test
 * to read.
 */
#include <string.h>
#include <stdio.h>
#include "gem.h"
#include "tos.h"
#include "av.h"

static void note(const char *a, const char *b)
{
	long h = Fopen("C:\\EWEB.LOG", 1);
	if (h < 0)
		h = Fcreate("C:\\EWEB.LOG", 0);
	if (h < 0)
		return;
	Fseek(0, (short)h, 2);
	Fwrite((short)h, strlen(a), a);
	Fwrite((short)h, strlen(b), b);
	Fwrite((short)h, 2, "\r\n");
	Fclose((short)h);
}

int main(void)
{
	short msg[8];
	AEVENT ev;
	const char *s;
	appl_init();
	av_init("EWEB");
	note("ready ", av_name());
	for (;;) {
		evnt_multi_(MU_MESAG | MU_TIMER, 0, 0, 0, 100, msg, &ev);
		if ((ev.which & MU_MESAG) && av_message(msg, &s) == AV_START)
			note("start ", s);
	}
}
