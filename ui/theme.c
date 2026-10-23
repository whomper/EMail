/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * theme.c - EMail's colours (see theme.h).
 */
#include <string.h>
#include "ui.h"
#include "theme.h"

short pen[NPEN];
int colour;

/* soft shades for 256-colour screens, VDI 0-1000 RGB */
static const short shade[NPEN][3] = {
	{ 133, 145, 165 },	/* P_TEXT: near black, a little blue */
	{ 420, 447, 502 },	/* P_DIM: slate grey */
	{ 780, 800, 830 },	/* P_LINE */
	{ 922, 929, 941 },	/* P_PANEL: light grey */
	{ 200, 255, 333 },	/* P_HEAD: dark slate */
	{   0, 471, 851 },	/* P_SEL: blue */
	{   0, 431, 788 },	/* P_ACCENT: blue */
	{ 863, 149, 149 },	/* P_FLAG: red */
	{ 145, 400, 760 },	/* P_Q1: blue */
	{ 145, 540, 290 },	/* P_Q2: green */
	{ 600, 380, 150 },	/* P_Q3: brown */
};
#define FIRST_PEN 16		/* past the desktop's sixteen */

/* with sixteen colours: the standard TOS ones, the darker shades (12
   navy, 11 dark green, 14 brown) rather than the loud full ones */
static const short stock[NPEN] = { 1, 9, 9, 8, 1, 12, 12, 2, 12, 11, 14 };

static short saved[NPEN][3];
static int set_rgb;

/* with four colours (ST medium: white, black, red, green): VDI's red made
   blue while EMail runs, for links and unread counts, as is customary */
#define PEN4_BLUE 2
static const short blue4[3] = { 0, 0, 930 };
static short saved4[3];
static int set4;

void theme_init(void)
{
	short i;
	theme_exit();
	colour = opt.colour && planes >= 4;
	for (i = 0; i < NPEN; i++)
		pen[i] = i == P_PANEL ? 0 : 1;	/* black and white */
	if (opt.colour && planes == 2) {
		short rgb[3];
		vq_color(vdi_h, PEN4_BLUE, saved4);
		memcpy(rgb, blue4, sizeof(rgb));
		vs_color(vdi_h, PEN4_BLUE, rgb);
		pen[P_ACCENT] = PEN4_BLUE;
		set4 = 1;
	}
	if (!colour)
		return;
	if (planes >= 8) {
		for (i = 0; i < NPEN; i++) {
			short rgb[3];
			vq_color(vdi_h, FIRST_PEN + i, saved[i]);
			rgb[0] = shade[i][0];
			rgb[1] = shade[i][1];
			rgb[2] = shade[i][2];
			vs_color(vdi_h, FIRST_PEN + i, rgb);
			pen[i] = FIRST_PEN + i;
		}
		set_rgb = 1;
	} else {
		for (i = 0; i < NPEN; i++)
			pen[i] = stock[i];
	}
}

void theme_exit(void)
{
	short i;
	if (set4) {
		vs_color(vdi_h, PEN4_BLUE, saved4);
		set4 = 0;
	}
	if (!set_rgb)
		return;
	for (i = 0; i < NPEN; i++)
		vs_color(vdi_h, FIRST_PEN + i, saved[i]);
	set_rgb = 0;
}
