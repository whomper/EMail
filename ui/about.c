/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * about.c - EMail's About box: the AERA programs' shared one
 * (common/ui/aboutbox.c), with its status lines in the family's words
 * (common/ui/aboutline.c).
 */
#include <string.h>
#include <stdio.h>
#include "ui.h"
#include "aboutbox.h"
#include "iconify.h"
#include "icon.h"
#include "icon16.h"
#include "icon256.h"
#include "../src/version.h"
#include "../src/plat.h"
#include "aboutline.h"

/* unread mail in the accounts' inboxes, as the iconified window has it */
static long inbox_unread(void)
{
	long n = 0;
	short i;
	for (i = 0; i < naccts; i++) {
		FINFO *in = folder_role(accts[i], FR_INBOX);
		if (in && in->unread > 0)
			n += in->unread;
	}
	return n;
}

/* the three status lines in the family's words (the guidelines, 1.3;
   common/ui/aboutline.c): the connection, EMail's accounts and unread
   mail, the memory. TLS.OVL's version and the DSP are in EMAIL.LOG */
void dlg_about(void)
{
	static char conn[80], mine[60], mem[80];
	short i, online = 0;
	long unread = inbox_unread();
	ABOUT a = { "EMail", mail_version, "E-mail for the Atari ST, TT and Falcon", { conn, mine, mem },
		    { 0, 0, 0 }, icon_bits, icon16_px, icon16_rgb, icon256_px, icon256_rgb };
	for (i = 0; i < naccts; i++)
		if (accts[i]->im)
			online = 1;
	about_net_line(conn, sizeof(conn), net_stack(), opt.falcon ? ABOUT_DIRECT : ABOUT_GATEWAY,
		       opt.offline ? ABOUT_OFFLINE : online ? ABOUT_ONLINE : ABOUT_READY);
	if (naccts)
		snprintf(mine, sizeof(mine), "%d account%s  \xF9  %s unread", naccts, naccts == 1 ? "" : "s",
			 num(unread));
	else
		str_copy(mine, "No account yet", sizeof(mine));
	about_mem_line(mem, sizeof(mem));
	about_show(&a);
}

/* An iconified window (common/ui/iconify.c): the icon, and under the
   main window's how many messages in the inboxes are unread. From a
   redraw message, area; from EMail itself (0), only when that changed */
void icon_window(short h, GRECT *area)
{
	static char shown[16] = "-";
	char s[16] = "";
	long n;
	if (h != main_h) {
		icon_redraw(h, area, icon_bits, icon16_px, "");
		return;
	}
	n = inbox_unread();
	if (n > 0)
		snprintf(s, sizeof(s), "%ld new", n);
	if (!area && !strcmp(s, shown))
		return;
	strcpy(shown, s);
	icon_redraw(h, area, icon_bits, icon16_px, s);
}
