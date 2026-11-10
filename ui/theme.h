/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * theme.h - EMail's colours: the family's UI palette (common/ui/theme.h:
 * text, dim, line, panel, head, the one selection colour; D42) and EMail's
 * own meaning colours after them: unread counts and links, the flag, and
 * quoted text by depth. With 4 colours (ST medium) only the accent is
 * blue (theme_accent4); on monochrome nothing changes.
 */
#ifndef THEME_H
#define THEME_H

#include "../common/ui/theme.h"

enum {
	P_ACCENT = P_BASE,	/* unread counts, links */
	P_FLAG,			/* the flag mark */
	P_Q1, P_Q2, P_Q3,	/* quoted text by depth */
	NPEN
};

void email_theme_init(void);	/* after the VDI is open and opt is read */

#endif
