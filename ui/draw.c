/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * draw.c - text output for EMail's windows. Text is kept in logical
 * order; lines containing Hebrew go through bidi_visual() and, when the
 * paragraph is right-to-left, are aligned to the right edge.
 */
#include <string.h>
#include "ui.h"
#include "theme.h"
#include "bidi.h"
#include "text.h"

short text_pen = 1;		/* the colour of text that isn't inverted */

/* the VDI attributes last set here, so that a row's texts and fills
   don't set them again at each call: 25 calls for a row of the list on
   a Falcon at 16 MHz. Forgotten as each pane starts drawing (clip_on),
   and by code that sets them itself (attr_forget) */
static short a_wr = -1, a_tcol = -1, a_tfx = -1, a_fcol = -1, a_lcol = -1, a_fill;

void attr_forget(void)
{
	a_wr = a_tcol = a_tfx = a_fcol = a_lcol = -1;
	a_fill = 0;
}

static void wr_mode(short m)
{
	if (m != a_wr)
		vswr_mode(vdi_h, a_wr = m);
}

static void text_col(short c)
{
	if (c != a_tcol)
		vst_color(vdi_h, a_tcol = c);
}

static void text_effects(short fx)
{
	if (fx != a_tfx)
		vst_effects(vdi_h, a_tfx = fx);
}

static void line_colour(short c)
{
	if (c != a_lcol)
		vsl_color(vdi_h, a_lcol = c);
}

/* the last rectangle filled, when it was white (pen 0): bold and
   underline are drawn onto plain text there by text_fx, a third of what
   vst_effects costs TOS (common/ui/text.h) */
static GRECT white;
static int white_set;

void clip_on(GRECT *r)
{
	short pxy[4];
	attr_forget();
	white_set = 0;
	pxy[0] = r->x;
	pxy[1] = r->y;
	pxy[2] = r->x + r->w - 1;
	pxy[3] = r->y + r->h - 1;
	text_clip(vdi_h, pxy);
}

/* no clipping: the whole screen (the shared popup and About box) */
void clip_off(void)
{
	white_set = 0;
	text_noclip(vdi_h);
}

void fill(GRECT *r, short color)
{
	short pxy[4];
	if (r->w <= 0 || r->h <= 0)
		return;
	pxy[0] = r->x;
	pxy[1] = r->y;
	pxy[2] = r->x + r->w - 1;
	pxy[3] = r->y + r->h - 1;
	if (!a_fill) {		/* solid, no border: all of ours */
		vsf_interior(vdi_h, 1);
		vsf_perimeter(vdi_h, 0);
		a_fill = 1;
	}
	if (color != a_fcol)
		vsf_color(vdi_h, a_fcol = color);
	wr_mode(1);
	vr_recfl(vdi_h, pxy);
	white = *r;
	white_set = !color;
}

void hline(short x1, short x2, short y)
{
	short pxy[4];
	pxy[0] = x1;
	pxy[1] = y;
	pxy[2] = x2;
	pxy[3] = y;
	wr_mode(1);
	line_colour(pen[P_LINE]);
	v_pline(vdi_h, 2, pxy);
}

/* ST RAM for a bitmap to blit (the About box's icon): the screen can't
   be fed from TT RAM */
static unsigned short st_fallback[256];
static unsigned short *st_buf;

void *st_ram(long n)
{
	if (!st_buf) {
		void *p = Mxalloc(sizeof(st_fallback), 0);
		st_buf = (long)p > 0 ? p : st_fallback;
	}
	return n <= (long)sizeof(st_fallback) ? st_buf : 0;
}

void line_col(short x1, short y1, short x2, short y2, short color)
{
	short p[4];
	p[0] = x1;
	p[1] = y1;
	p[2] = x2;
	p[3] = y2;
	wr_mode(1);
	line_colour(color);
	v_pline(vdi_h, 2, p);
}

void frame(GRECT *r, short color)
{
	short p[10];
	p[0] = p[6] = p[8] = r->x;
	p[1] = p[3] = p[9] = r->y;
	p[2] = p[4] = r->x + r->w - 1;
	p[5] = p[7] = r->y + r->h - 1;
	wr_mode(1);
	line_colour(color);
	v_pline(vdi_h, 5, p);
}

short line_rtl(const char *s, long n)
{
	if (n > BIDI_MAX)
		n = BIDI_MAX;
	return bidi_has_rtl(s, (short)n) && bidi_is_rtl(s, (short)n);
}

/* Text is kept in the Atari character set, Hebrew at 0xC2-0xDC. Some
 * fonts (Israeli system fonts, many GDOS fonts) have the letters where
 * ISO-8859-8 or DOS 862 put them; Options > Font says which, and the
 * letters are moved there just before drawing. */
static const unsigned char heb_order[27] = {
	/* alef..nun (0xC2-0xCF) */ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 14, 16,
	/* samekh..tav (0xD0-0xD7) */ 17, 18, 20, 22, 23, 24, 25, 26,
	/* final nun, kaf, mem, pe, tsadi (0xD8-0xDC) */ 15, 10, 13, 19, 21
};

void heb_font_map(char *s, long n, short where)
{
	long i;
	unsigned char base = where == HEB_ISO ? 0xE0 : 0x80;
	if (where != HEB_ISO && where != HEB_DOS)
		return;
	for (i = 0; i < n; i++) {
		unsigned char c = (unsigned char)s[i];
		if (c >= 0xC2 && c <= 0xDC)
			s[i] = (char)(base + heb_order[c - 0xC2]);
	}
}

static int has_heb(const char *s, long n)
{
	long i;
	for (i = 0; i < n; i++)
		if ((unsigned char)s[i] >= 0xC2 && (unsigned char)s[i] <= 0xDC)
			return 1;
	return 0;
}

static int is_heb(unsigned char c)
{
	return c >= 0xC2 && c <= 0xDC;
}

/* a run of characters, in their cells (font_cells): in one call, made
   to fill them, or one character to a cell, spaces left out */
static short cells_now = FC_RUNS;	/* font_cells or font_cells_bold, by the effects set */

static void cells_at(short x, short y, const char *s, short n)
{
	short k;
	if (cells_now == FC_RUNS || n == 1) {
		text_n(vdi_h, x, y, s, n);
		return;
	}
	if (cells_now == FC_JUSTIFY) {
		v_justified_n(vdi_h, x, y, s, n, n * cw, 0, 1);
		return;
	}
	for (k = 0; k < n; k++)
		if (s[k] != ' ')
			v_gtext_n(vdi_h, x + k * cw, y, s + k, 1);
}

/* a vector font: the Hebrew letters by the font's own indices (NVDI's
   map mode 0), or in the system font, one cell each; the rest as usual */
static void runs_at(short x, short y, const char *s, short n)
{
	short i = 0;
	while (i < n) {
		short j = i, heb = is_heb((unsigned char)s[i]);
		while (j < n && is_heb((unsigned char)s[j]) == heb && j - i < 120)
			j++;
		if (!heb) {
			cells_at(x + i * cw, y, s + i, j - i);
		} else if (heb_mode == HM_UNICODE) {
			unsigned short w;
			short k;
			vst_map_mode(vdi_h, 0);
			for (k = i; k < j; k++) {
				w = heb_idx[(unsigned char)s[k] - 0xC2];
				v_gtext_w(vdi_h, x + k * cw, y, &w, 1);
			}
			vst_map_mode(vdi_h, 1);
		} else {
			/* the system font, each letter centred in its cell */
			short keep_cw = cw, keep_ch = ch, dw, dh, k;
			char c[BIDI_MAX];
			vst_font(vdi_h, 1);
			vst_height(vdi_h, keep_ch >= 16 ? 13 : 6, &dw, &dh);
			vst_alignment(vdi_h, 0, 5);
			memcpy(c, s + i, j - i);
			heb_font_map(c, j - i, opt.hebfont);
			for (k = 0; k < j - i; k++)
				v_gtext_n(vdi_h, x + (i + k) * keep_cw + (keep_cw - dw) / 2,
					  y + (keep_ch - dh) / 2, c + k, 1);
			font_reselect();
			cw = keep_cw;
			ch = keep_ch;
		}
		i = j;
	}
}

/* n characters already in screen order, from x: the font's Hebrew
   places, the flags' effects and more (fx: 8 underlines) */
static void visual_at(short x, short y, const char *out, short n, short flags, short fx)
{
	char m[BIDI_MAX];
	short i, effects = ((flags & TX_BOLD) ? 1 : (flags & TX_LIGHT) ? 2 : 0) | fx;
	short col = (flags & TX_INVERSE) ? 0 : text_pen;
	if (heb_mode != HM_MAP && has_heb(out, n)) {
		wr_mode(2);
		text_col(col);
		text_effects(effects);
		cells_now = (effects & 1) ? font_cells_bold : font_cells;
		runs_at(x, y, out, n);
		text_effects(0);	/* off again: others draw text plainly (fontsel's sample) */
		return;
	}
	if (opt.hebfont != HEB_ATARI && has_heb(out, n)) {
		memcpy(m, out, n);
		out = m;
		heb_font_map(m, n, opt.hebfont);
	}
	wr_mode(2);
	text_col(col);
	/* on white, in the window's text: transparent there is TOS's
	   MD_REPLACE, so bold and underline go on afterwards */
	if (effects && !(flags & TX_INVERSE) && white_set && x >= white.x && y >= white.y &&
	    x + n * cw < white.x + white.w && y + ch <= white.y + white.h && text_fx_fast(effects, 1)) {
		text_effects(0);
		text_n(vdi_h, x, y, out, n);
		text_fx(vdi_h, x, y, out, n, effects, col);
		a_wr = -1;	/* text_fx sets its own */
		if (effects & 8)
			a_lcol = col;
		return;
	}
	text_effects(effects);
	cells_now = (effects & 1) ? font_cells_bold : font_cells;
	for (i = 0; i < n; i += 120) {
		short k = n - i > 120 ? 120 : (short)(n - i);
		cells_at(x + i * cw, y, out + i, k);
	}
	text_effects(0);
}

void text_at(short x, short y, const char *s, long n, short cols, short flags)
{
	text_marked_at(x, y, s, n, cols, flags, 0);
}

void text_marked_at(short x, short y, const char *s, long n, short cols, short flags, const char *mark)
{
	char vis[BIDI_MAX], vm[BIDI_MAX];
	short pos[BIDI_MAX];
	unsigned char odd[BIDI_MAX];
	const char *out = s, *om = mark;
	short i, j;
	if (cols <= 0)
		return;
	if (n > cols)
		n = cols;
	if (n > BIDI_MAX)
		n = BIDI_MAX;
	if (n <= 0)
		return;
	if (bidi_has_rtl(s, (short)n)) {
		short rtl = (flags & TX_LTR) ? 0 : bidi_is_rtl(s, (short)n);
		if (mark) {
			/* the marks go where their characters are drawn */
			bidi_visual_map(s, (short)n, rtl, vis, pos, odd);
			for (i = 0; i < n; i++)
				vm[pos[i]] = mark[i];
			om = vm;
		} else {
			bidi_visual(s, (short)n, rtl, vis);
		}
		out = vis;
		if (rtl && (flags & TX_RIGHT))
			x += (cols - (short)n) * cw;
	}
	if (!om) {
		visual_at(x, y, out, (short)n, flags, 0);
		return;
	}
	/* runs marked and not: a link in the accent colour, underlined */
	for (i = 0; i < n; i = j) {
		short keep = text_pen;
		for (j = i; j < n && !om[j] == !om[i]; j++)
			;
		if (om[i] && !(flags & TX_INVERSE))
			text_pen = pen[P_ACCENT];
		visual_at(x + i * cw, y, out + i, j - i, flags, om[i] ? 8 : 0);
		text_pen = keep;
	}
}

short text_index(const char *s, long n, short cols, short flags, short col)
{
	char vis[BIDI_MAX];
	short pos[BIDI_MAX], i, rtl;
	unsigned char odd[BIDI_MAX];
	if (n > cols)
		n = cols;
	if (n > BIDI_MAX)
		n = BIDI_MAX;
	if (n <= 0 || col < 0)
		return -1;
	if (!bidi_has_rtl(s, (short)n))
		return col < n ? col : -1;
	rtl = (flags & TX_LTR) ? 0 : bidi_is_rtl(s, (short)n);
	bidi_visual_map(s, (short)n, rtl, vis, pos, odd);
	if (rtl && (flags & TX_RIGHT))
		col -= cols - (short)n;
	for (i = 0; i < n; i++)
		if (pos[i] == col)
			return i;
	return -1;
}

long wrap_text(const char *s, long n, short width, long *breaks, long max)
{
	long count = 0, i = 0;
	if (width < 8)
		width = 8;
	/* a line ends at its '\n' (found by memchr, up to the place just
	   past the width), else after its last space: the characters aren't
	   gone through one by one, which an ST takes 165 ms over for a
	   letter of 10 KB */
	while (i <= n && count < max) {
		long start = i, j, lim = start + width;
		const char *nl;
		breaks[count++] = start;
		nl = start < n ? memchr(s + start, '\n', (lim < n ? lim + 1 : n) - start) : 0;
		if (nl) {
			i = nl - s + 1;
			if (i == n)
				break;
			continue;
		}
		if (lim >= n)
			break;
		/* too long: break after the last space, or hard at width */
		for (j = lim - 1; j > start && s[j] != ' '; j--)
			;
		i = j > start ? j + 1 : lim;
	}
	return count;
}
