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
 * EMail starts on plain TOS) it writes its command line there and ends;
 * given --back=PATH (guidelines 4.6, the round trip), it ends as EWeb does
 * on the user's Quit: shel_write(1, 1, 0, PATH, " --resume"), written to
 * the log too (tests/hatari/resume_test.py).
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
		const char *c = cmd_line(), *b = strstr(c, "--back=");
		note("args ", c);
		/* as EWeb 1.27 does: the address ends at the first " --" (1.26
		   took the whole line); a file (the link page) is opened as it is
		   named, or not at all */
		{
			char f[200];
			const char *a = c;
			short i;
			while (*a == ' ')
				a++;
			if (!strncmp(a, "file:///", 8))
				a += 8;
			if (a[0] && a[1] == ':') {
				long h;
				for (i = 0; a[i] && !(a[i] == ' ' && a[i + 1] == '-' && a[i + 2] == '-') &&
				    i < (short)sizeof(f) - 1; i++)
					f[i] = a[i] == '/' ? '\\' : a[i];
				f[i] = 0;
				h = Fopen(f, 0);
				if (h >= 0)
					Fclose((short)h);
				note(h >= 0 ? "opened " : "can't open ", f);
			}
		}
		if (b) {
			char path[200], tail[16];
			short i;
			for (i = 0, b += 7; b[i] && b[i] != ' ' && i < (short)sizeof(path) - 1; i++)
				path[i] = b[i];
			path[i] = 0;
			appl_init();
			evnt_timer_(1500);	/* a moment as the browser, then Quit */
			tail[0] = 9;
			memcpy(tail + 1, " --resume", 10);
			note(shel_write(1, 1, 0, path, tail) ? "back " : "back failed ", path);
			appl_exit();
		}
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
