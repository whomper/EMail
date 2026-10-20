/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * about.c - EMail's About box: the AERA programs' shared one
 * (common/ui/aboutbox.c), with how EMail is connected and the memory it
 * runs in.
 */
#include <string.h>
#include <stdio.h>
#include "ui.h"
#include "aboutbox.h"
#include "iconify.h"
#include "icon.h"
#include "icon16.h"
#include "../src/version.h"
#include "../src/plat.h"

void dlg_about(void)
{
	static char conn[80], where[60];
	char avail[60];
	short i, online = 0;
#ifdef MAIL_ST
#define EDITION "The ST edition, EMAILST.PRG"
#else
#define EDITION 0
#endif
	ABOUT a = { "EMail", mail_version, "E-mail for the Atari ST, TT and Falcon", { conn, where, EDITION }, { 0, 0, 0 },
		    icon_bits, icon16_px, icon16_rgb };
	for (i = 0; i < naccts; i++)
		if (accts[i]->im)
			online = 1;
	snprintf(conn, sizeof(conn), "%s  \xF9  %s  \xF9  %s",
		 strcmp(net_stack(), "none") ? net_stack() : "no network",
		 opt.falcon ? (strcmp(falcon_dsp_state(), "DSP") ? "Falcon" : "Falcon/DSP") : "Gateway",
		 opt.offline ? "offline" : online ? "online" : "ready");
	/* the memory it runs in: TT RAM when there is some */
	pf_memory(where, sizeof(where), avail, sizeof(avail));
	about_show(&a);
}

/* An iconified window (common/ui/iconify.c): the icon, and under the
   main window's how many messages in the inboxes are unread. From a
   redraw message, area; from EMail itself (0), only when that changed */
void icon_window(short h, GRECT *area)
{
	static char shown[16] = "-";
	char s[16] = "";
	long n = 0;
	short i;
	if (h != main_h) {
		icon_redraw(h, area, icon_bits, icon16_px, "");
		return;
	}
	for (i = 0; i < naccts; i++) {
		FINFO *in = folder_role(accts[i], FR_INBOX);
		if (in && in->unread > 0)
			n += in->unread;
	}
	if (n > 0)
		snprintf(s, sizeof(s), "%ld new", n);
	if (!area && !strcmp(s, shown))
		return;
	strcpy(shown, s);
	icon_redraw(h, area, icon_bits, icon16_px, s);
}
