/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * win.c - EMail's windows.
 *
 * The main window holds three panes, like a modern mail program and
 * Claude ST's sidebar: folders on the left, the message list on the
 * right, the message below it. The dividers between them can be dragged,
 * each pane has a header strip and its own scroll bar, and the pane last
 * clicked has the keyboard (its header is drawn dark). The editor is a
 * window of its own with the usual GEM slider. Sizes go to EMAIL.INF.
 *
 * What the AES draws in its own look is the AES's (the guidelines, 5.1):
 * the main window is sized by its SIZER; the message pane, at the
 * window's right edge, scrolls with the window's own slider (Erez, 24 Oct
 * 2026: the column the AES keeps beside its size box is used, not left
 * empty), and the folder and list panes have bars of their own,
 * common/ui/panebar.c's, AES objects drawn by objc_draw.
 */
#include <string.h>
#include "ui.h"
#include "theme.h"
#include "util.h"
#include "bidi.h"
#include "iconify.h"
#include "panebar.h"

WIN w_folders, w_list, w_reader, w_editor;
static WIN *panes[] = { &w_folders, &w_list, &w_reader };
#define NPANE 3

short main_h = -1;
short main_iconified;			/* shown as EMail's icon (WM_ICONIFY) */
static GRECT mwork;			/* the main window's work area */
static WIN *focus = &w_list;
static char main_info[120];
static short sbw;			/* a pane's bar: the AES's slider column (pb_width) */
#define DIV 4				/* divider thickness */

#define EDKIND (NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE)
/* sized by the AES's own SIZER (5.1), and its slider the message pane's */
#define MAINKIND (NAME | CLOSER | FULLER | MOVER | SIZER | INFO | UPARROW | DNARROW | VSLIDE)

static int intersect(GRECT *a, GRECT *b)
{
	short x1 = a->x > b->x ? a->x : b->x;
	short y1 = a->y > b->y ? a->y : b->y;
	short x2 = (a->x + a->w < b->x + b->w) ? a->x + a->w : b->x + b->w;
	short y2 = (a->y + a->h < b->y + b->h) ? a->y + a->h : b->y + b->h;
	b->x = x1;
	b->y = y1;
	b->w = x2 - x1;
	b->h = y2 - y1;
	return b->w > 0 && b->h > 0;
}

static int inside(GRECT *r, short x, short y)
{
	return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

WIN *win_find(short handle)
{
	if (handle > 0 && handle == w_editor.h)
		return &w_editor;
	return 0;
}

long win_rows(WIN *w)
{
	long r = (w->work.h - w->head_h) / (w->line_h ? w->line_h : ch);
	return r > 0 ? r : 1;
}

static short header_h(void)
{
	return ch + 4;
}

/* ---------------- layout ---------------- */

static void set_pane(WIN *w, short x, short y, short wd, short ht)
{
	w->box.x = x;
	w->box.y = y;
	w->box.w = wd;
	w->box.h = ht;
	w->work.x = x;
	w->work.y = y + header_h();
	w->work.w = wd - (w == &w_reader ? 0 : sbw);	/* the reader: the window's slider */
	w->work.h = ht - header_h();
	if (w->work.h < 0)
		w->work.h = 0;
	w->line_h = ch;
}

/* place the panes in the main window from opt.pane_w / pane_h */
static void layout(void)
{
	short fw = opt.pane_w, lh = opt.pane_h, i;
	short minw = 10 * cw + sbw, minh = header_h() + 3 * ch;
	sbw = pb_width();
	if (!fw)
		fw = 24 * cw + sbw;
	if (!lh)
		lh = mwork.h * 2 / 5;
	if (fw > mwork.w - minw - DIV)
		fw = mwork.w - minw - DIV;
	if (fw < minw)
		fw = minw;
	if (lh > mwork.h - minh - DIV)
		lh = mwork.h - minh - DIV;
	if (lh < minh)
		lh = minh;
	opt.pane_w = fw;
	opt.pane_h = lh;
	set_pane(&w_folders, mwork.x, mwork.y, fw, mwork.h);
	set_pane(&w_list, mwork.x + fw + DIV, mwork.y, mwork.w - fw - DIV, lh);
	set_pane(&w_reader, mwork.x + fw + DIV, mwork.y + lh + DIV, mwork.w - fw - DIV, mwork.h - lh - DIV);
	w_list.head_h = ch + 3;
	for (i = 0; i < NPANE; i++) {
		WIN *w = panes[i];
		long max = w->total - win_rows(w);
		if (w->top > max)
			w->top = max > 0 ? max : 0;
		if (w->resized)
			w->resized(w);
	}
	win_sliders(&w_reader);	/* the window's slider: the message pane's */
}

static GRECT div_v(void)
{
	GRECT r;
	r.x = w_folders.box.x + w_folders.box.w;
	r.y = mwork.y;
	r.w = DIV;
	r.h = mwork.h;
	return r;
}

static GRECT div_h(void)
{
	GRECT r;
	r.x = w_list.box.x;
	r.y = w_list.box.y + w_list.box.h;
	r.w = w_list.box.w;
	r.h = DIV;
	return r;
}

/* ---------------- drawing ---------------- */

/* a pane's own bar; none (0 wide) for the reader, which has the
   window's slider */
static GRECT sb_rect(WIN *w)
{
	GRECT r;
	r.x = w->box.x + w->box.w - sbw;
	r.y = w->work.y;
	r.w = w == &w_reader ? 0 : sbw;
	r.h = w->work.h;
	return r;
}

/* a pane's bar as panebar.c wants it */
static void bar_of(WIN *w, PANEBAR *b)
{
	b->r = sb_rect(w);
	b->total = w->total;
	b->shown = win_rows(w);
	b->top = w->top;
	b->across = 0;		/* down the pane's right side */
}

static void draw_header(WIN *w)
{
	GRECT h;
	short dark = (w == focus), cols = w->box.w / cw - 1;
	short il = (short)strlen(w->info), tl = (short)strlen(w->title);
	h.x = w->box.x;
	h.y = w->box.y;
	h.w = w->box.w;
	h.h = header_h();
	fill(&h, dark ? pen[P_HEAD] : pen[P_PANEL]);
	if (il > cols - tl - 3)
		il = 0;			/* a hint shows whole, two cells clear, or not at all */
	text_at(h.x + cw / 2, h.y + 2, w->title, tl, cols, TX_BOLD | TX_LTR | (dark ? TX_INVERSE : 0));
	if (il > 0) {
		if (dark || !colour)
			text_at(h.x + w->box.w - (il + 1) * cw, h.y + 2, w->info, il, il,
				TX_LTR | (dark ? TX_INVERSE : TX_LIGHT));
		else
			text_pen_at(pen[P_DIM], h.x + w->box.w - (il + 1) * cw, h.y + 2, w->info, il, il, TX_LTR);
	}
	hline(h.x, h.x + h.w - 1, h.y + h.h - 1);
}

static void draw_divider(GRECT *r)
{
	short p[4];
	if (colour) {
		fill(r, pen[P_PANEL]);
		return;
	}
	fill(r, 0);
	vsf_interior(vdi_h, 2);
	vsf_style(vdi_h, 4);
	vsf_color(vdi_h, 1);
	p[0] = r->x;
	p[1] = r->y;
	p[2] = r->x + r->w - 1;
	p[3] = r->y + r->h - 1;
	vswr_mode(vdi_h, 1);
	vr_recfl(vdi_h, p);
}

/* everything of the main window inside clip (already set as VDI clip) */
#ifdef DRAW_TRACE
#include <stdio.h>
#include "../src/plat.h"
/* a test build (tests/hatari/scroll_test.py) says on the emulator's
   console what each pane drew, and the heap's bytes in use and most
   (libc.c): "DRAW list y h heap 123456 200000" */
extern long heap_use, heap_peak;
static void trace(const char *what, WIN *w, GRECT *r)
{
	char t[80];
	snprintf(t, sizeof(t), "%s %s %d %d heap %ld %ld", what,
		 w == &w_folders ? "folders" : w == &w_list ? "list" : "reader", r->y - w->work.y, r->h,
		 heap_use, heap_peak);
	pf_debug(t);
}
#else
#define trace(what, w, r) ((void)0)
#endif

static void draw_main(GRECT *clip)
{
	short i;
	GRECT r;
	for (i = 0; i < NPANE; i++) {
		WIN *w = panes[i];
		r = w->box;
		if (!intersect(clip, &r))
			continue;
		r.x = w->box.x;
		r.y = w->box.y;
		r.w = w->box.w;
		r.h = header_h();
		if (intersect(clip, &r))
			draw_header(w);
		r = w->work;
		if (intersect(clip, &r) && w->draw) {
			trace("DRAW", w, &r);
			clip_on(&r);
			w->draw(w, &r);
			clip_on(clip);
		}
		r = sb_rect(w);
		if (r.w > 0 && intersect(clip, &r)) {
			PANEBAR b;
			bar_of(w, &b);
			pb_draw(&b, &r);
			clip_on(clip);
		}
	}
	r = div_v();
	if (intersect(clip, &r))
		draw_divider(&r);
	r = div_h();
	if (intersect(clip, &r))
		draw_divider(&r);
}

/* redraw area of the main window through its rectangle list */
static void main_redraw(GRECT *area)
{
	GRECT r, a;
	if (main_h <= 0)
		return;
	if (main_iconified) {	/* the icon, not the panes (area is the old window's) */
		icon_window(main_h, 0);
		return;
	}
	a = area ? *area : mwork;
	if (!intersect(&mwork, &a))
		return;
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, 0);
	wind_get(main_h, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	while (r.w > 0 && r.h > 0) {
		GRECT c = r;
		if (intersect(&a, &c)) {
			clip_on(&c);
			draw_main(&c);
		}
		wind_get(main_h, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	graf_mouse(M_ON, 0);
	wind_update(END_UPDATE);
}

void win_redraw(WIN *w, GRECT *area)
{
	GRECT r, a;
	if (w->pane) {
		a = area ? *area : w->work;
		if (intersect(&w->box, &a))
			main_redraw(&a);
		return;
	}
	if (w->h <= 0 || !w->draw)
		return;
	if (w->iconified) {
		icon_window(w->h, area);
		return;
	}
	a = area ? *area : w->work;
	if (!intersect(&w->work, &a))
		return;
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, 0);
	wind_get(w->h, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	while (r.w > 0 && r.h > 0) {
		GRECT c = r;
		if (intersect(&a, &c)) {
			clip_on(&c);
			w->draw(w, &c);
		}
		wind_get(w->h, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	graf_mouse(M_ON, 0);
	wind_update(END_UPDATE);
}

void win_redraw_area(GRECT *area)
{
	main_redraw(area);
}

void win_redraw_all(void)
{
	main_redraw(0);
}

void win_redraw_lines(WIN *w, long first, long n)
{
	GRECT a;
	long y = (first - w->top) * w->line_h;
	if (y < 0) {
		n += y / w->line_h;
		y = 0;
	}
	if (n <= 0)
		return;
	a.x = w->work.x;
	a.y = w->work.y + w->head_h + (short)y;
	a.w = w->work.w;
	a.h = (short)(n * w->line_h);
	win_redraw(w, &a);
}

void win_sliders(WIN *w)
{
	long rows = win_rows(w), size, pos;
	short h = w->h;
	if (w->pane && w != &w_reader) {
		GRECT s = sb_rect(w);
		main_redraw(&s);
		return;
	}
	if (w == &w_reader)		/* the main window's slider */
		h = main_iconified ? -1 : main_h;
	if (h <= 0)
		return;
	if (w->total <= rows) {
		size = 1000;
		pos = 0;
	} else {
		size = rows * 1000L / w->total;
		pos = w->top * 1000L / (w->total - rows);
	}
	if (size < 30)
		size = 30;
	if (pos > 1000)
		pos = 1000;
	wind_set(h, WF_VSLSIZE, (short)size, 0, 0, 0);
	wind_set(h, WF_VSLIDE, (short)pos, 0, 0, 0);
}

/* GEM draws titles as stored: lay Hebrew out right to left first */
static void visual(char *dst, const char *src, int size)
{
	short n = (short)strlen(src);
	if (n >= size)
		n = size - 1;
	if (n > BIDI_MAX)
		n = BIDI_MAX;
	if (bidi_has_rtl(src, n)) {
		bidi_visual(src, n, 0, dst);
		dst[n] = 0;
	} else {
		str_copy(dst, src, size);
	}
	heb_font_map(dst, (long)strlen(dst), opt.hebfont);
}

static void redraw_header(WIN *w)
{
	GRECT h;
	h.x = w->box.x;
	h.y = w->box.y;
	h.w = w->box.w;
	h.h = header_h();
	main_redraw(&h);
}

void win_title(WIN *w, const char *title)
{
	if (w->pane) {
		/* pane headers are drawn by text_at, which lays Hebrew out */
		if (strcmp(w->title, title)) {
			str_copy(w->title, title, sizeof(w->title));
			redraw_header(w);
		}
		return;
	}
	visual(w->title, title, sizeof(w->title));
	if (w->h > 0)
		wind_set_str(w->h, WF_NAME, w->title);
}

void win_info(WIN *w, const char *info)
{
	char v[sizeof(w->info)];
	if (w->pane) {
		if (strcmp(w->info, info)) {
			str_copy(w->info, info, sizeof(w->info));
			redraw_header(w);
		}
		return;
	}
	visual(v, info, sizeof(v));
	if (!strcmp(w->info, v))
		return;
	str_copy(w->info, v, sizeof(w->info));
	if (w->h > 0)
		wind_set_str(w->h, WF_INFO, w->info);
}

/* the main window's info line: progress and status */
void main_status(const char *s)
{
	char v[sizeof(main_info)];
	visual(v, s, sizeof(v));
	if (!strcmp(v, main_info))
		return;
	str_copy(main_info, v, sizeof(main_info));
	if (main_h > 0)
		wind_set_str(main_h, WF_INFO, main_info);
}

/* ---------------- opening and closing ---------------- */

static void get_main_work(void)
{
	wind_get(main_h, WF_WORKXYWH, &mwork.x, &mwork.y, &mwork.w, &mwork.h);
}

static void save_main_place(void)
{
	wind_get(main_h, WF_CURRXYWH, &opt.main_x, &opt.main_y, &opt.main_w, &opt.main_h);
}

void main_open(void)
{
	short x = opt.main_x, y = opt.main_y, wd = opt.main_w, ht = opt.main_h;
	if (main_h > 0) {
		wind_set(main_h, WF_TOP, 0, 0, 0, 0);
		return;
	}
	/* the saved place, if it still fits this screen */
	if (wd < 30 * cw || ht < 10 * ch || x < 0 || y < desk_y ||
	    x + wd > desk_x + desk_w + 4 || y + ht > desk_y + desk_h + 4) {
		x = desk_x;
		y = desk_y;
		wd = desk_w;
		ht = desk_h;
	}
	main_h = wind_create(icon_kind(MAINKIND), desk_x, desk_y, desk_w, desk_h);
	main_iconified = 0;
	if (main_h < 0) {
		alert(1, "[3][No more windows available.][ OK ]");
		return;
	}
	wind_set_str(main_h, WF_NAME, " EMail ");
	wind_set_str(main_h, WF_INFO, main_info);
	wind_open(main_h, x, y, wd, ht);
	get_main_work();
	save_main_place();
	layout();
}

void main_close(void)
{
	if (main_h <= 0)
		return;
	if (!main_iconified)	/* the place was kept when it was iconified */
		save_main_place();
	wind_close(main_h);
	wind_delete(main_h);
	main_h = -1;
	main_iconified = 0;
}

/* the fonts changed: lay the panes out again */
void win_relayout(void)
{
	if (main_h > 0 && !main_iconified) {
		layout();
		main_redraw(0);
	}
	if (w_editor.h > 0 && !w_editor.iconified) {
		if (w_editor.resized)
			w_editor.resized(&w_editor);
		w_editor.line_h = ch;
		win_sliders(&w_editor);
		win_redraw(&w_editor, 0);
	}
}

void win_open(WIN *w)
{
	if (w->pane) {
		main_open();
		return;
	}
	if (w->h > 0) {
		win_top(w);
		return;
	}
	w->line_h = ch;
	w->h = wind_create(icon_kind(EDKIND), desk_x, desk_y, desk_w, desk_h);
	w->iconified = 0;
	if (w->h < 0) {
		alert(1, "[3][No more windows available.|Close a window and try again.][ OK ]");
		return;
	}
	wind_set_str(w->h, WF_NAME, w->title);
	wind_set_str(w->h, WF_INFO, w->info);
	wind_open(w->h, w->place.x, w->place.y, w->place.w, w->place.h);
	wind_get(w->h, WF_WORKXYWH, &w->work.x, &w->work.y, &w->work.w, &w->work.h);
	win_sliders(w);
}

void win_close(WIN *w)
{
	if (w->pane || w->h <= 0)
		return;
	if (!w->iconified)	/* else the place was kept when it was iconified */
		wind_get(w->h, WF_CURRXYWH, &w->place.x, &w->place.y, &w->place.w, &w->place.h);
	if (w == &w_editor) {
		opt.ed_x = w->place.x;
		opt.ed_y = w->place.y;
		opt.ed_w = w->place.w;
		opt.ed_h = w->place.h;
	}
	wind_close(w->h);
	wind_delete(w->h);
	w->h = -1;
	w->iconified = 0;
}

/* keyboard focus to a pane (its header goes dark) */
void win_focus(WIN *w)
{
	WIN *old = focus;
	if (!w->pane || w == focus)
		return;
	focus = w;
	redraw_header(old);
	redraw_header(w);
}

void win_top(WIN *w)
{
	if (w->pane) {
		if (main_h > 0)
			wind_set(main_h, WF_TOP, 0, 0, 0, 0);
		win_focus(w);
		return;
	}
	if (w->h > 0)
		wind_set(w->h, WF_TOP, 0, 0, 0, 0);
}

WIN *win_topmost(void)
{
	short t, d;
	wind_get(0, WF_TOP, &t, &d, &d, &d);
	if (t == main_h && main_h > 0)
		return focus;
	return win_find(t);
}

/* the top window is shown as an icon: it takes no keys */
int win_icon_on_top(void)
{
	short t, d;
	wind_get(0, WF_TOP, &t, &d, &d, &d);
	return t > 0 && ((t == main_h && main_iconified) || (t == w_editor.h && w_editor.iconified));
}

int win_is_top(WIN *w)
{
	return win_topmost() == w;
}

/* ---------------- scrolling ---------------- */

/* A pane scrolled by fewer rows than it shows: the rows that stay move
   on the screen with one raster copy and only the new ones are drawn. A
   GDOS font's rows are drawn a letter at a time, so a pane drawn afresh
   at every step was seen filling in (Erez's video, 20 Oct 2026). Only
   where nothing covers the pane; else 0, and it is drawn as before */
static int scroll_copy(WIN *w, GRECT *a, long d)
{
	GRECT r, strip;
	short pxy[8], dy;
	MFDB scr;
	long rows = win_rows(w);
	if (!w->pane || main_h <= 0 || main_iconified || !d || d >= rows || -d >= rows)
		return 0;
	dy = (short)((d < 0 ? -d : d) * w->line_h);
	if (dy >= a->h)
		return 0;
	wind_update(BEG_UPDATE);
	wind_get(main_h, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	if (r.x > a->x || r.y > a->y || r.x + r.w < a->x + a->w || r.y + r.h < a->y + a->h) {
		wind_update(END_UPDATE);
		return 0;
	}
	memset(&scr, 0, sizeof(scr));	/* the screen */
	pxy[0] = pxy[4] = a->x;
	pxy[2] = pxy[6] = a->x + a->w - 1;
	strip = *a;
	strip.h = dy;
	if (d > 0) {			/* down the list: the rows move up */
		pxy[1] = a->y + dy;
		pxy[3] = a->y + a->h - 1;
		pxy[5] = a->y;
		pxy[7] = a->y + a->h - 1 - dy;
		strip.y = a->y + a->h - dy;
	} else {
		pxy[1] = a->y;
		pxy[3] = a->y + a->h - 1 - dy;
		pxy[5] = a->y + dy;
		pxy[7] = a->y + a->h - 1;
	}
	graf_mouse(M_OFF, 0);
	clip_on(a);			/* NVDI clips raster copies: the pane's own */
	vro_cpyfm(vdi_h, 3, pxy, &scr, &scr);
	trace("COPY", w, a);
	graf_mouse(M_ON, 0);
	wind_update(END_UPDATE);
	win_redraw(w, &strip);
	return 1;
}

void win_scroll_to(WIN *w, long top)
{
	long old = w->top;
	long rows = win_rows(w), max = w->total - rows;
	if (max < 0)
		max = 0;
	if (top > max)
		top = max;
	if (top < 0)
		top = 0;
	if (top == w->top)
		return;
	w->top = top;
	if (w->pane && w != &w_reader) {
		/* only the slider's old and new places (all of the bar took an
		   8 MHz ST 70 ms) */
		PANEBAR b;
		GRECT m;
		bar_of(w, &b);
		m = pb_moved(&b, old);
		main_redraw(&m);
	} else {
		win_sliders(w);
	}
	{
		GRECT a = w->work;
		a.y += w->head_h;
		a.h -= w->head_h;
		if (!scroll_copy(w, &a, top - old))
			win_redraw(w, &a);
	}
	if (w->scrolled)
		w->scrolled(w);
}

void win_ensure_visible(WIN *w, long line)
{
	long rows = win_rows(w);
	if (line < w->top)
		win_scroll_to(w, line);
	else if (line >= w->top + rows)
		win_scroll_to(w, line - rows + 1);
}

/* the pointer shape win_hover() last set; drags set it back to ARROW */
static short hover_shape = ARROW;

static short button_down(short *mx, short *my)
{
	short mb, ks;
	graf_mkstate(mx, my, &mb, &ks);
	return mb & 3;
}

/* a press on a pane's bar: panebar.c's arrows, track and slider */
static WIN *bar_win;
static PANEBAR bar_held;

static int bar_scroll(long n)
{
	long old = bar_win->top;
	win_scroll_to(bar_win, old + n);
	bar_held.top = bar_win->top;
	return bar_win->top != old;
}

static int bar_goto(long t)
{
	long old = bar_win->top;
	win_scroll_to(bar_win, t);
	bar_held.top = bar_win->top;
	return bar_win->top != old;
}

static void scrollbar_press(WIN *w, short mx, short my)
{
	bar_win = w;
	bar_of(w, &bar_held);
	pb_click(&bar_held, mx, my, bar_scroll, bar_goto);
}

/* drag a divider; vertical: between folders and the rest */
static void divider_drag(int vertical)
{
	short mx, my, last = -1, p[4], held;
	GRECT lim = mwork;
	wind_update(BEG_UPDATE);
	wind_update(3);
	graf_mouse(FLAT_HAND, 0);		/* as Claude ST's divider */
	clip_on(&lim);
	vswr_mode(vdi_h, 3);			/* XOR */
	vsl_color(vdi_h, 1);
	/* do..while: the press may have been reported only after the
	   release, then the pointer's place is where it was dropped */
	do {
		short pos;
		held = button_down(&mx, &my);
		pos = vertical ? mx : my;
		if (pos != last) {
			graf_mouse(M_OFF, 0);
			if (last >= 0)
				v_pline(vdi_h, 2, p);
			if (vertical) {
				p[0] = p[2] = mx;
				p[1] = mwork.y;
				p[3] = mwork.y + mwork.h - 1;
			} else {
				p[0] = w_list.box.x;
				p[2] = w_list.box.x + w_list.box.w - 1;
				p[1] = p[3] = my;
			}
			v_pline(vdi_h, 2, p);
			graf_mouse(M_ON, 0);
			last = pos;
		}
		evnt_timer_(10);
	} while (held);
	if (last >= 0) {
		graf_mouse(M_OFF, 0);
		v_pline(vdi_h, 2, p);
		graf_mouse(M_ON, 0);
	}
	vswr_mode(vdi_h, 1);
	graf_mouse(ARROW, 0);
	hover_shape = ARROW;
	wind_update(2);
	wind_update(END_UPDATE);
	if (last < 0)
		return;
	if (vertical)
		opt.pane_w = last - mwork.x;
	else
		opt.pane_h = last - mwork.y;
	layout();
	main_redraw(0);
}

/* where a divider can be grabbed: a little wider than it is drawn */
static int on_divider(short mx, short my, int vertical)
{
	GRECT r = vertical ? div_v() : div_h();
	if (main_h <= 0)
		return 0;
	if (vertical) {
		r.x -= 3;
		r.w += 6;
	} else {
		r.y -= 3;
		r.h += 6;
	}
	return inside(&r, mx, my);
}

/* the pointer shows the sizing arrows over a divider */
void win_hover(short mx, short my)
{
	short h = wind_find(mx, my), t, d, want = ARROW;
	wind_get(0, WF_TOP, &t, &d, &d, &d);
	if (h == main_h && t == main_h && !main_iconified) {
		/* the flat hand, as over Claude ST's divider */
		if (on_divider(mx, my, 1) || on_divider(mx, my, 0))
			want = FLAT_HAND;
	}
	if (want != hover_shape) {
		graf_mouse(want, 0);
		hover_shape = want;
	}
	/* the next move wakes us again */
	evnt_set_m1(1, mx, my, 1, 1);
}

/* a mouse press anywhere; returns 1 if it was ours */
int win_mouse(short mx, short my, short button, short clicks, short kstate)
{
	short h = wind_find(mx, my), i;
	GRECT r;
	if (h > 0 && ((h == main_h && main_iconified) || (h == w_editor.h && w_editor.iconified)))
		return 0;	/* an icon takes no clicks */
	if (h > 0 && h == main_h) {
		if (on_divider(mx, my, 1)) {
			divider_drag(1);
			return 1;
		}
		if (on_divider(mx, my, 0)) {
			divider_drag(0);
			return 1;
		}
		for (i = 0; i < NPANE; i++) {
			WIN *w = panes[i];
			if (!inside(&w->box, mx, my))
				continue;
			win_focus(w);
			r = sb_rect(w);
			if (inside(&r, mx, my)) {
				scrollbar_press(w, mx, my);
			} else if (inside(&w->work, mx, my)) {
				if ((button & 2) && w->rclick)
					w->rclick(w, mx, my);
				else if (w->click)
					w->click(w, mx, my, clicks, kstate);
			}
			return 1;
		}
		return 1;
	}
	if (h > 0 && h == w_editor.h && inside(&w_editor.work, mx, my)) {
		if ((button & 2) && w_editor.rclick)
			w_editor.rclick(&w_editor, mx, my);
		else if (w_editor.click)
			w_editor.click(&w_editor, mx, my, clicks, kstate);
		return 1;
	}
	return 0;
}

/* ---------------- AES messages ---------------- */

/* the window's arrows and slider, for w (the editor, or the message
   pane through the main window's) */
static void slider_message(WIN *w, short *msg)
{
	long rows = win_rows(w), max = w->total - rows;
	if (msg[0] == WM_VSLID) {
		if (max > 0)
			win_scroll_to(w, (long)msg[4] * max / 1000);
		return;
	}
	switch (msg[4]) {
	case 0: win_scroll_to(w, w->top - rows); break;
	case 1: win_scroll_to(w, w->top + rows); break;
	case 2: win_scroll_to(w, w->top - 1); break;
	case 3: win_scroll_to(w, w->top + 1); break;
	}
}

void main_closed(void);

static void main_message(short *msg)
{
	GRECT r;
	switch (msg[0]) {
	case WM_REDRAW:
		r.x = msg[4];
		r.y = msg[5];
		r.w = msg[6];
		r.h = msg[7];
		if (main_iconified)
			icon_window(main_h, &r);
		else
			main_redraw(&r);
		break;
	case WM_ICONIFY:
	case WM_ALLICONIFY:
		if (main_iconified)
			break;
		save_main_place();
		main_iconified = 1;
		wind_set(main_h, WF_ICONIFY, msg[4], msg[5], msg[6], msg[7]);
		break;
	case WM_UNICONIFY:
		if (!main_iconified)
			break;
		wind_set(main_h, WF_UNICONIFY, msg[4], msg[5], msg[6], msg[7]);
		main_iconified = 0;
		get_main_work();
		layout();
		main_redraw(0);
		break;
	case WM_TOPPED:
	case WM_NEWTOP: {
		short mx, my, mb, ks;
		wind_set(main_h, WF_TOP, 0, 0, 0, 0);
		/* the AES tops a window on a click in its contents and keeps
		   the click: act on it as well, as a modern program does */
		graf_mkstate(&mx, &my, &mb, &ks);
		if (msg[0] == WM_TOPPED && !main_iconified && inside(&mwork, mx, my))
			win_mouse(mx, my, mb ? mb : 1, 1, ks);
		break;
	}
	case WM_CLOSED:
		main_closed();
		break;
	case WM_ARROWED:
	case WM_VSLID:
		if (!main_iconified)
			slider_message(&w_reader, msg);
		break;
	case WM_BOTTOMED:
		wind_set(main_h, WF_BOTTOM, 0, 0, 0, 0);
		break;
	case WM_FULLED: {
		GRECT c, f, p;
		if (main_iconified)
			break;
		wind_get(main_h, WF_CURRXYWH, &c.x, &c.y, &c.w, &c.h);
		wind_get(main_h, WF_FULLXYWH, &f.x, &f.y, &f.w, &f.h);
		if (c.x == f.x && c.y == f.y && c.w == f.w && c.h == f.h) {
			wind_get(main_h, WF_PREVXYWH, &p.x, &p.y, &p.w, &p.h);
			wind_set(main_h, WF_CURRXYWH, p.x, p.y, p.w, p.h);
		} else {
			wind_set(main_h, WF_CURRXYWH, f.x, f.y, f.w, f.h);
		}
		get_main_work();
		save_main_place();
		layout();
		main_redraw(0);
		break;
	}
	case WM_SIZED:
	case WM_MOVED:
		if (main_iconified) {	/* the icon goes where it is put, as it is */
			wind_set(main_h, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
			break;
		}
		if (msg[6] < 40 * cw)
			msg[6] = 40 * cw;
		if (msg[7] < 12 * ch)
			msg[7] = 12 * ch;
		wind_set(main_h, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
		get_main_work();
		save_main_place();
		layout();
		if (msg[0] == WM_SIZED)
			main_redraw(0);
		break;
	}
}

void win_message(short *msg)
{
	WIN *w;
	GRECT r;
	if (msg[3] == main_h && main_h > 0) {
		main_message(msg);
		menu_update();
		return;
	}
	w = win_find(msg[3]);
	if (!w)
		return;
	switch (msg[0]) {
	case WM_REDRAW:
		r.x = msg[4];
		r.y = msg[5];
		r.w = msg[6];
		r.h = msg[7];
		win_redraw(w, &r);
		break;
	case WM_ICONIFY:
	case WM_ALLICONIFY:
		if (w->iconified)
			break;
		wind_get(w->h, WF_CURRXYWH, &w->place.x, &w->place.y, &w->place.w, &w->place.h);
		w->iconified = 1;
		wind_set(w->h, WF_ICONIFY, msg[4], msg[5], msg[6], msg[7]);
		break;
	case WM_UNICONIFY:
		if (!w->iconified)
			break;
		wind_set(w->h, WF_UNICONIFY, msg[4], msg[5], msg[6], msg[7]);
		w->iconified = 0;
		wind_get(w->h, WF_WORKXYWH, &w->work.x, &w->work.y, &w->work.w, &w->work.h);
		if (w->resized)
			w->resized(w);
		win_scroll_to(w, w->top);
		win_sliders(w);
		win_redraw(w, 0);
		break;
	case WM_TOPPED:
	case WM_NEWTOP: {
		short mx, my, mb, ks;
		win_top(w);
		graf_mkstate(&mx, &my, &mb, &ks);
		if (msg[0] == WM_TOPPED && !w->iconified && w->click && inside(&w->work, mx, my))
			w->click(w, mx, my, 1, ks);
		menu_update();
		break;
	}
	case WM_CLOSED:
		if (w->closed)
			w->closed(w);
		else
			win_close(w);
		menu_update();
		break;
	case WM_BOTTOMED:
		wind_set(w->h, WF_BOTTOM, 0, 0, 0, 0);
		break;
	case WM_FULLED: {
		GRECT c, f, p;
		if (w->iconified)
			break;
		wind_get(w->h, WF_CURRXYWH, &c.x, &c.y, &c.w, &c.h);
		wind_get(w->h, WF_FULLXYWH, &f.x, &f.y, &f.w, &f.h);
		if (c.x == f.x && c.y == f.y && c.w == f.w && c.h == f.h) {
			wind_get(w->h, WF_PREVXYWH, &p.x, &p.y, &p.w, &p.h);
			wind_set(w->h, WF_CURRXYWH, p.x, p.y, p.w, p.h);
		} else {
			wind_set(w->h, WF_CURRXYWH, f.x, f.y, f.w, f.h);
		}
		wind_get(w->h, WF_WORKXYWH, &w->work.x, &w->work.y, &w->work.w, &w->work.h);
		if (w->resized)
			w->resized(w);
		win_scroll_to(w, w->top);
		win_sliders(w);
		win_redraw(w, 0);
		break;
	}
	case WM_SIZED:
	case WM_MOVED:
		if (w->iconified) {
			wind_set(w->h, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
			break;
		}
		if (msg[6] < 12 * cw)
			msg[6] = 12 * cw;
		if (msg[7] < 6 * ch)
			msg[7] = 6 * ch;
		wind_set(w->h, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
		wind_get(w->h, WF_WORKXYWH, &w->work.x, &w->work.y, &w->work.w, &w->work.h);
		if (w == &w_editor) {
			opt.ed_x = msg[4];
			opt.ed_y = msg[5];
			opt.ed_w = msg[6];
			opt.ed_h = msg[7];
		}
		if (msg[0] == WM_SIZED) {
			if (w->resized)
				w->resized(w);
			win_scroll_to(w, w->top);
			win_sliders(w);
			win_redraw(w, 0);
		}
		break;
	case WM_ARROWED:
	case WM_VSLID:
		slider_message(w, msg);
		break;
	}
}
