/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fakecal.c - a stand-in for ECal's receiving side (the AERA guidelines,
 * 4.1), for tests/hatari/invite_test.py: an accessory that answers to the
 * name ECAL (as ECAL.ACC). A VA_START naming a file is answered with
 * AV_STARTED (av_message does it), and the name, the file's size and its
 * SUMMARY line go into C:\ECAL.LOG, for the test to read.
 */
#include <string.h>
#include <stdio.h>
#include "gem.h"
#include "tos.h"
#include "av.h"

static void note(const char *s)
{
	long h = Fopen("C:\\ECAL.LOG", 1);
	if (h < 0)
		h = Fcreate("C:\\ECAL.LOG", 0);
	if (h < 0)
		return;
	Fseek(0, (short)h, 2);
	Fwrite((short)h, strlen(s), s);
	Fwrite((short)h, 2, "\r\n");
	Fclose((short)h);
}

static char buf[4096];

static void got(const char *s)
{
	char name[200], line[300];
	long h, n = 0;
	const char *p;
	if (!av_arg(s, name, sizeof(name)))
		return;
	h = Fopen(name, 0);
	if (h >= 0) {
		n = Fread((short)h, sizeof(buf) - 1, buf);
		Fclose((short)h);
	}
	buf[n > 0 ? n : 0] = 0;
	p = strstr(buf, "SUMMARY");
	snprintf(line, sizeof(line), "start %s, %ld bytes, %.80s", name, n, p ? p : "no SUMMARY");
	if ((p = strchr(line, '\r')) != 0)
		*(char *)p = 0;
	note(line);
}

int main(void)
{
	short msg[8];
	AEVENT ev;
	const char *s;
	appl_init();
	av_init("ECAL");
	note(av_name());
	for (;;) {
		evnt_multi_(MU_MESAG | MU_TIMER, 0, 0, 0, 100, msg, &ev);
		if ((ev.which & MU_MESAG) && av_message(msg, &s) == AV_START)
			got(s);
	}
}
