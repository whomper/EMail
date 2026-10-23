/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * theme.h - EMail's colours: a few, for meaning (selection, unread
 * counts, flags, quoted text), as modern mail programs use them. With 256
 * colours EMail sets its own soft shades in palette entries the desktop
 * doesn't use (and puts them back at the end); with 16 it takes the
 * standard ones; with 4 (ST medium) only links and unread counts are
 * blue (theme.c turns VDI's red blue); on monochrome nothing changes.
 */
#ifndef THEME_H
#define THEME_H

enum {
	P_TEXT,		/* ordinary text */
	P_DIM,		/* dates, sizes, header labels */
	P_LINE,		/* rules and frames */
	P_PANEL,	/* light background: dividers, idle title bars */
	P_HEAD,		/* the focused pane's title bar */
	P_SEL,		/* the selection bar */
	P_ACCENT,	/* unread counts */
	P_FLAG,		/* the flag mark */
	P_Q1, P_Q2, P_Q3,	/* quoted text by depth */
	NPEN
};

extern short pen[NPEN];
extern int colour;		/* colours are on (else black and white) */

void theme_init(void);		/* after the VDI is open and opt is read */
void theme_exit(void);		/* the palette back as it was */

#endif
