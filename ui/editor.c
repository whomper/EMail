/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * editor.c - the compose window: a plain-text editor holding the
 * header lines (To, Cc, Bcc, Subject, Attach) above the body. Lines are
 * kept in logical order; Hebrew lines are shown right to left with the
 * cursor at the right visual place, and F10 switches the keyboard to
 * the Hebrew layout. Body lines wrap at the configured column.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui.h"
#include "../src/mail.h"
#include "../src/compose.h"
#include "../src/mime.h"
#include "bidi.h"
#include "util.h"
#include "clip.h"
#include "av.h"

typedef struct {
	char *s;
	short n, cap;
} ELINE;

#define MAXLINES 4000

/* the lines, a table that grows as the message does (all 4000 at once
   took 32 KB a 1 MB Atari doesn't have to spare) */
static ELINE *ln;
static long nl, ncap;
static long cy;
static short cx;
static short dirty;
static ACCOUNT *e_acct;
static char *e_irt, *e_refs;
static unsigned long e_reply_uid;
static unsigned ed_count;		/* messages opened: a pick is for one */
static FINFO *e_reply_fi;

int editor_dirty(void)
{
	return w_editor.h > 0 && dirty;
}

static int grow_line(ELINE *l, short need)
{
	if (need + 1 <= l->cap)
		return 1;
	{
		short cap = l->cap ? l->cap : 16;
		char *n;
		while (cap < need + 1)
			cap *= 2;
		n = realloc(l->s, cap);
		if (!n)
			return 0;
		l->s = n;
		l->cap = cap;
	}
	return 1;
}

static int ins_line(long at, const char *s, short n)
{
	ELINE *l;
	if (nl >= ncap) {
		long cap = ncap * 2 > MAXLINES ? MAXLINES : ncap * 2;
		ELINE *more = nl < MAXLINES ? realloc(ln, cap * sizeof(ELINE)) : 0;
		if (!more)
			return 0;
		ln = more;
		ncap = cap;
	}
	memmove(ln + at + 1, ln + at, (nl - at) * sizeof(ELINE));
	nl++;
	l = &ln[at];
	memset(l, 0, sizeof(*l));
	if (!grow_line(l, n))
		return 0;
	memcpy(l->s, s, n);
	l->n = n;
	l->s[n] = 0;
	return 1;
}

static void del_line(long at)
{
	free(ln[at].s);
	memmove(ln + at, ln + at + 1, (nl - at - 1) * sizeof(ELINE));
	nl--;
}

static void free_all(void)
{
	long i;
	for (i = 0; i < nl; i++)
		free(ln[i].s);
	nl = 0;
	free(e_irt);
	free(e_refs);
	e_irt = e_refs = 0;
}

/* index of the blank line that ends the header lines, or -1 */
static long header_end(void)
{
	long i;
	for (i = 0; i < nl; i++)
		if (ln[i].n == 0)
			return i;
	return -1;
}

static void eb_layout(WIN *w);

static int in_body(long line)
{
	long he = header_end();
	return he >= 0 && line > he;
}

static void set_title(void)
{
	char t[100];
	snprintf(t, sizeof(t), " %s - %s%s ", "New message", e_acct ? e_acct->name : "", dirty ? " *" : "");
	win_title(&w_editor, t);
}

void editor_open(ACCOUNT *a, char *text, const char *irt, const char *refs)
{
	const char *p;
	if (!text) {
		alert(1, "[1][Not enough memory to|start this message.][ OK ]");
		return;
	}
	if (w_editor.h > 0 && dirty) {
		if (alert(2, "[2][You are still writing a message.|Discard it?][Discard|Cancel]") != 1) {
			free(text);
			win_top(&w_editor);
			return;
		}
	}
	free_all();
	if (!ln && (ln = malloc(128 * sizeof(ELINE))) != 0)
		ncap = 128;
	if (!ln) {
		free(text);
		alert(1, "[1][Not enough memory to|start this message.][ OK ]");
		return;
	}
	e_acct = a;
	e_irt = irt && *irt ? strdup(irt) : 0;
	e_refs = refs && *refs ? strdup(refs) : 0;
	e_reply_uid = irt && *irt ? cur_uid : 0;
	e_reply_fi = cur_finfo;
	for (p = text; ; ) {
		const char *e = strchr(p, '\n');
		short n = (short)(e ? e - p : (long)strlen(p));
		ins_line(nl, p, n);
		if (!e)
			break;
		p = e + 1;
	}
	free(text);
	/* cursor: on an empty To: line, else at the start of the text */
	cy = 0;
	cx = 0;
	if (nl && ln[0].n > 4) {
		long he = header_end();
		cy = he >= 0 && he + 1 < nl ? he + 1 : 0;
	} else if (nl) {
		cx = ln[0].n;
	}
	dirty = 0;
	w_editor.top = 0;
	w_editor.total = nl;
	set_title();
	ed_count++;			/* a pick for the last message isn't for this one */
	win_open(&w_editor);
	eb_layout(&w_editor);		/* the bar's height, before the rows are counted */
	win_sliders(&w_editor);
	win_redraw(&w_editor, 0);
}

/* visual column of logical column col within s[0..n), laid out the way
   text_at() does it in a field of `cols` (right-aligned when it is a
   Hebrew paragraph and `right` is set) */
static short visual_col(const char *s, short n, short col, short cols, short right, short ltr)
{
	char vis[BIDI_MAX];
	short pos[BIDI_MAX];
	unsigned char odd[BIDI_MAX];
	short m = n > cols ? cols : n, rtl, xoff;
	if (m > BIDI_MAX)
		m = BIDI_MAX;
	if (m <= 0 || !bidi_has_rtl(s, m))
		return col;
	rtl = ltr ? 0 : bidi_is_rtl(s, m);
	xoff = (rtl && right) ? cols - m : 0;
	bidi_visual_map(s, m, rtl, vis, pos, odd);
	if (col < m)
		return xoff + pos[col];
	return rtl ? (right ? xoff - 1 : -1) : m;
}

static short rcpt_cursor(long line, short cols);

/* where the cursor sits on screen: column relative to the work area */
static short cursor_col(long line, short col, short cols, short *hoff)
{
	ELINE *l = &ln[line];
	long he = header_end();
	short off = 0, rc = line == cy && col == cx ? rcpt_cursor(line, cols) : -1;
	if (rc >= 0) {
		*hoff = 0;
		return rc;
	}
	if (col > cols - 2)
		off = col - cols + 2;
	*hoff = off;
	if ((he < 0 || line < he) && !off) {
		/* header line: "Label:" then the value one column further */
		const char *colon = memchr(l->s, ':', l->n);
		if (colon) {
			short lab = (short)(colon - l->s + 1), v = lab;
			if (v < l->n && l->s[v] == ' ')
				v++;
			if (col < v)
				return col;
			return lab + 1 + visual_col(l->s + v, l->n - v, col - v, cols - lab - 1, 0,
						    strncasecmp(l->s, "Subject:", 8) != 0);
		}
	}
	return visual_col(l->s + off, l->n - off, col - off, cols, 1, 0);
}

/* ---------------- recipients ----------------
 * To:, Cc: and Bcc: keep "Maya Carter <maya@x.example>, d@e" as typed or
 * picked, and that is what is sent. On the screen each recipient but the
 * one the cursor is in shows framed, by the name EContact has for it, else
 * as its address; the address is its tooltip. */
#define MAXRCPT 40
typedef struct {
	short rs, re;		/* its text in the line, without the spaces round it */
	short ds, dn;		/* where it shows (columns from the value's start), how wide */
	short chip;		/* shown framed, by name */
	char addr[PEOPLE_ADDR];
	char show[PEOPLE_NAME];
} RCPT;

static int addr_line(long line)
{
	long he = header_end();
	ELINE *l = &ln[line];
	if (line >= (he >= 0 ? he : nl))
		return 0;
	return !strncasecmp(l->s, "To:", 3) || !strncasecmp(l->s, "Cc:", 3) || !strncasecmp(l->s, "Bcc:", 4);
}

static short value_at(long line);

/* the recipients of an address line and where each shows: how many; *dend
   the width of all of it, from the value's start */
static int rcpt_layout(long line, RCPT *r, int max, short *dend)
{
	ELINE *l = &ln[line];
	short i = value_at(line), pe = i, pd = 0, k = 0;
	while (k < max) {
		short start = i, q = 0, ang = 0;
		RCPT *t = &r[k];
		char one[256];
		while (i < l->n && (q || ang || l->s[i] != ',')) {
			if (l->s[i] == '"')
				q = !q;
			else if (!q && l->s[i] == '<')
				ang = 1;
			else if (!q && l->s[i] == '>')
				ang = 0;
			i++;
		}
		t->rs = start;
		while (t->rs < i && l->s[t->rs] == ' ')
			t->rs++;
		t->re = i;
		while (t->re > t->rs && l->s[t->re - 1] == ' ')
			t->re--;
		t->addr[0] = t->show[0] = 0;
		t->chip = 0;
		if (t->re > t->rs) {
			short n = t->re - t->rs < (short)sizeof(one) ? t->re - t->rs : (short)sizeof(one) - 1;
			memcpy(one, l->s + t->rs, n);
			one[n] = 0;
			addr_split(one, 0, 0, t->addr, sizeof(t->addr));
			/* framed unless the cursor is in it (or just after it) */
			t->chip = strchr(t->addr, '@') && !(line == cy && cx > t->rs && cx <= t->re);
			if (t->chip)
				str_copy(t->show, person_shown(t->addr), sizeof(t->show));
		}
		t->ds = pd + (t->rs - pe);	/* what is between shows as it is */
		t->dn = t->chip ? (short)strlen(t->show) : t->re - t->rs;
		pd = t->ds + t->dn;
		pe = t->re;
		k++;
		if (i >= l->n)
			break;
		i++;			/* the comma */
	}
	if (dend)
		*dend = pd + (l->n - pe);
	return k;
}

/* the column a recipient line shows the line's column c at (from the
   value's start) */
static short rcpt_col(RCPT *r, int n, short v, short c)
{
	short pe = v, pd = 0;
	int k;
	for (k = 0; k < n; k++) {
		if (c < r[k].rs)
			return pd + (c - pe);
		if (c <= r[k].re)
			return r[k].chip ? r[k].ds + (c == r[k].rs ? 0 : r[k].dn) : r[k].ds + (c - r[k].rs);
		pd = r[k].ds + r[k].dn;
		pe = r[k].re;
	}
	return pd + (c - pe);
}

/* and back: the line's column for a column on the screen; on a framed
   recipient, its end (the cursor in it shows it as typed, to change it) */
static short rcpt_raw(RCPT *r, int n, short v, short d, short len)
{
	short pe = v, pd = 0, c;
	int k;
	for (k = 0; k < n; k++) {
		if (d < r[k].ds)
			return pe + (d - pd);
		if (d < r[k].ds + r[k].dn)
			return r[k].chip ? r[k].re : r[k].rs + (d - r[k].ds);
		pd = r[k].ds + r[k].dn;
		pe = r[k].re;
	}
	c = pe + (d - pd);
	return c > len ? len : c;
}

/* an address line drawn as recipients; 0 when the cursor would be off
   the right edge that way (the line is then drawn as typed, scrolled) */
static int draw_rcpts(long line, short x0, short y, short cols)
{
	ELINE *l = &ln[line];
	RCPT r[MAXRCPT];
	const char *colon = memchr(l->s, ':', l->n);
	short lab, v, dend, room, x, pe, pd;
	int n, k;
	if (!colon)
		return 0;
	lab = (short)(colon - l->s + 1);
	v = value_at(line);
	n = rcpt_layout(line, r, MAXRCPT, &dend);
	room = cols - lab - 1;
	if (line == cy && rcpt_col(r, n, v, cx) > room - 2)
		return 0;
	text_at(x0, y, l->s, lab, cols, TX_BOLD);
	x = x0 + (lab + 1) * cw;
	pe = v;
	pd = 0;
	for (k = 0; k <= n; k++) {
		/* what lies before the recipient (", "), then the recipient */
		short gap = (k < n ? r[k].rs : l->n) - pe, left = room - pd;
		if (gap > 0 && left > 0)
			text_at(x + pd * cw, y, l->s + pe, gap, left, TX_LTR);
		if (k == n)
			break;
		left = room - r[k].ds;
		if (left > 0 && r[k].dn > 0) {
			if (r[k].chip) {
				GRECT f;
				short w = r[k].dn < left ? r[k].dn : left;
				text_at(x + r[k].ds * cw, y, r[k].show, r[k].dn, w, TX_LTR);
				f.x = x + r[k].ds * cw - 2;
				f.y = y;
				f.w = w * cw + 4;
				f.h = ch;
				frame(&f, 1);
			} else {
				text_at(x + r[k].ds * cw, y, l->s + r[k].rs, r[k].dn, left, TX_LTR);
			}
		}
		pd = r[k].ds + r[k].dn;
		pe = r[k].re;
	}
	return 1;
}

/* where the cursor shows on an address line drawn as recipients, or -1 */
static short rcpt_cursor(long line, short cols)
{
	ELINE *l = &ln[line];
	RCPT r[MAXRCPT];
	const char *colon = memchr(l->s, ':', l->n);
	short lab, v, d;
	int n;
	if (!colon || !addr_line(line))
		return -1;
	lab = (short)(colon - l->s + 1);
	v = value_at(line);
	n = rcpt_layout(line, r, MAXRCPT, 0);
	if (cx < v)
		return cx;
	d = rcpt_col(r, n, v, cx);
	if (d > cols - lab - 1 - 2)
		return -1;
	return lab + 1 + d;
}

/* ---------------- the button bar ----------------
 * EFTP's toolbar, for the message being written: the commands of the
 * right-click menu, in its order, and of the Message menu; each button's
 * key in its tooltip. It is the window's fixed head (w->head_h). */
enum { EB_SEND, EB_OUTBOX, EB_ATTACH, EB_ABOOK, NEB };
static GRECT eb[NEB];
static short eb_narrow;

static int on_rect(GRECT *r, short x, short y)
{
	return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

static const char *eb_label(int i)
{
	switch (i) {
	case EB_SEND: return "Send";
	case EB_OUTBOX: return "Outbox";
	case EB_ATTACH: return eb_narrow ? "Attach" : "Attach...";
	case EB_ABOOK: return eb_narrow ? "Address" : "Address book...";
	}
	return "";
}

static const char *const eb_tip[NEB] = {
	"Send the message now (\x01^D)",
	"Put it in the Outbox, to go with the next check (^S)",
	"Attach a file (^T)",
	"An address from the address book (^B)",
};

static void eb_layout(WIN *w)
{
	short x = w->work.x + 4, i, bw;
	w->head_h = ch + 8;
	eb_narrow = w->work.w < 52 * cw;
	for (i = 0; i < NEB; i++) {
		bw = ((short)strlen(eb_label(i)) + 2) * cw;
		eb[i].x = x;
		eb[i].y = w->work.y + 3;
		eb[i].w = bw;
		eb[i].h = ch + 2;
		/* a gap between sending and adding to it */
		x += bw + (i == EB_OUTBOX ? cw : 2);
	}
}

static void eb_draw(int i)
{
	GRECT r = eb[i];
	const char *l = eb_label(i);
	short n = (short)strlen(l);
	fill(&r, 0);
	frame(&r, 1);
	text_at(r.x + (r.w - n * cw) / 2, r.y + 1, l, n, n, TX_LTR);
}

static void draw_bar(WIN *w, GRECT *clip)
{
	GRECT r;
	int i;
	r.x = w->work.x;
	r.y = w->work.y;
	r.w = w->work.w;
	r.h = w->head_h;
	if (clip->y >= r.y + r.h || clip->y + clip->h <= r.y)
		return;
	for (i = 0; i < NEB; i++)
		eb_draw(i);
	hline(w->work.x, w->work.x + w->work.w - 1, w->work.y + w->head_h - 1);
}

void editor_bar_redraw(void)
{
	GRECT r;
	if (w_editor.h <= 0)
		return;
	r = w_editor.work;
	r.h = w_editor.head_h;
	win_redraw(&w_editor, &r);
}

/* a click on the bar: the button acts at once, shown pressed a moment */
static void bar_click(short mx, short my)
{
	int i;
	for (i = 0; i < NEB; i++) {
		GRECT r = eb[i];
		short p[4];
		if (!on_rect(&r, mx, my))
			continue;
		wind_update(BEG_UPDATE);
		graf_mouse(M_OFF, 0);
		clip_on(&r);
		p[0] = r.x + 1;
		p[1] = r.y + 1;
		p[2] = r.x + r.w - 2;
		p[3] = r.y + r.h - 2;
		vswr_mode(vdi_h, 3);
		vsf_interior(vdi_h, 1);
		vsf_color(vdi_h, 1);
		vr_recfl(vdi_h, p);
		vswr_mode(vdi_h, 1);
		graf_mouse(M_ON, 0);
		wind_update(END_UPDATE);
		evnt_timer_(80);
		editor_bar_redraw();
		switch (i) {
		case EB_SEND: editor_send(1); break;
		case EB_OUTBOX: editor_send(0); break;
		case EB_ATTACH: editor_attach(); break;
		case EB_ABOOK: editor_insert_address(); break;
		}
		return;
	}
}

/* the tooltip for a button of the bar */
int editor_tip(short mx, short my, char *out, int size)
{
	int i;
	if (w_editor.h <= 0 || !win_is_top(&w_editor))
		return 0;
	eb_layout(&w_editor);
	for (i = 0; i < NEB; i++)
		if (on_rect(&eb[i], mx, my)) {
			str_copy(out, eb_tip[i], size);
			return 1;
		}
	/* a recipient shown by name: its address */
	if (on_rect(&w_editor.work, mx, my) && my >= w_editor.work.y + w_editor.head_h) {
		long line = w_editor.top + (my - w_editor.work.y - w_editor.head_h) / ch;
		short cols = w_editor.work.w / cw - 1, col = (mx - w_editor.work.x - cw / 2) / cw;
		if (line < nl && addr_line(line) && (line != cy || rcpt_cursor(line, cols) >= 0)) {
			RCPT r[MAXRCPT];
			const char *colon = memchr(ln[line].s, ':', ln[line].n);
			short d = col - (colon ? (short)(colon - ln[line].s + 2) : 0);
			int n = rcpt_layout(line, r, MAXRCPT, 0), k;
			for (k = 0; k < n; k++)
				if (r[k].chip && d >= r[k].ds && d < r[k].ds + r[k].dn &&
				    strcmp(r[k].show, r[k].addr)) {
					str_copy(out, r[k].addr, size);
					return 1;
				}
		}
	}
	return 0;
}

/* EContact's names came, or changed: the address lines again */
void editor_names_came(void)
{
	long he;
	if (w_editor.h <= 0)
		return;
	he = header_end();
	win_redraw_lines(&w_editor, 0, he >= 0 ? he : nl);
}

static void draw(WIN *w, GRECT *clip)
{
	long i, rows = win_rows(w) + 1, he = header_end();
	short cols = w->work.w / cw - 1, x0 = w->work.x + cw / 2;
	eb_layout(w);
	fill(clip, 0);
	draw_bar(w, clip);
	for (i = w->top; i < nl && i < w->top + rows; i++) {
		ELINE *l = &ln[i];
		short y = w->work.y + w->head_h + (short)((i - w->top) * ch), hoff = 0;
		if (i == cy && cx > cols - 2)
			hoff = cx - cols + 2;
		if ((he < 0 || i < he) && addr_line(i) && draw_rcpts(i, x0, y, cols)) {
			/* drawn as recipients */
		} else if (he < 0 || i < he) {
			const char *colon = memchr(l->s, ':', l->n);
			if (colon && !hoff) {
				/* the value one column after "Label:", whatever its
				   direction */
				short lab = (short)(colon - l->s + 1), v = lab;
				text_at(x0, y, l->s, lab, cols, TX_BOLD);
				if (v < l->n && l->s[v] == ' ')
					v++;
				/* addresses read left to right; a subject may be Hebrew */
				text_at(x0 + (lab + 1) * cw, y, l->s + v, l->n - v, cols - lab - 1,
					strncasecmp(l->s, "Subject:", 8) ? TX_LTR : 0);
			} else {
				text_at(x0, y, l->s + hoff, l->n - hoff, cols, 0);
			}
		} else if (i == he) {
			hline(w->work.x, w->work.x + w->work.w - 1, y + ch / 2);
		} else {
			text_at(x0, y, l->s + hoff, l->n - hoff, cols,
				TX_RIGHT | ((l->n && l->s[0] == '>') ? TX_LIGHT : 0));
		}
		if (i == cy && w->h > 0 && win_is_top(w)) {
			short c = cursor_col(i, cx, cols, &hoff), pxy[4];
			if (c < 0)
				c = 0;
			pxy[0] = x0 + c * cw;
			pxy[1] = y;
			pxy[2] = pxy[0] + 1;
			pxy[3] = y + ch - 1;
			vswr_mode(vdi_h, 3);	/* XOR */
			vsf_interior(vdi_h, 1);
			vsf_color(vdi_h, 1);
			vr_recfl(vdi_h, pxy);
			vswr_mode(vdi_h, 1);
		}
	}
}

static void changed(long from, long n)
{
	if (!dirty) {
		dirty = 1;
		set_title();
	}
	w_editor.total = nl;
	win_sliders(&w_editor);
	if (n < 0)
		win_redraw_lines(&w_editor, from, win_rows(&w_editor) + 1);
	else
		win_redraw_lines(&w_editor, from, n);
}

static void move_to(long line, short col)
{
	long old = cy;
	if (line < 0)
		line = 0;
	if (line >= nl)
		line = nl - 1;
	if (col < 0)
		col = 0;
	if (col > ln[line].n)
		col = ln[line].n;
	cy = line;
	cx = col;
	win_redraw_lines(&w_editor, old, 1);
	win_ensure_visible(&w_editor, cy);
	win_redraw_lines(&w_editor, cy, 1);
}

/* wrap the current body line at opt.wrap, moving the cursor along */
static void wrap_current(void)
{
	ELINE *l = &ln[cy];
	short sp, keep;
	if (!in_body(cy) || l->n <= opt.wrap || (l->n && l->s[0] == '>'))
		return;
	for (sp = opt.wrap; sp > 0 && l->s[sp] != ' '; sp--)
		;
	if (sp <= 0)
		return;
	keep = sp;
	if (!ins_line(cy + 1, l->s + sp + 1, l->n - sp - 1))
		return;
	l = &ln[cy];
	l->n = keep;
	l->s[keep] = 0;
	if (cx > keep) {
		cx -= keep + 1;
		cy++;
	}
	changed(cy - 1, -1);
}

static void insert_char(char c)
{
	ELINE *l = &ln[cy];
	if (!grow_line(l, l->n + 1))
		return;
	memmove(l->s + cx + 1, l->s + cx, l->n - cx + 1);
	l->s[cx++] = c;
	l->n++;
	if (in_body(cy) && l->n > opt.wrap && c != ' ')
		wrap_current();
	changed(cy, 1);
	win_ensure_visible(&w_editor, cy);
}

static void split_line(void)
{
	ELINE *l = &ln[cy];
	if (!ins_line(cy + 1, l->s + cx, l->n - cx))
		return;
	l = &ln[cy];
	l->n = cx;
	l->s[cx] = 0;
	cy++;
	cx = 0;
	changed(cy - 1, -1);
	win_ensure_visible(&w_editor, cy);
}

/* where a header line's value starts: after "To: " */
static short value_at(long line)
{
	ELINE *l = &ln[line];
	short i;
	if (in_body(line))
		return 0;
	for (i = 0; i < l->n && l->s[i] != ':'; i++)
		;
	if (i == l->n)
		return 0;			/* no name: all of it */
	for (i++; i < l->n && l->s[i] == ' '; i++)
		;
	return i;
}

/* text at the cursor, lines ending '\n', all at once and drawn once (a
   header line takes the first line only); what doesn't fit is left out */
void editor_insert_text(const char *s)
{
	long from = cy;
	int body = in_body(cy);
	if (w_editor.h <= 0 || !nl)
		return;
	for (; *s; s++) {
		ELINE *l = &ln[cy];
		if (*s == '\n') {
			if (!body || !ins_line(cy + 1, ln[cy].s + cx, ln[cy].n - cx))
				break;
			l = &ln[cy];
			l->n = cx;
			l->s[cx] = 0;
			cy++;
			cx = 0;
			continue;
		}
		if ((unsigned char)*s < 32 && *s != '\t')
			continue;
		if (!grow_line(l, l->n + 1))
			break;
		memmove(l->s + cx + 1, l->s + cx, l->n - cx + 1);
		l->s[cx++] = *s == '\t' ? ' ' : *s;
		l->n++;
	}
	changed(from, -1);
	win_ensure_visible(&w_editor, cy);
}

void editor_insert_body(const char *s)
{
	long he = header_end();
	if (w_editor.h <= 0 || he < 0)
		return;
	if (he + 1 >= nl && !ins_line(nl, "", 0))
		return;
	cy = he + 1;
	cx = 0;
	editor_insert_text(s);
}

/* ^C ^X ^V: with nothing selected, the line the cursor is in (a header
   line's value, after "To: ") */
void editor_clip(int what)
{
	char buf[2048];
	ELINE *l;
	short v;
	if (w_editor.h <= 0 || !nl)
		return;
	if (what == CLIP_PASTE) {
		if (clip_get(buf, sizeof(buf)))
			editor_insert_text(in_body(cy) ? buf : clip_line(buf));
		return;
	}
	l = &ln[cy];
	v = value_at(cy);
	if (!clip_put(l->s + v, l->n - v) || what != CLIP_CUT)
		return;
	if (v || !in_body(cy) || nl - 1 <= header_end() + 1) {
		/* a header keeps its name; the only body line stays, empty */
		l->n = v;
		l->s[v] = 0;
		cx = v;
		changed(cy, 1);
	} else {
		del_line(cy);
		if (cy >= nl)
			cy = nl - 1;
		cx = 0;
		changed(cy, -1);
	}
}

static void join_next(long line)
{
	ELINE *a = &ln[line], *b;
	if (line + 1 >= nl)
		return;
	b = &ln[line + 1];
	if (!grow_line(a, a->n + b->n))
		return;
	memcpy(a->s + a->n, b->s, b->n + 1);
	a->n += b->n;
	del_line(line + 1);
	changed(line, -1);
}

static int key(WIN *w, short kstate, short k)
{
	short scan = KEY_SCAN(k);
	long rows = win_rows(w);
	short rtl = line_rtl(ln[cy].s, ln[cy].n);
	unsigned char c;

	switch (scan) {
	case 0x1c:	/* Return */
	case 0x72:
		split_line();
		return 1;
	case 0x0e:	/* Backspace */
		if (cx > 0) {
			ELINE *l = &ln[cy];
			memmove(l->s + cx - 1, l->s + cx, l->n - cx + 1);
			l->n--;
			cx--;
			changed(cy, 1);
		} else if (cy > 0) {
			short n = ln[cy - 1].n;
			cy--;
			join_next(cy);
			cx = n;
			win_ensure_visible(w, cy);
		}
		return 1;
	case 0x53:	/* Delete */
		if (cx < ln[cy].n) {
			ELINE *l = &ln[cy];
			memmove(l->s + cx, l->s + cx + 1, l->n - cx);
			l->n--;
			changed(cy, 1);
		} else {
			join_next(cy);
		}
		return 1;
	case 0x4b:	/* left: backwards, or forwards in a Hebrew line */
	case 0x4d: {
		short fwd = (scan == 0x4d) ^ rtl;
		if (kstate & 3) {
			move_to(cy, fwd ? ln[cy].n : 0);
		} else if (fwd) {
			if (cx < ln[cy].n)
				move_to(cy, cx + 1);
			else if (cy + 1 < nl)
				move_to(cy + 1, 0);
		} else {
			if (cx > 0)
				move_to(cy, cx - 1);
			else if (cy > 0)
				move_to(cy - 1, ln[cy - 1].n);
		}
		return 1;
	}
	case 0x48:
		move_to(cy - ((kstate & 3) ? rows : 1), cx);
		return 1;
	case 0x50:
		move_to(cy + ((kstate & 3) ? rows : 1), cx);
		return 1;
	case 0x47:	/* Clr/Home */
		if (kstate & 3)
			move_to(nl - 1, ln[nl - 1].n);
		else
			move_to(cy, 0);
		return 1;
	case 0x0f:	/* Tab: next header line, or spaces in the text */
		if (!in_body(cy)) {
			long he = header_end();
			if (cy + 1 < (he >= 0 ? he : nl))
				move_to(cy + 1, ln[cy + 1].n);
			else
				move_to(he >= 0 && he + 1 < nl ? he + 1 : cy, 0);
		} else {
			do
				insert_char(' ');
			while (cx % 4);
		}
		return 1;
	case 0x01:	/* Esc */
		w->closed(w);
		return 1;
	}
	if (key_ctrl(kstate, k))
		return 0;
	c = key_char(kstate, k);
	if (c >= 32 || c >= 0x80) {
		insert_char((char)c);
		return 1;
	}
	return 0;
}

static char *join_text(long *len)
{
	SBUF b;
	long i;
	sb_init(&b);
	for (i = 0; i < nl; i++) {
		sb_add(&b, ln[i].s, ln[i].n);
		if (i + 1 < nl)
			sb_addc(&b, '\n');
	}
	*len = b.len;
	return sb_steal(&b);
}

/* the To and Cc addresses into the address book, which is read once */
static void remember_rcpts(void)
{
	static char list[40][160];
	long i, he = header_end();
	int n = 0;
	for (i = 0; i < (he >= 0 ? he : nl) && n < 40; i++)
		if (!strncasecmp(ln[i].s, "To:", 3) || !strncasecmp(ln[i].s, "Cc:", 3))
			n += addr_list(ln[i].s + 3, list + n, 40 - n);
	if (n)
		abook_add_list(list, n);
}

void editor_send(int now)
{
	long len, rl;
	char *text, *raw, err[200];
	int sent = 0, r = 1;
	if (w_editor.h <= 0 || !e_acct)
		return;
	text = join_text(&len);
	raw = compose_build(e_acct, text, len, e_irt, e_refs, err, sizeof(err), &rl);
	free(text);
	if (!raw) {
		alert(1, "[1][%s][ OK ]", err);
		return;
	}
	now = now && !opt.offline;
	if (!now && !mail_queue(e_acct, raw, rl)) {
		alert(1, "[1][%s][ OK ]", mail_err);
		free(raw);
		return;
	}
	win_close(&w_editor);
	if (now) {
		/* straight from memory: the Outbox on the disk only when it
		   can't go now */
		ui_redraws();
		busy(1);
		mail_err[0] = 0;
		r = mail_send(e_acct, raw, rl, &sent);
		busy(0);
		if (r == -1) {
			str_copy(err, mail_err, sizeof(err));
			if (!mail_queue(e_acct, raw, rl)) {
				/* nowhere to keep it: the message comes back */
				alert(1, "[1][%s|%s||The message is back|in its window.][ OK ]", err, mail_err);
				free(raw);
				dirty = 1;
				win_open(&w_editor);
				eb_layout(&w_editor);
				win_sliders(&w_editor);
				win_redraw(&w_editor, 0);
				menu_update();
				return;
			}
			str_copy(mail_err, err, sizeof(mail_err));
		}
	}
	free(raw);
	remember_rcpts();
	/* mark the original answered */
	if (e_reply_uid && cur_folder && cur_finfo == e_reply_fi) {
		HDR *h = fold_get(cur_folder, e_reply_uid);
		if (h && !opt.offline)
			mail_flag(cur_folder, h, MF_ANSWERED, 1);
	}
	dirty = 0;
	free_all();
	if (r < 0)
		alert(1, "[1][%s||It waits in the Outbox.][ OK ]", mail_err);
	else if (!now)
		alert(1, "[1][The message is in the Outbox.|It goes out with the next|Check mail.][ OK ]");
	folders_build();
	if (cur_finfo && (cur_finfo->role == FR_OUTBOX || cur_finfo->role == FR_SENT)) {
		if (cur_folder)
			fold_close(cur_folder);
		cur_folder = fold_open(cur_acct, cur_finfo);
		list_refresh();
	}
	menu_update();
}

void editor_attach(void)
{
	static char path[160], name[16];
	short button;
	char full[200];
	if (w_editor.h <= 0)
		return;
	if (!path[0]) {
		str_copy(path, opt.workdir, sizeof(path) - 5);
		strcat(path, "\\*.*");
	}
	name[0] = 0;
	if (!fsel_exinput(path, name, &button, "Attach which file?") || !button || !name[0])
		return;
	str_copy(full, path, sizeof(full));
	{
		char *bs = strrchr(full, '\\');
		if (bs)
			bs[1] = 0;
	}
	strncat(full, name, sizeof(full) - strlen(full) - 1);
	editor_attach_file(full);
}

/* an Attach: line for the file, at the end of the header */
void editor_attach_file(const char *full)
{
	char line[220];
	long he;
	if (w_editor.h <= 0)
		return;
	snprintf(line, sizeof(line), "Attach: %s", full);
	he = header_end();
	if (he < 0)
		he = nl;
	ins_line(he, line, (short)strlen(line));
	if (cy >= he)
		cy++;
	changed(he, -1);
}

/* the address line ^B adds to: the one the cursor is on (To, Cc, Bcc),
   else To */
static long pick_line(void)
{
	long i, he = header_end();
	if (addr_line(cy))
		return cy;
	for (i = 0; i < (he >= 0 ? he : nl); i++)
		if (!strncasecmp(ln[i].s, "To:", 3))
			return i;
	return 0;
}

/* a picked person as the line has them: "Maya Carter <maya@x>", the name
   in quotes when it has a comma or the like in it */
static void person_text(SBUF *b, const PERSON *p)
{
	if (!p->name[0]) {
		sb_adds(b, p->addr);
		return;
	}
	if (strpbrk(p->name, ",;:\"<>@()[]")) {
		const char *c;
		sb_addc(b, '"');
		for (c = p->name; *c; c++) {
			if (*c == '"' || *c == '\\')
				sb_addc(b, '\\');
			sb_addc(b, *c);
		}
		sb_addc(b, '"');
	} else {
		sb_adds(b, p->name);
	}
	sb_printf(b, " <%s>", p->addr);
}

/* where a pick puts its people: the line, and the text typed there that
   asked for them (replaced by them), as it was when ^B was pressed */
static long pk_line = -1;
static short pk_from, pk_to;
static unsigned pk_ed;			/* which message (ed_count) */

/* add people to `line`, in place of [from, to), commas between, and the
   cursor after them: ready for the next */
static void add_people(long line, short from, short to, const char *text)
{
	ELINE *l = &ln[line];
	SBUF b;
	char prev, next;
	long old;
	while (from > 0 && l->s[from - 1] == ' ')
		from--;
	while (to < l->n && l->s[to] == ' ')
		to++;
	prev = from > 0 ? l->s[from - 1] : ':';
	next = to < l->n ? l->s[to] : 0;
	sb_init(&b);
	sb_adds(&b, prev == ':' || prev == ',' ? " " : ", ");
	sb_adds(&b, text);
	if (next != ',')
		sb_adds(&b, ", ");
	if (!grow_line(l, (short)(l->n - (to - from) + b.len))) {
		sb_free(&b);
		return;
	}
	l = &ln[line];
	memmove(l->s + from + b.len, l->s + to, l->n - to + 1);
	memcpy(l->s + from, b.s, b.len);
	l->n = (short)(l->n - (to - from) + b.len);
	old = cy;
	cy = line;
	cx = (short)(from + b.len);
	sb_free(&b);
	if (old != line)
		win_redraw_lines(&w_editor, old, 1);	/* the cursor left it */
	changed(line, 1);
	win_ensure_visible(&w_editor, cy);
}

/* ^B: people from EContact's window; without EContact, from the
   addresses EMail remembers (ADDRESS.TXT). What was typed of a name goes
   to EContact's search and is replaced by the people chosen. */
void editor_insert_address(void)
{
	char *addr, partial[100];
	long line;
	short from, to;
	ELINE *l;
	if (w_editor.h <= 0)
		return;
	line = pick_line();
	l = &ln[line];
	from = to = l->n;
	partial[0] = 0;
	if (line == cy) {
		RCPT r[MAXRCPT];
		int n = rcpt_layout(line, r, MAXRCPT, 0), k;
		for (k = 0; k < n; k++)
			if (cx > r[k].rs && cx <= r[k].re && !strchr(r[k].addr, '@')) {
				short m = r[k].re - r[k].rs < (short)sizeof(partial) ? r[k].re - r[k].rs
										 : (short)sizeof(partial) - 1;
				memcpy(partial, l->s + r[k].rs, m);
				partial[m] = 0;
				from = r[k].rs;
				to = r[k].re;
			}
	}
	if (people_picking()) {
		av_send("ECONTACT", "");	/* its window is open already: up with it */
		return;
	}
	if (people_pick(partial)) {
		pk_line = line;
		pk_from = from;
		pk_to = to;
		pk_ed = ed_count;
		status("Choose in EContact...");
		return;
	}
	addr = dlg_pick_address();
	if (!addr)
		return;
	if (w_editor.h > 0 && line < nl)
		add_people(line, from, to, addr);
	free(addr);
}

/* the people chosen in EContact (n = 0: none) */
void editor_picked(const PERSON *list, int n)
{
	SBUF b;
	int k;
	status("");
	if (w_editor.h <= 0 || pk_ed != ed_count || pk_line < 0 || pk_line >= nl || !addr_line(pk_line)) {
		pk_line = -1;
		return;
	}
	if (pk_to > ln[pk_line].n)
		pk_from = pk_to = ln[pk_line].n;
	sb_init(&b);
	for (k = 0; k < n; k++) {
		if (k)
			sb_adds(&b, ", ");
		person_text(&b, &list[k]);
	}
	sb_addc(&b, 0);
	if (n)
		add_people(pk_line, pk_from, pk_to, b.s);
	sb_free(&b);
	pk_line = -1;
	win_top(&w_editor);
}

static void closed(WIN *w)
{
	if (dirty) {
		short b = alert(1, "[2][Keep this message?][Outbox|Discard|Cancel]");
		if (b == 3)
			return;
		if (b == 1) {
			editor_send(0);
			return;
		}
	}
	dirty = 0;
	win_close(w);
	free_all();
}

static void resized(WIN *w)
{
	(void)w;
}

/* clicking puts the cursor there */
static void click(WIN *w, short mx, short my, short clicks, short kstate)
{
	long line;
	short col = (mx - w->work.x - cw / 2) / cw, cols = w->work.w / cw - 1;
	(void)clicks;
	(void)kstate;
	eb_layout(w);
	if (my < w->work.y + w->head_h) {
		bar_click(mx, my);
		return;
	}
	line = w->top + (my - w->work.y - w->head_h) / ch;
	if (line >= nl)
		line = nl - 1;
	if (line < 0)
		return;
	if (addr_line(line) && (line != cy || rcpt_cursor(line, cols) >= 0)) {
		/* drawn as recipients: a click on one is a click at its end,
		   which shows it as typed */
		RCPT r[MAXRCPT];
		const char *colon = memchr(ln[line].s, ':', ln[line].n);
		short lab = colon ? (short)(colon - ln[line].s + 1) : 0, v = value_at(line);
		int n = rcpt_layout(line, r, MAXRCPT, 0);
		move_to(line, col <= lab ? v : rcpt_raw(r, n, v, col - lab - 1, ln[line].n));
		return;
	}
	if (line_rtl(ln[line].s, ln[line].n)) {
		/* map the visual column back to a logical one */
		char vis[BIDI_MAX];
		short pos[BIDI_MAX], m = ln[line].n > cols ? cols : ln[line].n, i, best = m;
		unsigned char odd[BIDI_MAX];
		short xoff = cols - m;
		bidi_visual_map(ln[line].s, m, 1, vis, pos, odd);
		for (i = 0; i < m; i++)
			if (pos[i] + xoff == col)
				best = i;
		col = best;
	}
	move_to(line, col);
}

static void rclick(WIN *w, short mx, short my)
{
	const char *lab[11];
	short r;
	(void)w;
	lab[0] = "Send now         \x01^D";
	lab[1] = "Put in Outbox     ^S";
	lab[2] = "-";
	lab[3] = "Attach file...    ^T";
	lab[4] = "Address book...   ^B";
	lab[5] = "-";
	lab[6] = hebrew_kbd ? "\x08Hebrew keyboard   F10" : "Hebrew keyboard   F10";
	lab[7] = "-";
	lab[8] = "Cut line          ^X";
	lab[9] = "Copy line         ^C";
	lab[10] = "Paste             ^V";
	r = popup(mx, my, lab, 11);
	switch (r) {
	case 0: editor_send(1); break;
	case 1: editor_send(0); break;
	case 3: editor_attach(); break;
	case 4: editor_insert_address(); break;
	case 6: set_hebrew_kbd(!hebrew_kbd); break;
	case 8: editor_clip(CLIP_CUT); break;
	case 9: editor_clip(CLIP_COPY); break;
	case 10: editor_clip(CLIP_PASTE); break;
	}
}

void editor_init(void);
void editor_init(void)
{
	w_editor.h = -1;
	w_editor.draw = draw;
	w_editor.key = key;
	w_editor.click = click;
	w_editor.rclick = rclick;
	w_editor.closed = closed;
	w_editor.resized = resized;
	strcpy(w_editor.title, " New message ");
}

