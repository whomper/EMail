/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * list.c - the message list window (the GFA Troll's "headers" window):
 * newest first, unread in bold, flag/attachment marks, and the From (or
 * To, in Sent and Outbox) and Subject columns laid out right to left
 * when they are Hebrew.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui.h"
#include "theme.h"
#include "../src/mail.h"
#include "util.h"

static long *view;		/* indexes into cur_folder->h, newest first */
static long nview, sel = -1;
static long shown;		/* local folders: how many of the newest to list */
static long more;		/* older messages not listed yet (0: no "load more" row) */
static unsigned long sel_uid;

/* more than one message selected (Shift+click, Control+click, ^A): their
   UIDs. Empty: the one current message (sel) is the selection. */
static unsigned long *marks;
static long nmarks, capmarks;
static long anchor = -1;	/* where a Control+click range starts */

static long mark_find(unsigned long uid)
{
	long i;
	for (i = 0; i < nmarks; i++)
		if (marks[i] == uid)
			return i;
	return -1;
}

static int marked(long row)
{
	return nmarks && row >= 0 && row < nview && mark_find(cur_folder->h[view[row]].uid) >= 0;
}

static void mark_set(long row, int on)
{
	unsigned long uid;
	long k;
	if (row < 0 || row >= nview)
		return;
	uid = cur_folder->h[view[row]].uid;
	k = mark_find(uid);
	if (on && k < 0) {
		if (nmarks == capmarks) {
			long cap = capmarks ? capmarks * 2 : 64;
			unsigned long *n = realloc(marks, cap * sizeof(*marks));
			if (!n)
				return;
			marks = n;
			capmarks = cap;
		}
		marks[nmarks++] = uid;
	} else if (!on && k >= 0) {
		marks[k] = marks[--nmarks];
	}
}

void list_unmark(void)
{
	nmarks = 0;
}

long list_marked(void)
{
	return nmarks;
}

/* the messages a command works on: the selected ones, or the current
   one. A copy of their UIDs (the list changes as they move), or NULL */
unsigned long *list_targets(long *n)
{
	unsigned long *u;
	HDR *h = list_current();
	*n = nmarks ? nmarks : h ? 1 : 0;
	if (!*n)
		return 0;
	u = malloc(*n * sizeof(*u));
	if (!u) {
		*n = 0;
		return 0;
	}
	if (nmarks)
		memcpy(u, marks, nmarks * sizeof(*u));
	else
		u[0] = h->uid;
	return u;
}

static int cmp_date(const void *x, const void *y)
{
	HDR *a = &cur_folder->h[*(const long *)x], *b = &cur_folder->h[*(const long *)y];
	if (a->date != b->date)
		return a->date < b->date ? 1 : -1;
	return a->uid < b->uid ? 1 : a->uid > b->uid ? -1 : 0;
}

static int show_to(void)
{
	return cur_finfo && (cur_finfo->role == FR_SENT || cur_finfo->role == FR_OUTBOX ||
			     cur_finfo->role == FR_DRAFTS);
}

void list_titles(void)
{
	char t[100], i[120];
	if (!cur_folder) {
		win_title(&w_list, "Messages");
		win_info(&w_list, "");
		return;
	}
	snprintf(t, sizeof(t), "%s - %s", cur_finfo->disp, cur_acct->name);
	win_title(&w_list, t);
	if (nmarks > 1)
		snprintf(i, sizeof(i), "%s selected", num(nmarks));
	else if (opt.offline)
		snprintf(i, sizeof(i), "%s unread  (offline)", num(cur_finfo->unread));
	else if (cur_acct->cut && !cur_finfo->local)
		snprintf(i, sizeof(i), "%s unread  (not connected: Check mail tries again)", num(cur_finfo->unread));
	else
		snprintf(i, sizeof(i), "%s of %s, %s unread", num(cur_folder->n),
			 num(cur_finfo->total > cur_folder->n ? cur_finfo->total : cur_folder->n),
			 num(cur_finfo->unread));
	win_info(&w_list, i);
}

/* the view is of one state of the folder: a job working on it (a check,
   Download all) changes cur_folder->h, and the view is made again
   before it is used */
static FOLDER *view_of;
static unsigned view_gen;

static void build_view(void);

/* the messages drawn while EContact hadn't said their names yet: when
   its answers come, only those rows are drawn again, and only if a name
   came (it answers a moment after a scroll brings new people in) */
#define NPEND 64
static unsigned long pend[NPEND];
static int npend;

static void pend_add(unsigned long uid)
{
	int k;
	for (k = 0; k < npend; k++)
		if (pend[k] == uid)
			return;
	if (npend < NPEND)
		pend[npend++] = uid;
}

static void fresh(void)
{
	if (cur_folder != view_of || (cur_folder && cur_folder->gen != view_gen)) {
		build_view();
		w_list.total = nview + (more ? 1 : 0);
		if (w_list.top > nview)
			w_list.top = 0;
	}
}

/* what the list shows, in a number: a check that found nothing new
   leaves it as it was, and the list isn't drawn again (a GDOS font's
   rows are drawn a letter at a time, and the pane is seen filling in) */
static unsigned long shows(void)
{
	unsigned long s = (unsigned long)cur_folder ^ ((unsigned long)cur_finfo << 1);
	long i;
	s = s * 31 + (unsigned long)nview;
	s = s * 31 + (unsigned long)more;
	s = s * 31 + (unsigned long)sel;
	s = s * 31 + (unsigned long)nmarks;
	s = s * 31 + (unsigned long)w_list.top;
	for (i = 0; i < nview; i++) {
		HDR *h = &cur_folder->h[view[i]];
		s = s * 31 + h->uid;
		s = s * 31 + h->flags;
	}
	return s;
}

static unsigned long shown_sig;
static int shown_valid;

void list_refresh(void)
{
	unsigned long s;
	build_view();
	w_list.total = nview + (more ? 1 : 0);
	if (w_list.top > nview)
		w_list.top = 0;
	list_titles();
	win_sliders(&w_list);
	s = shows();
	if (shown_valid && s == shown_sig)
		return;
	shown_sig = s;
	shown_valid = 1;
	win_redraw(&w_list, 0);
}

static void build_view(void)
{
	long i;
	view_of = cur_folder;
	view_gen = cur_folder ? cur_folder->gen : 0;
	free(view);
	view = 0;
	nview = 0;
	sel = -1;
	if (cur_folder && cur_folder->n) {
		view = malloc(cur_folder->n * sizeof(long));
		if (view) {
			for (i = 0; i < cur_folder->n; i++)
				view[i] = i;
			nview = cur_folder->n;
			qsort(view, nview, sizeof(long), cmp_date);
			/* a big local folder lists a page at a time too */
			if (cur_finfo->local && nview > shown)
				nview = shown;
			for (i = 0; i < nview; i++)
				if (cur_folder->h[view[i]].uid == sel_uid)
					sel = i;
		}
	}
	if (cur_folder) {
		long k;
		fold_count(cur_folder);
		for (k = nmarks - 1; k >= 0; k--)
			if (!fold_get(cur_folder, marks[k]))
				marks[k] = marks[--nmarks];
	} else {
		nmarks = 0;
	}
	more = 0;
	if (cur_folder && cur_finfo->local)
		more = cur_folder->n - nview;
	else if (cur_folder) {
		long server = cur_folder->exists ? cur_folder->exists : cur_finfo->total;
		more = server - cur_folder->n;
	}
	if (more < 0)
		more = 0;
}

void list_load(void)
{
	sel_uid = 0;
	nmarks = 0;
	anchor = -1;
	shown = opt.page;
	w_list.top = 0;
	list_refresh();
}

HDR *list_current(void)
{
	fresh();
	if (!cur_folder || sel < 0 || sel >= nview)
		return 0;
	return &cur_folder->h[view[sel]];
}

void list_select_uid(unsigned long uid)
{
	long i;
	fresh();
	sel_uid = uid;
	for (i = 0; i < nview; i++)
		if (cur_folder->h[view[i]].uid == uid) {
			sel = i;
			win_ensure_visible(&w_list, i);
		}
	win_redraw(&w_list, 0);
}

/* after the current message was deleted or moved: nothing chosen, so
   no message is opened (and perhaps downloaded) unasked */
/* after messages went (Delete, Move): the list drawn once; the reader
   is its callers' to clear, before (Erez's video, 26 Oct 2026: the list
   and the reader each drawn two and three times for one Delete) */
void list_after_remove(void)
{
	sel_uid = 0;
	shown_valid = 0;
	list_refresh();
	folders_build();
}

static short wide;	/* room for the size column */

/* the marks column: ! (flagged) and @ (attachments), then a space.
   Unread messages are bold. */
#define MK 3

static void columns(short cols, short *c_from, short *c_subj, short *c_date)
{
	short rest;
	wide = cols >= 76;	/* the size column */
	*c_date = 15;		/* "11.10.26 14:05": always with the year */
	rest = cols - MK - *c_date - (wide ? 6 : 0);
	*c_from = rest / 3;
	if (*c_from > 24)
		*c_from = 24;
	if (*c_from < 8)
		*c_from = 8;
	*c_subj = rest - *c_from - 1;
	if (*c_subj < 4)
		*c_subj = 4;
}

static void size_str(long n, char *out)
{
	if (n < 1024)
		snprintf(out, 8, "%ldb", n);
	else if (n < 1024L * 1000)
		snprintf(out, 8, "%ldK", (n + 512) / 1024);
	else
		snprintf(out, 8, "%ldM", (n + 512L * 1024) / (1024L * 1024));
}

static void draw(WIN *w, GRECT *clip)
{
	short cols = w->work.w / cw, c_from, c_subj, c_date;
	long i, rows = win_rows(w) + 1, first, last;
	short x0 = w->work.x + 2, y0 = w->work.y + w->head_h;
	GRECT r;
	fresh();
	/* only the rows the area meets: a row drawn again (a click, a mark)
	   draws one row, not the list (a Falcon at 16 MHz: 90 ms a time) */
	first = clip->y > y0 ? w->top + (clip->y - y0) / ch : w->top;
	last = clip->y + clip->h > y0 ? w->top + (clip->y + clip->h - 1 - y0) / ch : w->top - 1;
	columns(cols, &c_from, &c_subj, &c_date);
	/* each row fills its own background as it is drawn: a list drawn
	   again changes row by row, and is never white first */
	if (!cur_folder || !nview) {
		fill(clip, 0);
	} else {
		long end = nview + (more ? 1 : 0);
		if (end > w->top + rows)
			end = w->top + rows;
		if (end < w->top)
			end = w->top;
		r.x = w->work.x;
		r.y = w->work.y;
		r.w = w->work.w;
		r.h = w->head_h;
		fill(&r, 0);
		r.y = w->work.y + w->head_h + (short)((end - w->top) * ch);
		r.h = w->work.y + w->work.h - r.y;
		if (r.h > 0)
			fill(&r, 0);
	}

	/* column titles */
	if (clip->y < y0) {
		const char *who = show_to() ? "To" : "From";
		text_pen = pen[P_DIM];
		text_at(x0 + MK * cw, w->work.y + 1, who, strlen(who), c_from, TX_BOLD);
		text_at(x0 + (MK + 1 + c_from) * cw, w->work.y + 1, "Subject", 7, c_subj, TX_BOLD);
		text_at(x0 + (MK + 1 + c_from + c_subj) * cw, w->work.y + 1, "Date", 4, c_date, TX_BOLD);
		text_pen = 1;
		hline(w->work.x, w->work.x + w->work.w - 1, w->work.y + w->head_h - 2);
	}
	if (!cur_folder) {
		text_at(x0, w->work.y + w->head_h + ch, "Choose a folder on the left.", 28, cols, TX_LIGHT);
		return;
	}
	if (!nview) {
		const char *m = cur_finfo->role == FR_OUTBOX ? "Nothing waiting to be sent." : "No messages.";
		text_at(x0, w->work.y + w->head_h + ch, m, strlen(m), cols, TX_LIGHT);
		return;
	}
	if (more && nview >= w->top && nview < w->top + rows) {
		/* the last row offers the next page */
		char t[80];
		short y = w->work.y + w->head_h + (short)((nview - w->top) * ch);
		long next = more < opt.page ? more : opt.page;
		snprintf(t, sizeof(t), "\x02 Load %s more  (%s older on %s)", num(next), num(more),
			 cur_finfo->local ? "disk" : "the server");
		r.x = w->work.x;
		r.y = y;
		r.w = w->work.w;
		r.h = ch;
		fill(&r, sel == nview ? pen[P_SEL] : 0);
		text_at(x0 + MK * cw, y, t, strlen(t), cols - MK, TX_BOLD | (sel == nview ? TX_INVERSE : 0));
	}
	for (i = first; i < nview && i < w->top + rows && i <= last; i++) {
		HDR *h = &cur_folder->h[view[i]];
		short y = w->work.y + w->head_h + (short)((i - w->top) * ch);
		int lit = nmarks ? marked(i) : i == sel;
		short fl = lit ? TX_INVERSE : 0;
		char marks[4], date[24], size[8];
		const char *who = show_to() ? h->to : h->from;
		char name[64];
		r.x = w->work.x;
		r.y = y;
		r.w = w->work.w;
		r.h = ch;
		fill(&r, lit ? pen[P_SEL] : 0);
		if (!(h->flags & MF_SEEN))
			fl |= TX_BOLD;
		/* fixed places: ! flagged, @ attachments */
		marks[0] = (h->flags & MF_FLAGGED) ? '!' : ' ';
		marks[1] = (h->flags & MF_ATTACH) ? '@' : ' ';
		marks[2] = 0;
		if (colour && !lit) {
			text_pen_at(pen[P_FLAG], x0, y, marks, 1, 1, TX_BOLD);
			text_pen_at(pen[P_DIM], x0 + cw, y, marks + 1, 1, 1, 0);
		} else {
			text_at(x0, y, marks, 2, 2, fl & ~TX_BOLD);
		}
		/* "Name <a@b>" shows as the name EContact has for a@b, else as
		   Name, else as a@b */
		{
			char mail[PEOPLE_ADDR];
			const char *known;
			addr_split(who ? who : "", name, sizeof(name), mail, sizeof(mail));
			known = mail[0] ? people_name(mail) : 0;
			if (mail[0] && !known)
				pend_add(h->uid);	/* asked: it may come */
			if (known && *known)
				str_copy(name, known, sizeof(name));
			else if (!name[0])
				str_copy(name, mail, sizeof(name));
		}
		text_at(x0 + MK * cw, y, name, strlen(name), c_from, fl | TX_RIGHT);
		text_at(x0 + (MK + 1 + c_from) * cw, y, h->subject, strlen(h->subject), c_subj - 1, fl | TX_RIGHT);
		date_str(h->date, date, sizeof(date), 1);
		/* dates and sizes in grey, so who and what stand out */
		text_pen = lit ? 1 : pen[P_DIM];
		text_at(x0 + (MK + 1 + c_from + c_subj) * cw, y, date, strlen(date), c_date, fl & ~TX_BOLD);
		if (wide) {
			size_str(h->size, size);
			text_at(x0 + (cols - 6) * cw, y, size, strlen(size), 5, fl & ~TX_BOLD);
		}
		text_pen = 1;
	}
}

/* the "load more" row: the next page of older messages */
static void load_more(void)
{
	long added = 0, keep_top = w_list.top;
	if (!more)
		return;
	if (cur_finfo->local) {
		shown += opt.page;
	} else if (opt.offline || cur_acct->cut) {
		alert(1, "[1][EMail is not connected.|Older messages are on the server:|Check mail to connect again.][ OK ]");
		return;
	} else {
		busy(1);
		mail_err[0] = 0;
		if (!mail_load_more(cur_acct, cur_finfo, &added) && mail_err[0])
			alert(1, "[1][%s][ OK ]", mail_err);
		busy(0);
		fold_done(cur_folder);	/* it worked on cur_folder itself: saved once */
	}
	list_refresh();
	win_scroll_to(&w_list, keep_top);
	folders_build();
}

static void select_row(long i, int open)
{
	long old = sel;
	if (i >= nview && more) {
		/* onto the "load more" row; Return (or a click) loads */
		sel = nview;
		win_redraw_lines(&w_list, old, 1);
		win_redraw_lines(&w_list, sel, 1);
		win_ensure_visible(&w_list, nview);
		if (open)
			load_more();
		return;
	}
	if (i < 0 || i >= nview)
		return;
	if (nmarks) {
		nmarks = 0;
		win_redraw(&w_list, 0);
	}
	anchor = i;
	if (i == old && cur_msg && !reader_missing && cur_uid == cur_folder->h[view[i]].uid) {
		/* the message shown already: nothing is drawn again (Erez's
		   video, 20 Oct: a click on it redrew the row twice, the
		   reader and the folders) */
		win_ensure_visible(&w_list, sel);
		if (open)
			win_focus(&w_reader);
		return;
	}
	sel = i;
	sel_uid = cur_folder->h[view[i]].uid;
	if (old != sel)
		win_redraw_lines(&w_list, old, 1);
	win_redraw_lines(&w_list, sel, 1);
	win_ensure_visible(&w_list, sel);
	{
		unsigned short was = cur_folder->h[view[i]].flags;
		reader_show(&cur_folder->h[view[i]]);
		/* reading may have marked it read: then the row, the counts */
		if (cur_folder->h[view[i]].flags != was)
			win_redraw_lines(&w_list, sel, 1);
	}
	list_titles();
	folders_build();	/* draws only the rows that changed */
	if (open)
		win_focus(&w_reader);
}

/* Shift+click: this one in or out of the selection; Control+click: all
   from the last clicked one to this one */
static void mark_click(long i, short kstate)
{
	long a, b, k;
	if (i < 0 || i >= nview)
		return;
	if (!nmarks && sel >= 0 && sel < nview)
		mark_set(sel, 1);		/* the message shown so far comes along */
	if ((kstate & K_CTRL) && anchor >= 0 && anchor < nview) {
		a = anchor < i ? anchor : i;
		b = anchor < i ? i : anchor;
		for (k = a; k <= b; k++)
			mark_set(k, 1);
	} else {
		mark_set(i, !marked(i));
		anchor = i;
	}
	sel = i;
	sel_uid = cur_folder->h[view[i]].uid;
	if (nmarks == 1) {
		/* back to one: show it */
		long only = 0;
		for (k = 0; k < nview; k++)
			if (marked(k))
				only = k;
		nmarks = 0;
		select_row(only, 0);
		return;
	}
	reader_selection(nmarks);
	win_redraw(&w_list, 0);
	list_titles();
	menu_update();
}

void list_select_all(void)
{
	long k;
	fresh();
	if (!cur_folder || nview < 2)
		return;
	for (k = 0; k < nview; k++)
		mark_set(k, 1);
	reader_selection(nmarks);
	win_redraw(&w_list, 0);
	list_titles();
	menu_update();
}

/* ---------------- dragging messages onto a folder ----------------
 * A press on a message that moves before the button comes up drags it,
 * or the whole selection when it is part of one, to a folder of the
 * folder pane: an outline follows the mouse, the folder under it that can
 * take them is lit, and letting go there moves them as Move to does. Esc,
 * Undo or the right button stops the drag; let go anywhere else and
 * nothing happens. */
static void xor_rect(GRECT *r, int filled)
{
	short p[10];
	p[0] = p[6] = p[8] = r->x;
	p[1] = p[3] = p[9] = r->y;
	p[2] = p[4] = r->x + r->w - 1;
	p[5] = p[7] = r->y + r->h - 1;
	graf_mouse(M_OFF, 0);
	if (filled) {
		short q[4];
		q[0] = r->x;
		q[1] = r->y;
		q[2] = r->x + r->w - 1;
		q[3] = r->y + r->h - 1;
		vr_recfl(vdi_h, q);
	} else {
		v_pline(vdi_h, 5, p);
	}
	graf_mouse(M_ON, 0);
}

/* the outline: one row's, two stacked for several messages */
static void xor_drag(GRECT *b, int several)
{
	xor_rect(b, 0);
	if (several) {
		GRECT c = *b;
		c.x += 3;
		c.y += 3;
		xor_rect(&c, 0);
	}
}

/* 1 when the press on row i became a drag (done, or stopped) */
static int drag_messages(long i, short mx, short my)
{
	short x = mx, y = my, b, k, held, stop = 0, offx, offy;
	int several = nmarks > 1 && marked(i);
	GRECT box, scr, lit;
	FINFO *target = 0;
	unsigned long *u;
	long n;
	/* a click, unless the mouse moves with the button down */
	for (;;) {
		graf_mkstate(&x, &y, &b, &k);
		if (!(b & 1))
			return 0;
		if (x - mx > 3 || mx - x > 3 || y - my > 3 || my - y > 3)
			break;
		evnt_timer_(10);
	}
	/* the outline rides below and right of the pointer, clear of the
	   folder it points at */
	box.w = 16 * cw;
	box.h = ch;
	offx = -12;
	offy = -(ch * 3 / 4);
	scr.x = scr.y = 0;
	scr.w = scr_w;
	scr.h = scr_h;
	wind_update(BEG_UPDATE);
	wind_update(3);			/* the mouse is ours */
	graf_mouse(FLAT_HAND, 0);
	clip_on(&scr);
	vswr_mode(vdi_h, 3);		/* XOR */
	vsl_color(vdi_h, 1);
	vsf_interior(vdi_h, 1);
	vsf_color(vdi_h, 1);
	box.x = x - offx;
	box.y = y - offy;
	xor_drag(&box, several);
	do {
		AEVENT ev;
		short msg[8];
		GRECT row;
		FINFO *t;
		if (evnt_multi_(MU_KEYBD | MU_TIMER, 0, 0, 0, 10, msg, &ev) & MU_KEYBD) {
			short sc = KEY_SCAN(ev.kreturn);
			if (sc == 0x01 || sc == 0x61)
				stop = 1;	/* Esc, Undo */
		}
		graf_mkstate(&x, &y, &b, &k);
		held = b & 1;
		if (b & 2)
			stop = 1;		/* the right button */
		t = stop ? 0 : folders_drop_target(x, y, &row);
		if (x - offx != box.x || y - offy != box.y || t != target) {
			xor_drag(&box, several);
			if (target)
				xor_rect(&lit, 1);
			box.x = x - offx;
			box.y = y - offy;
			target = t;
			lit = row;
			if (target)
				xor_rect(&lit, 1);
			if (!stop)
				xor_drag(&box, several);
		}
	} while (held && !stop);
	if (!stop)
		xor_drag(&box, several);
	if (target)
		xor_rect(&lit, 1);
	vswr_mode(vdi_h, 1);
	attr_forget();
	graf_mouse(ARROW, 0);
	wind_update(2);
	wind_update(END_UPDATE);
	if (stop || !target)
		return 1;
	if (several) {
		u = list_targets(&n);
	} else {
		u = malloc(sizeof(*u));
		n = 1;
		if (u)
			u[0] = cur_folder->h[view[i]].uid;
	}
	if (u)
		move_messages(u, n, target);
	return 1;
}

static void click(WIN *w, short mx, short my, short clicks, short kstate)
{
	long i;
	fresh();
	if (my < w->work.y + w->head_h)
		return;
	i = w->top + (my - w->work.y - w->head_h) / ch;
	if (i == nview && more) {
		load_more();
		return;
	}
	if (kstate & (K_LSHIFT | K_RSHIFT | K_CTRL)) {
		mark_click(i, kstate);
		return;
	}
	if (clicks < 2 && i >= 0 && i < nview && drag_messages(i, mx, my))
		return;
	select_row(i, clicks > 1);
}

/* the message menu, shared with the reader */
void message_menu(short mx, short my, int with_open)
{
	HDR *h = list_current();
	int imap = cur_finfo && !cur_finfo->local, shown;
	const char *lab[12];
	short cmd[12], n = 0, r;
	fresh();
	if (nmarks > 1) {
		/* for all the selected messages */
		static const char *blab[] = { "Mark as read", "Mark as unread   ^U", "Flag / unflag    ^G",
					      "-", "Move to...       ^M", "Download again", "-", "Delete          Del" };
		static const short bcmd[] = { C_MARKREAD, C_UNREAD, C_FLAG, 0, C_MOVE, C_REFETCH, 0, C_DELETE };
		const char *l[8];
		memcpy(l, blab, sizeof(l));
		if (!imap)
			l[5] = "~Download again";
		r = popup(mx, my, l, 8);
		if (r >= 0 && bcmd[r]) {
			win_redraw_area(&popup_area);	/* not the menu's ghost while it works */
			ui_command(bcmd[r]);
		}
		return;
	}
	/* a message that didn't come (or came wrong) still has the menu, to
	   download it again */
	if (!h && cur_folder)
		h = fold_get(cur_folder, cur_uid);
	if (!h)
		return;
	shown = cur_msg && cur_uid == h->uid;
	if (with_open) {
		lab[n] = "Open";
		cmd[n++] = -1;
	}
	lab[n] = shown ? "Reply            ^R" : "~Reply            ^R";
	cmd[n++] = C_REPLY;
	lab[n] = shown ? "Reply to all     ^E" : "~Reply to all     ^E";
	cmd[n++] = C_REPLYALL;
	lab[n] = shown ? "Forward         \x01^F" : "~Forward         \x01^F";
	cmd[n++] = C_FORWARD;
	lab[n] = "-";
	cmd[n++] = 0;
	lab[n] = (h->flags & MF_SEEN) ? "Mark as unread   ^U" : "Mark as read";
	cmd[n++] = (h->flags & MF_SEEN) ? C_UNREAD : C_MARKREAD;
	lab[n] = (h->flags & MF_FLAGGED) ? "Remove the flag  ^G" : "Flag             ^G";
	cmd[n++] = C_FLAG;
	lab[n] = "Move to...       ^M";
	cmd[n++] = C_MOVE;
	lab[n] = imap ? "Download again" : "~Download again";
	cmd[n++] = C_REFETCH;
	lab[n] = "-";
	cmd[n++] = 0;
	lab[n] = "Delete          Del";
	cmd[n++] = C_DELETE;
	r = popup(mx, my, lab, n);
	if (r < 0)
		return;
	if (cmd[r] == -1) {
		if (shown)
			win_focus(&w_reader);
		else
			reader_show(h);
	} else if (cmd[r]) {
		win_redraw_area(&popup_area);
		ui_command(cmd[r]);
	}
}

static void rclick(WIN *w, short mx, short my)
{
	long i;
	fresh();
	if (my < w->work.y + w->head_h)
		return;
	i = w->top + (my - w->work.y - w->head_h) / ch;
	if (i < 0 || i >= nview)
		return;
	if (nmarks && marked(i)) {
		message_menu(mx, my, 0);	/* for the whole selection */
		return;
	}
	if (cur_folder->h[view[i]].uid != cur_uid || !cur_msg)
		select_row(i, 0);
	message_menu(mx, my, 1);
}

static int key(WIN *w, short kstate, short k)
{
	short scan = KEY_SCAN(k);
	long rows = win_rows(w);
	fresh();
	switch (scan) {
	case 0x48:	/* up (shift: page) */
		select_row(sel < 0 ? 0 : sel - ((kstate & 3) ? rows : 1), 0);
		return 1;
	case 0x50:	/* down */
		select_row(sel < 0 ? 0 : sel + ((kstate & 3) ? rows : 1), 0);
		return 1;
	case 0x47:	/* Clr/Home: first, shift: last */
		select_row((kstate & 3) ? nview - 1 : 0, 0);
		return 1;
	case 0x1c:	/* Return: read it (or load more, on that row) */
	case 0x72:
		select_row(sel < 0 ? 0 : sel, 1);
		return 1;
	}
	return 0;
}

void list_init(void);
void list_init(void)
{
	w_list.h = -1;
	w_list.pane = 1;
	w_list.draw = draw;
	w_list.click = click;
	w_list.rclick = rclick;
	w_list.key = key;
	strcpy(w_list.title, "Messages");
}

/* EContact's names came, or changed */
void list_names_came(void)
{
	long i, rows = win_rows(&w_list) + 1;
	unsigned long was[NPEND];
	int n = npend, k;
	if (!cur_folder || !n)
		return;
	memcpy(was, pend, n * sizeof(*was));
	npend = 0;		/* drawing again asks again for what is still out */
	fresh();
	for (i = w_list.top; i < nview && i < w_list.top + rows; i++) {
		HDR *h = &cur_folder->h[view[i]];
		const char *who = show_to() ? h->to : h->from, *known;
		char name[64], mail[PEOPLE_ADDR];
		for (k = 0; k < n && was[k] != h->uid; k++)
			;
		if (k == n)
			continue;
		addr_split(who ? who : "", name, sizeof(name), mail, sizeof(mail));
		known = mail[0] ? people_name(mail) : 0;
		if (!known)
			pend_add(h->uid);	/* not yet */
		else if (*known)
			win_redraw_lines(&w_list, i, 1);
	}
}
