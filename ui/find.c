/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * find.c - Find in the open folder (Erez, 31 Oct 2026).
 *
 * Ctrl+F (Message > Find) puts a field across the top of the message
 * list. What is typed there, after a short pause, leaves in the list
 * only the folder's messages on disk whose From (To in Sent, Outbox and
 * Drafts) or Subject has it, case aside. Search server (or Shift+Return)
 * asks an IMAP server for the folder's messages with it anywhere, the
 * body too: the newest 200 of them are listed, their headers mirrored.
 * Esc empties the field, and the whole folder is back (guidelines 3.5);
 * Undo, Ctrl+F again, the close box or another folder leaves Find.
 * Offline the button is grey; a POP3 or local folder has none.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ui.h"
#include "theme.h"
#include "field.h"
#include "clip.h"
#include "util.h"
#include "../src/mail.h"
#include "../src/plat.h"

#define FIND_CAP 64		/* the text it holds (5.4: more than it shows) */
#define SERVER_MAX 200		/* the newest results Search server lists */
#define PAUSE 300		/* ms after the last key before the list follows */

static int on;			/* Find is open */
static char text[FIND_CAP + 1];	/* the field */
static char found[FIND_CAP + 1];	/* what the list shows the messages with */
static LFIELD fl;
static unsigned long due;	/* pf_ms() when the typed text is looked for; 0: done */
static int server;		/* the list shows Search server's results */
static unsigned long *suid;	/* their UIDs, in order */
static long nsuid;
static unsigned char needle[FIND_CAP + 1];	/* found, folded */
static unsigned char first2;	/* the needle's first letter as a capital too */
static short nlen;
static unsigned char fold[256];	/* a letter as its small one */
static GRECT row, field, button, closer;	/* where they are drawn */

#ifdef DRAW_TRACE
static void trace_found(long n)
{
	char t[100];
	snprintf(t, sizeof(t), "FIND %s \"%s\" %ld", server ? "server" : "disk", found, n);
	pf_debug(t);
}
static void trace_on(int on)
{
	pf_debug(on ? "FIND on" : "FIND off");
}
#else
#define trace_found(n) ((void)0)
#define trace_on(on) ((void)0)
#endif

/* ---------------- matching ---------------- */

static void fold_init(void)
{
	/* the Atari's capitals with their small letters (Ç Ü É Ä Å Æ Ö Ñ) */
	static const unsigned char big[] = { 0x80, 0x9A, 0x90, 0x8E, 0x8F, 0x92, 0x99, 0xA5 };
	static const unsigned char small[] = { 0x87, 0x81, 0x82, 0x84, 0x86, 0x91, 0x94, 0xA4 };
	short i;
	for (i = 0; i < 256; i++)
		fold[i] = (unsigned char)(i >= 'A' && i <= 'Z' ? i + 32 : i);
	for (i = 0; i < (short)sizeof(big); i++)
		fold[big[i]] = small[i];
}

/* the needle in s, case aside: its first letter looked for, then the
   rest (500 messages' From and Subject on an 8 MHz ST: well inside a
   turn) */
static int has(const char *s)
{
	const unsigned char *p = (const unsigned char *)s;
	unsigned char c0 = needle[0], c1 = first2;
	if (!p)
		return 0;
	for (;;) {
		unsigned char c = *p++;
		short k;
		/* the first letter, small or capital, compared as it is: no
		   table looked up for each character (95 ms an ST for 500
		   messages' From and Subject, with it) */
		if (c != c0 && c != c1) {
			if (!c)
				return 0;
			continue;
		}
		for (k = 1; k < nlen && fold[p[k - 1]] == needle[k]; k++)
			;
		if (k == nlen)
			return 1;
	}
}

/* uid among Search server's (in order) */
static int found_uid(unsigned long uid)
{
	long lo = 0, hi = nsuid - 1;
	while (lo <= hi) {
		long m = (lo + hi) / 2;
		if (suid[m] == uid)
			return 1;
		if (suid[m] < uid)
			lo = m + 1;
		else
			hi = m - 1;
	}
	return 0;
}

int find_filtering(void)
{
	return on && (server || found[0]);
}

int find_match(HDR *h, int to)
{
	if (server)
		return found_uid(h->uid);
	return has(to ? h->to : h->from) || has(h->subject);
}

/* ---------------- the list's titles ---------------- */

/* the list pane's title and info line while it finds: 1, else 0 and the
   folder's own */
int find_titles(char *t, int tn, char *i, int in, long shown, long ondisk, long onserver)
{
	if (!find_filtering())
		return 0;
	if (server) {
		snprintf(t, tn, "Server: \"%s\" in %s: %s", found, cur_finfo->disp, num(shown));
		snprintf(i, in, nsuid >= SERVER_MAX ? "the newest %d" : "", SERVER_MAX);
		return 1;
	}
	/* the header's hint, at its right where it fits (win.c) */
	i[0] = 0;
	if (!cur_finfo->local && !cur_acct->pop && onserver > ondisk) {
		snprintf(t, tn, "Find \"%s\" in %s: %s of the %s on disk", found, cur_finfo->disp, num(shown),
			 num(ondisk));
		str_copy(i, "Search server: the rest", in);
	} else {
		snprintf(t, tn, "Find \"%s\" in %s: %s of %s", found, cur_finfo->disp, num(shown), num(ondisk));
	}
	return 1;
}

/* ---------------- the field's row ---------------- */

static int can_server(void)
{
	return cur_finfo && cur_acct && !cur_finfo->local && !cur_acct->pop && !cur_finfo->noselect;
}

static int server_greyed(void)
{
	return opt.offline || cur_acct->cut;
}

short find_row_h(void)
{
	return on ? ch + 8 : 0;
}

static void layout(void)
{
	WIN *w = &w_list;
	short x0 = TEXT_X(w->work.x), right = w->work.x + w->work.w - 4, bw;
	row.x = w->work.x;
	row.y = w->work.y;
	row.w = w->work.w;
	row.h = find_row_h();
	closer.w = 3 * cw;
	closer.h = ch + 2;
	closer.x = right - closer.w;
	closer.y = row.y + 3;
	bw = 15 * cw;			/* " Search server " */
	button.w = can_server() ? bw : 0;
	button.h = ch + 2;
	button.x = closer.x - 4 - button.w;
	button.y = row.y + 3;
	field.x = x0 - 4;
	field.y = row.y + 3;
	field.h = ch + 2;
	field.w = (button.w ? button.x : closer.x) - 6 - field.x;
	fl.show = (short)((field.w - 8) / cw);
	if (fl.show > LF_VIEW)
		fl.show = LF_VIEW;
	if (fl.show < 1)
		fl.show = 1;
	lf_view(&fl);
}

static void button_draw(GRECT *r, const char *l, short flags)
{
	short n = (short)strlen(l);
	fill(r, 0);
	frame(r, flags & TX_LIGHT ? pen[P_DIM] : 1);
	text_at(r->x + (r->w - n * cw) / 2, r->y + 1, l, n, n, TX_LTR | flags);
}

/* drawn by the list's draw(), its clip set */
void find_draw(GRECT *clip)
{
	GRECT c;
	if (!on)
		return;
	layout();
	if (clip->y >= row.y + row.h || clip->y + clip->h <= row.y)
		return;
	fill(&row, 0);
	c = field;
	frame(&c, 1);
	text_at(field.x + 4, field.y + 1, fl.view, strlen(fl.view), fl.show, TX_LTR);
	if (win_topmost() == &w_list) {	/* the keyboard is the field's: its cursor */
		c.x = field.x + 4 + (fl.cur - fl.off) * cw;
		c.y = field.y + 1;
		c.w = 2;
		c.h = ch;
		fill(&c, 1);
	}
	if (button.w)
		button_draw(&button, "Search server", server_greyed() ? TX_LIGHT : 0);
	button_draw(&closer, "x", 0);
}

static void row_redraw(void)
{
	layout();
	win_redraw(&w_list, &row);
}

/* the focus came to the list or left it: the cursor with it */
void find_focus(void)
{
	if (on)
		row_redraw();
}

/* ---------------- what is found ---------------- */

static void server_forget(void);

static void set_needle(void)
{
	short i;
	nlen = (short)strlen(found);
	for (i = 0; i < nlen; i++)
		needle[i] = fold[(unsigned char)found[i]];
	needle[nlen] = 0;
	first2 = needle[0];
	for (i = 0; i < 256; i++)
		if (fold[i] == needle[0] && i != needle[0])
			first2 = (unsigned char)i;
}

static void show_found(void)
{
	list_find_refresh(0, 0x7FFFFFFFL);	/* only the rows that changed */
	trace_found(list_count());
}

/* ---------------- the look through the folder, a step a turn ----------------
 * An 8 MHz ST looks through 500 messages' From and Subject in 95 to 155
 * ms: in steps of STEP ms, one a turn, and the list follows in the turn
 * after the last (the guidelines' 2.2: a long job never as one). A text
 * that only grows (another letter typed) looks only through what the
 * shorter one found. */
#define STEP 50

static long *src, nsrc, pos;	/* what is looked through, in date order; how far */
static long *res, nres;		/* what was found */
static int looking, show_due;
static int res_ok;		/* res is found's, for the folder as it was: */
static FOLDER *res_of;
static unsigned res_gen;

const long *find_results(long *n)
{
	*n = 0;
	if (!on || server || !res_ok || res_of != cur_folder || !cur_folder || res_gen != cur_folder->gen)
		return 0;
	*n = nres;
	return res;
}

static void look_stop(void)
{
	free(src);
	src = 0;
	nsrc = pos = 0;
	looking = show_due = 0;
}

/* the text only grew since what res holds was found */
static int narrows(void)
{
	short i, n = (short)strlen(found);
	long k;
	if (!find_results(&k) || !n || (short)strlen(text) < n)
		return 0;
	for (i = 0; i < n; i++)
		if (fold[(unsigned char)text[i]] != fold[(unsigned char)found[i]])
			return 0;
	return 1;
}

static void look_step(void)
{
	unsigned long t0 = pf_ms();
	HDR *h0;
	int to = show_to_list();
	if (!cur_folder || res_of != cur_folder || res_gen != cur_folder->gen) {
		/* the folder changed under the look (a check): from the start */
		look_stop();
		show_due = 1;	/* build_view looks through it at once */
		return;
	}
	h0 = cur_folder->h;
	while (pos < nsrc) {
		long k = src[pos++];
		if (find_match(&h0[k], to))
			res[nres++] = k;
		if (!(pos & 15) && (long)(pf_ms() - t0) >= STEP)
			return;
	}
	free(src);
	src = 0;
	looking = 0;
	res_ok = 1;
	show_due = 1;		/* the list in the next turn */
}

static void look_start(void)
{
	const long *from;
	long n, *r;
	int narrow = narrows() && !server;
	look_stop();
	if (narrow) {
		from = find_results(&n);
	} else {
		server_forget();
		from = list_order(&n);
	}
	strcpy(found, text);
	set_needle();
	res_ok = 0;
	if (!found[0] || !from) {
		show_found();	/* the whole folder (or build_view's own look) */
		return;
	}
	src = malloc((n ? n : 1) * sizeof(long));
	r = narrow ? res : realloc(res, (n ? n : 1) * sizeof(long));
	if (!src || !r) {
		free(src);
		src = 0;
		show_found();
		return;
	}
	memcpy(src, from, n * sizeof(long));
	res = r;
	nsrc = n;
	nres = pos = 0;
	res_of = cur_folder;
	res_gen = cur_folder->gen;
	looking = 1;
	look_step();
}

static void server_forget(void)
{
	free(suid);
	suid = 0;
	nsuid = 0;
	server = 0;
}

int find_waiting(void)
{
	return due != 0 || looking || show_due;
}

/* work for a quiet turn: the next step of the look, the list after it,
   or the typed text once the keys have paused; 1 when it did some */
int find_now(void)
{
	if (looking) {
		look_step();
		return 1;
	}
	if (show_due) {
		show_due = 0;
		show_found();
		return 1;
	}
	if (!due || (long)(pf_ms() - due) < 0)
		return 0;
	due = 0;
	if (!on || (!strcmp(found, text) && !server))
		return 0;
	look_start();
	return 1;
}

static void typed(void)
{
	look_stop();		/* what it was looking for has changed */
	due = pf_ms() + PAUSE;
	if (!due)
		due = 1;
	layout();
	win_redraw(&w_list, &field);	/* the field alone, not its row's buttons */
}

static void search_server(void)
{
	unsigned long *u;
	long n;
	int ok;
	if (!on || !can_server())
		return;
	if (server_greyed()) {
		alert(1, "[1][EMail is not connected.|Search server looks when it is:|Check mail to connect again.][ OK ]");
		return;
	}
	if (!text[0])
		return;
	due = 0;
	look_stop();
	busy(1);
	mail_err[0] = 0;
	ok = mail_search(cur_acct, cur_finfo, text, SERVER_MAX, &u, &n);
	busy(0);
	fold_done(cur_folder);	/* headers came into cur_folder: saved once */
	if (!ok) {
		if (mail_err[0])
			alert(1, "[1][%s][ OK ]", mail_err);
		list_refresh();
		return;
	}
	server_forget();
	suid = u;
	nsuid = n;
	server = 1;
	res_ok = 0;
	strcpy(found, text);
	set_needle();
	show_found();
	folders_build();
}

/* ---------------- opening and leaving ---------------- */

int find_active(void)
{
	return on;
}

/* Find's row came (dy > 0) or went: the list moved under it on the
   screen, Find's row drawn, or the rows it uncovered; drawn whole only
   where the pane isn't all in sight */
static void relayout(short dy)
{
	GRECT a = w_list.work;
	int moved = win_shift(&w_list, &a, dy);
	w_list.head_h = ch + 3 + find_row_h();
	if (!moved) {
		list_relayout();
		return;
	}
	if (dy > 0) {
		win_redraw(&w_list, &row);
		win_sliders_later(&w_list);
	} else {
		/* the rows from the one the strip at the foot starts in */
		short from = (short)((a.h + dy - w_list.head_h) / ch);
		list_find_refresh(-1, from < 0 ? 0 : from);
	}
}

void find_open(void)
{
	if (on) {
		win_focus(&w_list);
		return;
	}
	if (!cur_folder)
		return;
	if (!fold[1])
		fold_init();
	on = 1;
	text[0] = found[0] = 0;
	memset(&fl, 0, sizeof(fl));
	fl.text = text;
	fl.cap = FIND_CAP;
	due = 0;
	layout();
	win_top(&w_list);	/* from the editor too: the main window up */
	relayout(find_row_h());
	trace_on(1);
}

/* left: draw (0 when the caller draws the list itself: another folder) */
void find_close(int draw)
{
	short dy = find_row_h();
	if (!on)
		return;
	on = 0;
	due = 0;
	look_stop();
	res_ok = 0;
	trace_on(0);
	found[0] = 0;
	server_forget();
	if (!draw) {
		w_list.head_h = ch + 3;	/* the caller draws the list without the row */
		return;
	}
	relayout(-dy);	/* the whole folder again, the selection in sight */
}

void find_toggle(void)
{
	if (on)
		find_close(1);
	else
		find_open();
}

/* ---------------- keys and clicks ---------------- */

/* a key while the list has the keyboard: 1 when it was Find's */
int find_key(short kstate, short key)
{
	short scan = KEY_SCAN(key);
	unsigned char c = key_char(kstate, key);
	char buf[256], *p;
	if (!on)
		return 0;
	if (scan == 0x61) {			/* Undo: leave */
		find_close(1);
		return 1;
	}
	if ((scan == 0x1c || scan == 0x72) && (kstate & 3)) {	/* Shift+Return */
		search_server();
		return 1;
	}
	switch (clip_key(key)) {
	case CLIP_COPY:
		clip_put(text, (long)strlen(text));
		return 1;
	case CLIP_CUT:
		if (text[0] && clip_put(text, (long)strlen(text))) {
			text[0] = 0;
			lf_view(&fl);
			typed();
		}
		return 1;
	case CLIP_PASTE:
		clip_get(buf, sizeof(buf));
		for (p = clip_line(buf); *p; p++)
			lf_edit(&fl, (unsigned char)*p, (unsigned char)*p, 0);
		typed();
		return 1;
	}
	if (key_ctrl(kstate, key) || (kstate & K_ALT))
		return 0;			/* the menu's keys */
	if (scan == 0x48 || scan == 0x50 || scan == 0x47 || scan == 0x1c || scan == 0x72 || scan == 0x0f)
		return 0;			/* the list's: Up, Down, Home, Return, Tab */
	if (!lf_edit(&fl, key, c, kstate))
		return 0;
	typed();
	return 1;
}

static int in(GRECT *r, short x, short y)
{
	return r->w > 0 && x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

/* a click on Find's row */
void find_click(short mx, short my)
{
	if (!on)
		return;
	layout();
	win_focus(&w_list);
	if (in(&closer, mx, my)) {
		find_close(1);
	} else if (in(&button, mx, my)) {
		search_server();
	} else if (in(&field, mx, my)) {
		short k = (short)((mx - field.x - 4 + cw / 2) / cw);
		if (k < 0)
			k = 0;
		fl.cur = fl.off + k;
		lf_view(&fl);
		row_redraw();
	}
}

/* the tooltip over the row's buttons */
int find_tip(short mx, short my, char *out, int size)
{
	if (!on || my < row.y || my >= row.y + row.h || mx < row.x || mx >= row.x + row.w)
		return 0;
	if (in(&closer, mx, my))
		str_copy(out, "Leave Find (Undo, ^F)", size);
	else if (in(&button, mx, my))
		str_copy(out, server_greyed() ? "Offline: Search server looks when connected"
			       : "Look in the messages' text on the server (\x01Return)", size);
	else
		return 0;
	return 1;
}
