/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fakeweb.c - a stand-in for another program's receiving side, so EMail's
 * rigs run EMail alone (guidelines 1.2: no AERA app tests itself with
 * another AERA program). Never released.
 *
 * As an accessory it answers to FAKE_NAME (EWEB by default, for
 * tests/hatari/link_test.py; EHYPVIEW for help_test.py, built with
 * -DFAKE_NAME and -DFAKE_LOG): a VA_START is answered with AV_STARTED
 * (av_message does it), and its text goes into FAKE_LOG, for the test to
 * read. Started as a program (fakeweb.prg as C:\EWEB\EWEB.PRG, the browser
 * EMail starts on plain TOS) it writes its command line there and ends.
 */
#include <string.h>
#include <stdio.h>
#include "gem.h"
#include "tos.h"
#include "av.h"

#ifndef FAKE_NAME
#define FAKE_NAME "EWEB"
#endif
#ifndef FAKE_LOG
#define FAKE_LOG "C:\\EWEB.LOG"
#endif

extern short _is_acc;		/* crt0.S: started as a desk accessory */
const char *cmd_line(void);	/* tos.c */

static void note(const char *a, const char *b)
{
	long h = Fopen(FAKE_LOG, 1);
	if (h < 0)
		h = Fcreate(FAKE_LOG, 0);
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
	if (!_is_acc) {
		note("args ", cmd_line());
		return 0;
	}
	appl_init();
	av_init(FAKE_NAME);
	note("ready ", av_name());
	for (;;) {
		evnt_multi_(MU_MESAG | MU_TIMER, 0, 0, 0, 100, msg, &ev);
		if ((ev.which & MU_MESAG) && av_message(msg, &s) == AV_START)
			note("start ", s);
	}
}
