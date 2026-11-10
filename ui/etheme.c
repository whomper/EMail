/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * theme.c - EMail's own colours on the family's palette (theme.h).
 */
#include "ui.h"
#include "theme.h"

/* the roles after the family's six, in the order of theme.h's enum:
   with 256 colours the soft shades, with 16 the darker of TOS's own */
static const THEME_ROLE mine[NPEN - P_BASE] = {
	{ {   0, 431, 788 }, {    0,    0,  533 }, 1, 1 },	/* P_ACCENT: blue */
	{ { 863, 149, 149 }, { 1000,    0,    0 }, 1, 1 },	/* P_FLAG: red */
	{ { 145, 400, 760 }, {    0,    0,  533 }, 1, 0 },	/* P_Q1: blue */
	{ { 145, 540, 290 }, {    0,  533,    0 }, 1, 0 },	/* P_Q2: green */
	{ { 600, 380, 150 }, {  533,  533,    0 }, 1, 0 },	/* P_Q3: brown */
};

/* with four colours (ST medium) VDI's red made blue while EMail runs, for
   links and unread counts, as is customary */
static const short blue4[3] = { 0, 0, 930 };

void email_theme_init(void)
{
	theme_init(opt.colour, mine, NPEN - P_BASE);
	theme_accent4(P_ACCENT, blue4);
}
