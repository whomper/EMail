/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * dialogs.c - EMail's dialogs, built in code (no .RSC file needed):
 * account setup, settings, a one-line question, list pickers, alerts.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "ui.h"
#include "dialog.h"
#include "field.h"
#include "bidi.h"
#include "util.h"
#include "charset.h"
#include "tz.h"
#include "clip.h"

#define DMAX 72
#define TMAX 24

static OBJECT tree[DMAX];
static void make_eyes(void);
static short nlf, npw;
static TEDINFO ted[TMAX];
static char tmpl[TMAX][64], valid[TMAX][64];
static short nobj, nted, last_child;

/* ---------------- builder ---------------- */

OBJECT *d_tree(void)
{
	return tree;
}

void d_begin(short w, short h)
{
	static short eyes_made;
	if (!eyes_made) {
		make_eyes();
		eyes_made = 1;
	}
	lf_forget(tree);
	nlf = npw = 0;
	memset(tree, 0, sizeof(tree));
	nobj = 1;
	nted = 0;
	last_child = 0;
	tree[0].ob_next = -1;
	tree[0].ob_head = tree[0].ob_tail = -1;
	tree[0].ob_type = G_BOX;
	tree[0].ob_state = OUTLINED;
	tree[0].ob_spec = 0x00021100L;
	tree[0].ob_width = w;
	tree[0].ob_height = h;
}

short d_add(short type, short flags, short state, long spec, short x, short y, short w, short h)
{
	short i = nobj++;
	OBJECT *o = &tree[i];
	o->ob_next = 0;
	o->ob_head = o->ob_tail = -1;
	o->ob_type = type;
	o->ob_flags = flags;
	o->ob_state = state;
	o->ob_spec = spec;
	o->ob_x = x;
	o->ob_y = y;
	o->ob_width = w;
	o->ob_height = h;
	if (last_child)
		tree[last_child].ob_next = i;
	else
		tree[0].ob_head = i;
	tree[0].ob_tail = i;
	last_child = i;
	return i;
}

short d_text(short x, short y, const char *s)
{
	return d_add(G_STRING, 0, 0, (long)s, x, y, (short)strlen(s), 1);
}

/* an editable field showing len characters of buf (buf holds len+1) */
static short d_edit(short x, short y, char *buf, short len, char kind)
{
	TEDINFO *t = &ted[nted];
	short i;
	if (len > 60)
		len = 60;
	for (i = 0; i < len; i++) {
		tmpl[nted][i] = '_';
		valid[nted][i] = kind;
	}
	tmpl[nted][len] = valid[nted][len] = 0;
	buf[len] = 0;
	t->te_ptext = buf;
	t->te_ptmplt = tmpl[nted];
	t->te_pvalid = valid[nted];
	t->te_font = 3;
	t->te_just = 0;
	t->te_color = 0x1180;
	t->te_thickness = 0;
	t->te_txtlen = len + 1;
	t->te_tmplen = len + 1;
	nted++;
	return d_add(G_FTEXT, EDITABLE, 0, (long)t, x, y, len, 1);
}

short d_button(short x, short y, short w, const char *s, short flags)
{
	return d_add(G_BUTTON, SELECTABLE | flags, 0, (long)s, x, y, w, 1);
}

short d_check(short x, short y, const char *s, short on)
{
	return d_add(G_BUTTON, SELECTABLE, on ? SELECTED : 0, (long)s, x, y, (short)strlen(s) + 2, 1);
}

void d_end(void)
{
	short i;
	tree[last_child].ob_next = 0;
	tree[nobj - 1].ob_flags |= LASTOB;
	for (i = 0; i < nobj; i++)
		rsrc_obfix(tree, i);
	dlg_3d(tree);		/* the AES's 3D look where it has one (5.6) */
}

/* ---------------- long fields and passwords ----------------
 * A field holds as much as its setting does and shows a window of it
 * that scrolls with the cursor (common/ui/field.c; the AERA guidelines,
 * 5.4): GEM's own fields hold only what they show. A password shows
 * stars; the eye button next to it shows it in clear. form_loop() below
 * is form_do() written out with form_keybd/form_button/objc_edit, so the
 * long fields take their own keys. */

#define LFMAX 12
#define PWMAX 2

static LFIELD lfs[LFMAX];

/* the eye: 16 pixels wide, 16 rows (8 on ST medium), open and crossed */
static const char *const eye_art[16] = {
	"................",
	"................",
	"................",
	".....######.....",
	"...##......##...",
	"..#....##....#..",
	".#....####....#.",
	"#.....####.....#",
	".#....####....#.",
	"..#....##....#..",
	"...##......##...",
	".....######.....",
	"................",
	"................",
	"................",
	"................",
};
static unsigned short eye_open[16], eye_shut[16];

/* a tick box, 16 pixels square, and the tick in it */
static const char *tick_art[16] = {
	"................",
	".##############.",
	".#............#.",
	".#..........#.#.",
	".#.........##.#.",
	".#........##..#.",
	".#.......##...#.",
	".#.##...##....#.",
	".#.###.##.....#.",
	".#..####......#.",
	".#...##.......#.",
	".#............#.",
	".#............#.",
	".#............#.",
	".##############.",
	"................",
};
static unsigned short tick_on[16], tick_off[16];
static BITBLK tick_on_blk, tick_off_blk;

static void make_ticks(void)
{
	/* half height on 8-pixel screens: every other line, and the frame */
	short tall = gl_hchar >= 16, rows = tall ? 16 : 8, r, c;
	for (r = 0; r < rows; r++) {
		const char *src = tick_art[tall ? r : (r == rows - 1 ? 14 : r * 2 + (r ? 0 : 1))];
		unsigned short v = 0, frame = 0;
		for (c = 0; c < 16; c++) {
			v = (unsigned short)((v << 1) | (src[c] == '#'));
			frame = (unsigned short)((frame << 1) | (src[c] == '#' && (c == 1 || c == 14 || r == 0 || r == rows - 1 ||
										(tall && (r == 1 || r == 14)))));
		}
		tick_on[r] = v;
		tick_off[r] = tall ? (r == 1 || r == 14 ? v : (r == 0 || r == 15 ? 0 : 0x4002)) : (r == 0 || r == rows - 1 ? v : 0x4002);
	}
	tick_on_blk.bi_pdata = tick_on;
	tick_off_blk.bi_pdata = tick_off;
	tick_on_blk.bi_wb = tick_off_blk.bi_wb = 2;
	tick_on_blk.bi_hl = tick_off_blk.bi_hl = rows;
	tick_on_blk.bi_color = tick_off_blk.bi_color = 1;
}
static BITBLK eye_blk[PWMAX];
static short eye_obj[PWMAX];	/* its button */
static LFIELD *eye_lf[PWMAX];	/* and its field */

static void make_eyes(void)
{
	short tall = gl_hchar >= 16, r, rows = tall ? 16 : 8, c;
	for (r = 0; r < rows; r++) {
		const char *src = eye_art[tall ? r : r * 2 + 1];
		unsigned short v = 0;
		for (c = 0; c < 16; c++)
			v = (v << 1) | (src[c] == '#');
		eye_open[r] = v;
		/* crossed out: a diagonal from bottom left to top right */
		eye_shut[r] = v | (0x8000 >> ((rows - 1 - r) * 16 / rows)) | (0x4000 >> ((rows - 1 - r) * 16 / rows));
	}
}

/* a field showing len characters of text, which holds cap (and cap+1
   bytes); mask '*': a password, with its eye button after it */
static short d_long(short x, short y, char *text, short len, short cap, char mask)
{
	LFIELD *f = &lfs[nlf++];
	short obj = d_edit(x, y, f->view, len, 'X');
	lf_init(f, tree, obj, text, cap, mask);
	if (mask) {
		BITBLK *b = &eye_blk[npw];
		b->bi_pdata = eye_open;
		b->bi_wb = 2;
		b->bi_hl = gl_hchar >= 16 ? 16 : 8;
		b->bi_x = b->bi_y = 0;
		b->bi_color = 1;
		eye_lf[npw] = f;
		eye_obj[npw++] = d_add(G_IMAGE, TOUCHEXIT, 0, (long)b, x + len + 1, y, 2, 1);
	}
	return obj;
}

static LFIELD *lf_at(short obj)
{
	short i;
	for (i = 0; i < nlf; i++)
		if (lfs[i].obj == obj && obj > 0)
			return &lfs[i];
	return 0;
}

/* an object drawn again where the dialog is now, through what shows of
   it (a dialog may be a window, and moved: the guidelines, 3.5) */
static void draw_obj(short obj)
{
	dlg_draw(tree, obj);
}

static short first_editable(void)
{
	short i;
	for (i = 1; i < nobj; i++)
		if (tree[i].ob_flags & EDITABLE)
			return i;
	return 0;
}

/* what Esc presses (the guidelines, 3.2): Cancel, else the only button
   that ends the dialog (OK in a box that only says something); 0 when
   neither is there */
static short esc_button(void)
{
	short i, exits = 0, last = 0;
	for (i = 1; i < nobj; i++) {
		if (tree[i].ob_type != G_BUTTON || !(tree[i].ob_flags & EXIT))
			continue;
		if (!strcmp((const char *)tree[i].ob_spec, "Cancel"))
			return i;
		exits++;
		last = i;
	}
	return exits == 1 ? last : 0;
}

/* form_do, with password fields, the clipboard (^C ^X ^V) and Esc for
   Cancel */
static short form_loop(short start)
{
	short next = start ? start : first_editable(), edit = 0, idx = 0, cont = 1;
	short m[8];
	AEVENT e;
	while (cont) {
		if (next && next != edit) {
			edit = next;
			next = 0;
			if (lf_at(edit))
				lf_enter(lf_at(edit), &idx);
			else
				objc_edit(tree, edit, 0, &idx, ED_INIT);
		}
		/* messages too: EContact's answers can't wait (the guidelines, 4.1) */
		dlg_evnt(MU_KEYBD | MU_BUTTON | MU_MESAG | MU_TIMER, 2, 1, 1, 250, m, &e);
		if (e.which & MU_MESAG)
			ui_modal_message(m);
		if (e.which & MU_KEYBD) {
			LFIELD *lf = lf_at(edit);
			short scan = KEY_SCAN(e.kreturn);
			if ((scan == 0x01 || scan == 0x61) && esc_button()) {
				/* Esc or Undo: Cancel, as if clicked, even in a text
				   field (the AERA guidelines, 3.5) */
				next = esc_button();
				cont = 0;
			} else if (lf && lf_key(lf, e.kreturn, KEY_ASCII(e.kreturn), e.kstate, &idx)) {
				/* the long field took it: a letter, an edit, ^C ^X ^V */
			} else if (edit && !lf && clip_key(e.kreturn)) {
				clip_edit(tree, edit, &idx, e.kreturn);
			} else {
				short kr;
				cont = form_keybd(tree, edit, next, e.kreturn, &next, &kr);
				if (kr && !lf)	/* a long field's text isn't objc_edit's */
					objc_edit(tree, edit, kr, &idx, ED_CHAR);
			}
		}
		if (e.which & MU_BUTTON) {
			short obj = objc_find(tree, 0, 8, e.mx, e.my), i, k = -1;
			for (i = 0; i < npw; i++)
				if (eye_obj[i] == obj && obj > 0)
					k = i;
			if (k >= 0) {
				/* show or hide the password; the dialog stays */
				LFIELD *f = eye_lf[k];
				eye_blk[k].bi_pdata = f->mask ? eye_shut : eye_open;
				lf_mask(f, f->mask ? 0 : '*', &idx, edit == f->obj);
				draw_obj(eye_obj[k]);
				evnt_timer_(150);
			} else if (obj < 0) {
				next = 0;
			} else {
				cont = form_button(tree, obj, e.breturn, &next);
			}
		}
		if (!cont || (next && next != edit))
			if (edit)
				objc_edit(tree, edit, 0, &idx, ED_END);
	}
	return next;
}

/* the dialog opened (over the screen on TOS, in a window of the AES's on
   MiNT and MagiC: dialog.h), run and put away; name: its window's title */
static short d_do_named(const char *name, short edit)
{
	short x, y, w, h, r;
	dlg_name(name);
	dlg_open(tree, &x, &y, &w, &h);
	r = form_loop(edit) & 0x7fff;
	dlg_close();
	tree[r].ob_state &= ~SELECTED;
	nlf = npw = 0;
	return r;
}

static int selected(short i)
{
	return (tree[i].ob_state & SELECTED) != 0;
}

/* ---------------- account ---------------- */

int dlg_account(ACCOUNT *a)
{
	/* as long as the account's own (store.h); the fields show less */
	static char name[32], full[64], email[96], host[64], port[7], user[96], pass[64],
		    shost[64], sport[7], suser[96], spass[64];
	short f_name, f_full, f_email, f_imap, f_pop, f_host, f_port, f_user, f_pass, f_leave,
	      f_shost, f_sport, f_suser, f_spass, b_ok, b_del, r;

	str_copy(name, a->name, sizeof(name));
	str_copy(full, a->fullname, sizeof(full));
	str_copy(email, a->email, sizeof(email));
	if (opt.falcon && !a->dhost[0])
		acct_preset(a);
	str_copy(host, opt.falcon ? a->dhost : a->host, sizeof(host));
	snprintf(port, sizeof(port), "%u", opt.falcon ? acct_port(a, 0) : a->port);
	/* each mode has its own login: a Direct connection's goes to the provider,
	   the gateway's to the Pi (which may want something else) */
	acct_fill_logins(a);
	str_copy(user, opt.falcon ? a->duser : a->user, sizeof(user));
	str_copy(pass, opt.falcon ? a->dpass : a->pass, sizeof(pass));
	str_copy(shost, opt.falcon ? a->dsmtphost : a->smtphost, sizeof(shost));
	snprintf(sport, sizeof(sport), "%u", opt.falcon ? acct_port(a, 1) : a->smtpport);
	str_copy(suser, opt.falcon ? a->dsmtpuser : a->smtpuser, sizeof(suser));
	str_copy(spass, opt.falcon ? a->dsmtppass : a->smtppass, sizeof(spass));

	d_begin(62, 21);
	d_add(G_STRING, 0, 0, (long)"Mail account", 2, 1, 12, 1);
	d_text(16, 1, opt.falcon ? "Direct connection: the provider's servers"
				 : "the Pi gateway's servers and login");
	d_text(2, 3, "Account name:");
	f_name = d_long(17, 3, name, 30, sizeof(name) - 1, 0);
	d_text(2, 4, "Your name:");
	f_full = d_long(17, 4, full, 30, sizeof(full) - 1, 0);
	d_text(2, 5, "E-mail:");
	f_email = d_long(17, 5, email, 42, sizeof(email) - 1, 0);

	d_text(2, 7, "Incoming mail:");
	f_imap = d_add(G_BUTTON, SELECTABLE | RBUTTON, a->pop ? 0 : SELECTED, (long)"IMAP", 17, 7, 8, 1);
	f_pop = d_add(G_BUTTON, SELECTABLE | RBUTTON, a->pop ? SELECTED : 0, (long)"POP3", 27, 7, 8, 1);
	d_text(2, 8, "Server:");
	f_host = d_long(17, 8, host, 30, sizeof(host) - 1, 0);
	d_text(49, 8, "Port:");
	f_port = d_edit(55, 8, port, 5, '9');
	d_text(2, 9, "User:");
	f_user = d_long(17, 9, user, 42, sizeof(user) - 1, 0);
	d_text(2, 10, "Password:");
	f_pass = d_long(17, 10, pass, 30, sizeof(pass) - 1, '*');
	/* 3 pixels down: the 3D outset (5.6) clear of the password above */
	f_leave = d_check(17, (short)(11 | (3 << 8)), "POP3: leave mail on the server", a->leave);

	d_text(2, 13, "Outgoing (SMTP)");
	d_text(2, 14, "Server:");
	f_shost = d_long(17, 14, shost, 30, sizeof(shost) - 1, 0);
	d_text(49, 14, "Port:");
	f_sport = d_edit(55, 14, sport, 5, '9');
	d_text(2, 15, "User:");
	f_suser = d_long(17, 15, suser, 42, sizeof(suser) - 1, 0);
	d_text(2, 16, "Password:");
	f_spass = d_long(17, 16, spass, 30, sizeof(spass) - 1, '*');
	d_text(17, 17, "(empty: as incoming, \"-\": no login)");

	b_del = d_button(2, 19, 10, "Delete", EXIT);
	d_button(38, 19, 10, "Cancel", EXIT);
	b_ok = d_button(50, 19, 10, "OK", EXIT | DEFAULT);
	d_end();
	(void)f_name; (void)f_full; (void)f_email; (void)f_host; (void)f_port; (void)f_user;
	(void)f_pass; (void)f_shost; (void)f_sport; (void)f_suser; (void)f_spass;
	(void)f_imap;

	r = d_do_named("Account", f_name);
	if (r == b_del) {
		if (alert(2, "[2][Delete the account|\"%s\"?|(Its mail stays on the server.)][Delete|Cancel]", a->name) == 1)
			return -1;
		return 0;
	}
	if (r != b_ok)
		return 0;
	str_copy(a->name, name[0] ? name : "Mail", sizeof(a->name));
	str_copy(a->fullname, full, sizeof(a->fullname));
	str_copy(a->email, email, sizeof(a->email));
	a->pop = selected(f_pop);
	if (opt.falcon) {
		str_copy(a->dhost, host, sizeof(a->dhost));
		a->dport = (unsigned short)atoi(port);
		if (!a->dport)
			a->dport = a->pop ? 995 : 993;
		acct_preset_kind(a);	/* IMAP's server for POP3, or the other way */
	} else {
		str_copy(a->host, host, sizeof(a->host));
		a->port = (unsigned short)atoi(port);
		if (!a->port)
			a->port = a->pop ? 110 : 143;
	}
	if (opt.falcon) {
		str_copy(a->duser, user, sizeof(a->duser));
		str_copy(a->dpass, pass, sizeof(a->dpass));
	} else {
		str_copy(a->user, user, sizeof(a->user));
		str_copy(a->pass, pass, sizeof(a->pass));
	}
	a->leave = selected(f_leave);
	if (opt.falcon) {
		str_copy(a->dsmtphost, shost, sizeof(a->dsmtphost));
		a->dsmtpport = (unsigned short)atoi(sport);
		if (!a->dsmtpport)
			a->dsmtpport = 465;
		/* a new account: the provider's servers from the address */
		if (!a->dhost[0])
			acct_preset(a);
	} else {
		str_copy(a->smtphost, shost, sizeof(a->smtphost));
		a->smtpport = (unsigned short)atoi(sport);
		if (!a->smtpport)
			a->smtpport = 587;
	}
	if (opt.falcon) {
		str_copy(a->dsmtpuser, suser, sizeof(a->dsmtpuser));
		str_copy(a->dsmtppass, spass, sizeof(a->dsmtppass));
	} else {
		str_copy(a->smtpuser, suser, sizeof(a->smtpuser));
		str_copy(a->smtppass, spass, sizeof(a->smtppass));
	}
	acct_fill_logins(a);		/* a new account: the other mode starts the same */
	return 1;
}

/* ---------------- settings ---------------- */

/* ---------------- settings ----------------
 * A black title band, a list of parts on the left (the chosen one in a
 * black bar) and that part's settings on the right: ticks (a box that is
 * crossed when on, clicked on the box or its words) and framed fields. */

static char s_zone[40], s_zlabel[42], s_check[4], s_page[5], s_wrap[3];
#define SIG_ROWS 4
#define SIG_W 40			/* a row shows */
#define SIG_CAP 78			/* and holds: a line of mail */
static char s_sig[SIG_ROWS][SIG_CAP + 1];	/* the signature, a line a row */
#define PDIR_W 32
static char s_pdir[sizeof(opt.peopledir)];	/* EContact's folder */
static short s_on[8];			/* keep, log, hebrew, falcon, dsp, colour, people file, start */
enum { T_KEEP, T_LOG, T_HEB, T_FALCON, T_DSP, T_COLOUR, T_PFILE, T_START };

#define NTICK 8
static short tick_box[NTICK], tick_lbl[NTICK], tick_var[NTICK], nticks;
static short page_of[DMAX], npages, cur_page, page_tab[7];
static short pages_obj;		/* the box the pages show in */

static TEDINFO *d_ted(const char *text, short just, short color)
{
	TEDINFO *t = &ted[nted++];
	t->te_ptext = (char *)text;
	t->te_ptmplt = "";
	t->te_pvalid = "";
	t->te_font = 3;
	t->te_just = just;
	t->te_color = color;
	t->te_thickness = 0;
	t->te_txtlen = (short)strlen(text) + 1;
	t->te_tmplen = 1;
	return t;
}

/* black band across the top, white title */
static short band_obj;			/* the open dialog's band, 0 none */

static void d_band(short w, const char *title)
{
	band_obj = d_add(G_BOXTEXT, 0, 0, (long)d_ted(title, 0, 0x1071), 0, 0, w, 1);
}

static short d_tick(short x, short y, const char *label, short var)
{
	short k = nticks++;
	tick_var[k] = var;
	if (!tick_on_blk.bi_pdata)
		make_ticks();
	tick_box[k] = d_add(G_IMAGE, TOUCHEXIT, 0, (long)(s_on[var] ? &tick_on_blk : &tick_off_blk), x, y, 2, 1);
	tick_lbl[k] = d_add(G_STRING, TOUCHEXIT, 0, (long)label, x + 3, y, (short)strlen(label), 1);
	return k;
}

static void tick_draw(short k)
{
	tree[tick_box[k]].ob_spec = (long)(s_on[tick_var[k]] ? &tick_on_blk : &tick_off_blk);
	dlg_draw(tree, tick_box[k]);
}

/* field i in a thin frame */
static short d_framed(short i)
{
	TEDINFO *t = (TEDINFO *)tree[i].ob_spec;
	tree[i].ob_type = G_FBOXTEXT;
	t->te_thickness = -1;
	t->te_color = 0x1180;
	return i;
}

/* a number field in a thin frame */
static short d_field(short x, short y, char *buf, short len, char kind)
{
	return d_framed(d_edit(x, y, buf, len, kind));
}

/* rows of a part: y in characters plus a few pixels per framed field
   above, so the frames don't touch */
/* rows of a part, with some air between them: row n of a part starting
   at line y is n lines and n gaps down (6 pixels with the 16-pixel font) */
static short row_y0, row_gap;
#define ROW(n) ((short)((row_y0 + (n)) | ((2 + (n) * row_gap) << 8)))

static void rows_at(short y)
{
	row_y0 = y;
	row_gap = (short)(gl_hchar * 3 / 8);
}

/* objects added from here on belong to page p (0: always shown) */
static short adding_page;

static void page_mark(short from)
{
	short i;
	for (i = from; i < nobj; i++)
		page_of[i] = adding_page;
}

static void show_page(short p, int draw)
{
	short i;
	cur_page = p;
	for (i = 1; i < nobj; i++) {
		if (!page_of[i])
			continue;
		if (page_of[i] == p) {
			tree[i].ob_flags &= ~HIDETREE;
			if (tree[i].ob_type == G_FTEXT || tree[i].ob_type == G_FBOXTEXT)
				tree[i].ob_flags |= EDITABLE;
		} else {
			tree[i].ob_flags |= HIDETREE;
			tree[i].ob_flags &= ~EDITABLE;
		}
	}
	/* the chosen page in a black bar, white words (5.6: not SELECTED,
	   which the 3D look draws as pressed in) */
	for (i = 1; i <= npages; i++) {
		tree[page_tab[i]].ob_state = 0;
		((TEDINFO *)tree[page_tab[i]].ob_spec)->te_color = i == p ? 0x1071 : 0x1180;
	}
	if (draw) {
		short k;
		for (k = 1; k <= npages; k++)
			dlg_draw(tree, page_tab[k]);
		if (pages_obj)
			dlg_draw(tree, pages_obj);
	}
}

static short first_edit(void)
{
	short i;
	for (i = 1; i < nobj; i++)
		if ((tree[i].ob_flags & EDITABLE) && !(tree[i].ob_flags & HIDETREE))
			return i;
	return 0;
}

/* the zone button: its name, the offset now, and a down arrow */
static short zone_btn;

static void zone_label(void)
{
	const TZONE *z = tz_find(s_zone);
	char off[16];
	short n, o;
	if (!z)
		z = tz_utc();
	tz_label(z, zone_utc_now(), off, sizeof(off));
	memset(s_zlabel, ' ', 28);
	s_zlabel[28] = 0;
	n = (short)strlen(z->name);
	o = (short)strlen(off);
	if (n > 25 - o)
		n = (short)(25 - o);
	memcpy(s_zlabel + 1, z->name, n);
	memcpy(s_zlabel + 26 - o, off, o);
	s_zlabel[27] = 2;		/* the system font's down arrow */
}

static short pick_w(const char *title, const char **items, short n, short lw);
static short pick_from;		/* the row a picker starts on */
/* the settings dialog, kept aside while a picker builds in the one tree */
typedef struct {
	OBJECT tree[DMAX];
	TEDINFO ted[TMAX];
	short page[DMAX], n[5];
} KEEP;
static GRECT pick_area;		/* where the last picker was */

/* every zone with its offset now; the one chosen goes into s_zone */
static void zone_pick(void)
{
	/* allocated while the picker is up: about 7 KB a small Atari needs
	   the rest of the time */
	struct {
		char lab[120][40];
		const char *items[120];
		KEEP keep;
	} *z = malloc(sizeof(*z));
	long now = zone_utc_now();
	short n = (short)tz_count(), i, sel = 0;
	if (!z) {
		alert(1, "[1][Not enough memory for|the list of time zones.][ OK ]");
		return;
	}
	if (n > 120)
		n = 120;
	for (i = 0; i < n; i++) {
		const TZONE *t = tz_index(i);
		char off[16];
		tz_label(t, now, off, sizeof(off));
		snprintf(z->lab[i], sizeof(z->lab[i]), "%-28s %s", t->name, off);
		z->items[i] = z->lab[i];
		if (!strcmp(t->name, s_zone))
			sel = i;
	}
	/* the picker builds in the one tree: keep the settings aside */
	memcpy(z->keep.tree, tree, sizeof(z->keep.tree));
	memcpy(z->keep.ted, ted, sizeof(z->keep.ted));
	memcpy(z->keep.page, page_of, sizeof(z->keep.page));
	z->keep.n[0] = nobj;
	z->keep.n[1] = nted;
	z->keep.n[2] = last_child;
	z->keep.n[3] = npw;
	z->keep.n[4] = nlf;
	pick_from = sel;
	i = pick_w("Your time zone", z->items, n, 38);
	pick_from = 0;
	memcpy(tree, z->keep.tree, sizeof(z->keep.tree));
	memcpy(ted, z->keep.ted, sizeof(z->keep.ted));
	memcpy(page_of, z->keep.page, sizeof(z->keep.page));
	nobj = z->keep.n[0];
	nted = z->keep.n[1];
	last_child = z->keep.n[2];
	npw = z->keep.n[3];
	nlf = z->keep.n[4];
	free(z);
	if (i >= 0)
		str_copy(s_zone, tz_index(i)->name, sizeof(s_zone));
	zone_label();
}

/* EContact's folder: the folder of a file chosen in it (CONTACTS.TXT) */
static short pdir_btn = -1, pdir_field;

static void pdir_pick(void)
{
	char path[200], name[16] = "CONTACTS.TXT", *e;
	short button = 0;
	if (s_pdir[0])
		path_join(path, sizeof(path), s_pdir, "*.*");
	else
		str_copy(path, "C:\\*.*", sizeof(path));
	if (!fsel_exinput(path, name, &button, "Where is CONTACTS.TXT?") || !button)
		return;
	e = strrchr(path, '\\');
	if (e)
		*e = 0;
	str_copy(s_pdir, path, sizeof(s_pdir));
	if (lf_at(pdir_field))
		lf_end(lf_at(pdir_field));	/* the field shows the new one */
}

/* the dialog stays open while ticks and pages change */
static short settings_run(short pages_box)
{
	short x, y, w, h, r, k;
	pages_obj = pages_box;
	dlg_name("Settings");
	if (band_obj)
		dlg_band(band_obj);	/* in a window its title says it (3.5) */
	dlg_open(tree, &x, &y, &w, &h);
	for (;;) {
		r = form_loop(first_edit()) & 0x7fff;
		for (k = 0; k < nticks; k++)
			if (r == tick_box[k] || r == tick_lbl[k]) {
				if (tree[tick_box[k]].ob_state & DISABLED)
					break;
				s_on[tick_var[k]] = !s_on[tick_var[k]];
				tick_draw(k);
				evnt_timer_(120);
				break;
			}
		if (k < nticks)
			continue;
		for (k = 1; k <= npages; k++)
			if (r == page_tab[k])
				break;
		if (k <= npages) {
			if (k != cur_page)
				show_page(k, 1);
			continue;
		}
		if (r == pdir_btn) {
			/* EContact's folder in the file selector, then the
			   settings again over what it covered */
			tree[r].ob_state &= ~SELECTED;
			pdir_pick();
			ui_redraws();
			dlg_draw(tree, 0);
			continue;
		}
		if (r == zone_btn) {
			/* the zone list over the settings, then the settings again */
			tree[r].ob_state &= ~SELECTED;
			zone_pick();
			if (!dlg_in_window()) {
				/* on TOS what the list covered, drawn again (in a
				   window the AES asks for it) */
				win_redraw_area(&pick_area);
				dlg_draw(tree, 0);
			}
			continue;
		}
		break;
	}
	dlg_close();
	pages_obj = band_obj = 0;
	tree[r].ob_state &= ~SELECTED;
	return r;
}

static void settings_begin(short w, short h)
{
	d_begin(w, h);
	nticks = 0;
	npages = 0;
	adding_page = 0;
	memset(page_of, 0, sizeof(page_of));
}

/* the values the dialog edits, from the options */
static void settings_load(void)
{
	str_copy(s_zone, opt.tzname, sizeof(s_zone));
	snprintf(s_check, sizeof(s_check), "%d", opt.check);
	snprintf(s_page, sizeof(s_page), "%d", opt.page);
	snprintf(s_wrap, sizeof(s_wrap), "%d", opt.wrap);
	s_on[T_KEEP] = opt.keepcache;
	s_on[T_START] = opt.startoffline;
	s_on[T_LOG] = opt.log;
	s_on[T_HEB] = opt.hebrew;
	s_on[T_FALCON] = dlg_falcon;
	s_on[T_DSP] = opt.dsp;
	s_on[T_COLOUR] = opt.colour;
	s_on[T_PFILE] = opt.peoplefile;
	str_copy(s_pdir, opt.peopledir, sizeof(s_pdir));
	{
		const char *p = opt.signature;
		short k;
		for (k = 0; k < SIG_ROWS; k++) {
			const char *nl = strchr(p, '\n');
			long n = nl ? nl - p : (long)strlen(p);
			if (n > SIG_CAP)
				n = SIG_CAP;
			memcpy(s_sig[k], p, n);
			s_sig[k][n] = 0;
			p = nl ? nl + 1 : p + strlen(p);
		}
	}
}

/* the rows back into one signature, without the empty rows at its end */
static void sig_store(void)
{
	short k, last = -1;
	opt.signature[0] = 0;
	for (k = 0; k < SIG_ROWS; k++)
		if (s_sig[k][0])
			last = k;
	for (k = 0; k <= last; k++) {
		size_t n = strlen(opt.signature);
		if (k && n + 1 < sizeof(opt.signature))
			opt.signature[n++] = '\n';
		str_copy(opt.signature + n, s_sig[k], sizeof(opt.signature) - n);
	}
}

/* the settings of each part, at (x, y); fields in one column. Each
   returns its height: characters, plus pixels in the high byte */
#define FX 30

static void part_mail(short x, short y)
{
	rows_at(y);
	d_add(G_STRING, 0, 0, (long)"Check for new mail every", x, ROW(0), 24, 1);
	d_field(x + FX, ROW(0), s_check, 3, '9');
	d_add(G_STRING, 0, 0, (long)"min", x + FX + 4, ROW(0), 3, 1);
	d_add(G_STRING, 0, 0, (long)"Messages to load at a time", x, ROW(1), 26, 1);
	d_field(x + FX, ROW(1), s_page, 4, '9');
	d_tick(x, ROW(2), "Keep read messages on disk", T_KEEP);
	d_tick(x, ROW(3), "Start offline, with the mail on disk", T_START);
}

static void part_writing(short x, short y)
{
	rows_at(y);
	d_add(G_STRING, 0, 0, (long)"Wrap my lines at column", x, ROW(0), 23, 1);
	d_field(x + FX, ROW(0), s_wrap, 2, '9');
	d_tick(x, ROW(1), "Start with the Hebrew keyboard", T_HEB);
}

/* four rows in one frame, as one box to write in */
static void part_signature(short x, short y)
{
	short k;
	rows_at(y);
	d_add(G_BOX, 0, 0, 0x00FF1100L, x, ROW(0), SIG_W, SIG_ROWS);
	for (k = 0; k < SIG_ROWS; k++)
		d_long(x, (short)((y + k) | (2 << 8)), s_sig[k], SIG_W, SIG_CAP, 0);
	d_add(G_STRING, 0, 0, (long)"Added to new messages, replies and", x, (short)(y + SIG_ROWS + 1), 34, 1);
	d_add(G_STRING, 0, 0, (long)"forwards from every account.", x, (short)(y + SIG_ROWS + 2), 28, 1);
}

static void part_look(short x, short y)
{
	rows_at(y);
	d_tick(x, ROW(0), "Use colours", T_COLOUR);
	d_add(G_STRING, 0, 0, (long)"on screens with 16 colours or more:", x + 3, ROW(1), 35, 1);
	d_add(G_STRING, 0, 0, (long)"selection, unread counts, flags,", x + 3, ROW(2), 32, 1);
	d_add(G_STRING, 0, 0, (long)"quoted text, links; with four", x + 3, ROW(3), 29, 1);
	d_add(G_STRING, 0, 0, (long)"colours, links in blue", x + 3, ROW(4), 22, 1);
}

/* EContact's contacts file, when EContact isn't loaded (the guidelines,
   4.1; an option for a machine short of memory, Erez, 21 Oct 2026) */
static void part_contacts(short x, short y)
{
	rows_at(y);
	d_tick(x, ROW(0), "When EContact isn't loaded, read", T_PFILE);
	d_add(G_STRING, 0, 0, (long)"its contacts file (CONTACTS.TXT)", x + 3, ROW(1), 32, 1);
	d_add(G_STRING, 0, 0, (long)"EContact's folder:", x, ROW(3), 18, 1);
	pdir_field = d_framed(d_long(x, ROW(4), s_pdir, PDIR_W, sizeof(s_pdir) - 1, 0));
	pdir_btn = d_button(x + PDIR_W + 2, ROW(4), 5, "...", TOUCHEXIT);	/* to column 39 */
	d_add(G_STRING, 0, 0, (long)"With EContact loaded, EMail asks it.", x, ROW(6), 36, 1);
}

static void part_connection(short x, short y)
{
	rows_at(y);
	d_add(G_STRING, 0, 0, (long)"Time zone", x, ROW(0), 9, 1);
	zone_label();
	zone_btn = d_add(G_BOXTEXT, TOUCHEXIT, 0, (long)d_ted(s_zlabel, 0, 0x1180), x + 11, ROW(0), 29, 1);
	((TEDINFO *)tree[zone_btn].ob_spec)->te_thickness = -1;
	d_tick(x, ROW(1), "Direct connection: TLS on this Atari", T_FALCON);
	d_add(G_STRING, 0, 0, (long)"(68020 or better); off: the Pi gateway", x + 3, ROW(2), 38, 1);
	d_tick(x + 3, ROW(3), "Use the DSP for the signatures", T_DSP);
	d_tick(x, ROW(4), "Write a protocol log (EMAIL.LOG)", T_LOG);
}

#define SW 13			/* the list's width */
#define SX (SW + 2)		/* where the settings start */

static short settings_sidebar(short *ok)
{
	static const char *names[] = { "  Mail", "  Writing", "  Signature", "  Look", "  Contacts", "  Connection" };
	short k, box, from, gap = (short)(gl_hchar / 2);
	settings_begin(SX + 42, 17);		/* six parts in the list */
	d_band(SX + 42, " Settings");
	/* the list: plain words on white, the chosen one in a black bar, a
	   line between it and the settings */
	d_add(G_BOX, 0, 0, 0x00001171L, SW, 1, (short)(0 | (1 << 8)), 16);
	for (k = 0; k < 6; k++) {
		npages++;
		page_tab[npages] = d_add(G_BOXTEXT, TOUCHEXIT, 0, (long)d_ted(names[k], 0, 0x1180),
					 (short)(0 | (2 << 8)), (short)((2 + k * 2) | ((k * gap) << 8)),
					 (short)((SW - 1) | (6 << 8)), 1);
	}
	box = d_add(G_IBOX, 0, 0, 0, SX - 1, 2, 42, 12);
	for (k = 1; k <= 6; k++) {
		from = nobj;
		adding_page = k;
		d_add(G_STRING, 0, 0, (long)(names[k - 1] + 2), SX, 2, (short)strlen(names[k - 1] + 2), 1);
		d_add(G_BOX, 0, 0, 0x00001171L, SX, (short)(3 | (2 << 8)), 40, (short)(0 | (1 << 8)));
		if (k == 1)
			part_mail(SX, 4);
		else if (k == 2)
			part_writing(SX, 4);
		else if (k == 3)
			part_signature(SX, 4);
		else if (k == 4)
			part_look(SX, 4);
		else if (k == 5)
			part_contacts(SX, 4);
		else
			part_connection(SX, 4);
		page_mark(from);
	}
	adding_page = 0;
	d_button(SX + 18, 15, 10, "Cancel", EXIT);
	*ok = d_button(SX + 30, 15, 10, "OK", EXIT | DEFAULT);
	d_end();
	show_page(1, 0);
	return box;
}

int dlg_settings(void)
{
	short ok, box, r;
	settings_load();
	box = settings_sidebar(&ok);
	r = settings_run(box);
	if (r != ok)
		return 0;
	str_copy(opt.tzname, s_zone, sizeof(opt.tzname));
	opt.check = (short)atoi(s_check);
	opt.page = (short)atoi(s_page);
	if (opt.page < 20)
		opt.page = 20;
	if (opt.page > 1000)
		opt.page = 1000;
	opt.wrap = (short)atoi(s_wrap);
	if (opt.wrap < 40 || opt.wrap > 78)
		opt.wrap = 72;
	opt.keepcache = s_on[T_KEEP];
	opt.startoffline = s_on[T_START];
	opt.log = s_on[T_LOG];
	opt.hebrew = s_on[T_HEB];
	dlg_falcon = s_on[T_FALCON];
	opt.dsp = s_on[T_DSP];
	opt.colour = s_on[T_COLOUR];
	opt.peoplefile = s_on[T_PFILE];
	str_copy(opt.peopledir, s_pdir, sizeof(opt.peopledir));
	sig_store();
	return 1;
}

/* ---------------- a line of text ---------------- */

int dlg_ask(const char *title, const char *label, char *buf, short cap)
{
	static char field[200];
	short f, b_ok, len = cap;
	if (cap > (short)sizeof(field) - 1)
		cap = (short)sizeof(field) - 1;
	if (len > 46)
		len = 46;		/* it shows that much, and holds cap */
	str_copy(field, buf, (size_t)cap + 1);
	d_begin(len + 6 > 40 ? len + 6 : 40, 8);
	d_add(G_STRING, 0, 0, (long)title, 2, 1, (short)strlen(title), 1);
	d_text(2, 3, label);
	f = d_long(2, 4, field, len, cap, 0);
	d_button((len + 6 > 40 ? len + 6 : 40) - 24, 6, 10, "Cancel", EXIT);
	b_ok = d_button((len + 6 > 40 ? len + 6 : 40) - 12, 6, 10, "OK", EXIT | DEFAULT);
	d_end();
	if (d_do_named(title, f) != b_ok || !field[0])
		return 0;
	str_copy(buf, field, (size_t)cap + 1);
	return 1;
}

/* ---------------- pickers ---------------- */

#define PROWS 12			/* rows shown at a time */
#define PW 38				/* the widest list, in characters */

/* choose one of n strings (Atari charset); -1 = cancelled. One dialog
   that stays open: a framed list with arrows to page through it (the
   list is redrawn in place), a click picks a row, OK or a double click
   takes it. */
static short pick_w(const char *title, const char **items, short n, short lw)
{
	static char shown[PROWS][PW + 2], count[40];
	short first = 0, cur = -1, row[PROWS], box, b_up, b_down, b_ok, b_cancel, f_count;
	short x, y, w, h, r, i, result = -1, tw = (short)strlen(title);

	if (pick_from > 0 && pick_from < n) {
		cur = pick_from;
		first = cur - cur % PROWS;
	}
	if (lw > PW)
		lw = PW;
	if (lw < 22)
		lw = 22;
	if (tw > lw + 2)
		lw = tw > PW ? PW : tw - 2;
	d_begin(lw + 8, PROWS + 8);
	d_add(G_STRING, 0, 0, (long)title, 2, 1, (short)strlen(title), 1);
	box = d_add(G_BOX, 0, 0, 0x00FF1100L, 2, 3, lw, PROWS);
	for (i = 0; i < PROWS; i++)
		row[i] = d_add(G_STRING, TOUCHEXIT, 0, (long)shown[i], 2, 3 + i, lw, 1);
	b_up = d_add(G_BOXCHAR, TOUCHEXIT, 0, 0x01FF1100L, lw + 3, 3, 2, 1);
	b_down = d_add(G_BOXCHAR, TOUCHEXIT, 0, 0x02FF1100L, lw + 3, 2 + PROWS, 2, 1);
	f_count = d_add(G_STRING, 0, 0, (long)count, 2, PROWS + 4, lw, 1);
	b_cancel = d_button(lw - 18, PROWS + 6, 10, "Cancel", EXIT);
	b_ok = d_button(lw - 6, PROWS + 6, 10, "OK", EXIT | DEFAULT);
	d_end();

	for (;;) {
		/* fill the rows from `first`; the chosen one inverted */
		short k;
		for (k = 0; k < PROWS; k++) {
			short it = first + k, len = 0;
			memset(shown[k], ' ', lw);
			shown[k][lw] = 0;
			tree[row[k]].ob_state = 0;
			if (it < n) {
				const char *s = items[it];
				len = (short)strlen(s);
				if (len > lw - 2)
					len = lw - 2;
				/* GEM draws in storage order: lay Hebrew out first */
				if (bidi_has_rtl(s, len))
					bidi_visual(s, len, bidi_is_rtl(s, len), shown[k] + 1);
				else
					memcpy(shown[k] + 1, s, len);
				heb_font_map(shown[k] + 1, len, opt.hebfont);
				if (it == cur)
					tree[row[k]].ob_state = SELECTED;
			}
		}
		if (n > PROWS)
			snprintf(count, sizeof(count), "%d-%d of %d", first + 1,
				 first + PROWS < n ? first + PROWS : n, n);
		else
			snprintf(count, sizeof(count), n == 1 ? "1 to choose from" : "%d to choose from", n);
		tree[b_up].ob_state = first > 0 ? 0 : DISABLED;
		tree[b_down].ob_state = first + PROWS < n ? 0 : DISABLED;
		tree[b_ok].ob_state = cur >= 0 ? 0 : DISABLED;
		if (result == -1) {
			/* first time round: open the dialog */
			dlg_name(title);
			dlg_open(tree, &x, &y, &w, &h);
			result = -2;
		} else {
			/* only the list, the arrows, the count and OK change */
			dlg_draw(tree, box);
			dlg_draw(tree, b_up);
			dlg_draw(tree, b_down);
			dlg_draw(tree, f_count);
			dlg_draw(tree, b_ok);
		}
		r = dlg_do(tree, 0, b_cancel);	/* Esc and Undo: Cancel (3.5) */
		{
			int dbl = (r & 0x8000) != 0;
			r &= 0x7fff;
			tree[r].ob_state &= ~SELECTED;
			if (r == b_cancel)
				break;
			if (r == b_ok) {
				if (cur >= 0) {
					result = cur;
					break;
				}
				continue;
			}
			if (r == b_up && first > 0) {
				first -= PROWS;
				if (first < 0)
					first = 0;
				evnt_timer_(120);	/* held down: a page at a time */
				continue;
			}
			if (r == b_down && first + PROWS < n) {
				first += PROWS;
				evnt_timer_(120);
				continue;
			}
			for (k = 0; k < PROWS; k++)
				if (r == row[k] && first + k < n) {
					cur = first + k;
					if (dbl)
						result = cur;
				}
			if (result >= 0)
				break;
		}
	}
	dlg_area(&x, &y, &w, &h);	/* where it is now: it may have been moved */
	dlg_close();
	pick_area.x = x;
	pick_area.y = y;
	pick_area.w = w;
	pick_area.h = h;
	return result >= 0 ? result : -1;
}

static short pick(const char *title, const char **items, short n)
{
	return pick_w(title, items, n, PW);
}

short dlg_pick_list(const char *title, const char **items, short n);
short dlg_pick_list(const char *title, const char **items, short n)
{
	return pick(title, items, n);
}

/* a folder's name with its parents', "Archive / 2023", so two folders
   of the same name can be told apart */
static void folder_label(ACCOUNT *a, FINFO *f, char *out, int size)
{
	FINFO *parent = 0;
	short i;
	size_t best = 0;
	if (f->depth > 0) {
		for (i = 0; i < a->nfolders; i++) {
			FINFO *g = &a->folders[i];
			size_t l = strlen(g->server);
			if (g != f && g->depth == f->depth - 1 && l > best && !strncmp(g->server, f->server, l) &&
			    f->server[l] && !((f->server[l] | 0x20) >= 'a' && (f->server[l] | 0x20) <= 'z')) {
				parent = g;
				best = l;
			}
		}
	}
	if (parent) {
		char up[120];
		folder_label(a, parent, up, sizeof(up));
		snprintf(out, size, "%s / %s", up, f->disp);
	} else {
		str_copy(out, f->disp, size);
	}
}

static int cmp_label(const void *x, const void *y)
{
	const char *a = *(const char * const *)x, *b = *(const char * const *)y;
	return strcasecmp(a, b);
}

FINFO *dlg_pick_folder(ACCOUNT *a, const char *title)
{
	/* allocated while the picker is up (6 KB) */
	char (*labels)[100] = malloc(MAXFOLDER * sizeof(*labels));
	const char *items[MAXFOLDER];
	FINFO *map[MAXFOLDER];
	short i, n = 0, r;
	if (!labels) {
		alert(1, "[1][Not enough memory for|the list of folders.][ OK ]");
		return 0;
	}
	for (i = 0; i < a->nfolders; i++) {
		FINFO *f = &a->folders[i];
		if (f->noselect || f->role == FR_OUTBOX || f == cur_finfo)
			continue;
		if (cur_finfo && f->local != cur_finfo->local)
			continue;
		folder_label(a, f, labels[n], sizeof(labels[n]));
		items[n] = labels[n];
		n++;
	}
	if (!n) {
		free(labels);
		alert(1, "[1][There is no other folder|to choose.][ OK ]");
		return 0;
	}
	/* alphabetical; the labels sit in a fixed array, so find each one's
	   folder again after sorting */
	qsort(items, n, sizeof(items[0]), cmp_label);
	for (r = 0; r < n; r++) {
		short k = 0;
		for (i = 0; i < a->nfolders; i++) {
			FINFO *f = &a->folders[i];
			if (f->noselect || f->role == FR_OUTBOX || f == cur_finfo ||
			    (cur_finfo && f->local != cur_finfo->local))
				continue;
			if (labels[k] == items[r])
				map[r] = f;
			k++;
		}
	}
	r = pick_w(title, items, n, 25);	/* folder names are short */
	free(labels);
	return r < 0 ? 0 : map[r];
}

ACCOUNT *dlg_pick_account(const char *title)
{
	const char *items[MAXACCT];
	short i, r;
	if (naccts == 1)
		return accts[0];
	for (i = 0; i < naccts; i++)
		items[i] = accts[i]->name;
	r = pick(title, items, naccts);
	return r < 0 ? 0 : accts[r];
}

/* the address book: returns a malloc'ed "Name <a@b>" or NULL */
char *dlg_pick_address(void);
char *dlg_pick_address(void)
{
	long len;
	char *book = abook_load(&len), *p, *res = 0;
	const char *items[200];
	short n = 0, r;
	if (!book || !len) {
		free(book);
		alert(1, "[1][The address book is empty.|Addresses you write to are|added to it.][ OK ]");
		return 0;
	}
	for (p = book; *p && n < 200; ) {
		char *e = strchr(p, '\n');
		if (e)
			*e = 0;
		if (*p && p[strlen(p) - 1] == '\r')
			p[strlen(p) - 1] = 0;
		if (*p)
			items[n++] = p;
		if (!e)
			break;
		p = e + 1;
	}
	r = pick("Address book", items, n);
	if (r >= 0)
		res = strdup(items[r]);
	free(book);
	return res;
}

/* ---------------- the keys (Help) ---------------- */

void dlg_keys(void)
{
	static const char *lines[] = {
		"^N  new message        \x01^N check mail (^K)",
		"^R  reply              \x01^R reply to all",
		"\x01^F forward            ^U  mark as unread",
		"^G  flag / unflag      ^M  move to a folder",
		"Del delete             ^A  select all",
		"^C  copy   ^X  cut     ^V  paste",
		"Shift+click   add or remove one message",
		"Control+click select a range of messages",
		"Tab next pane          F10 Hebrew keyboard",
		"In the editor: \x01^D send  ^S into the Outbox",
		"               ^T attach  ^B addresses",
		"^,  settings           Help this guide",
		"^W  close the window   ^Q  quit"
	};
	short i, n = (short)(sizeof(lines) / sizeof(lines[0]));
	d_begin(52, n + 6);
	d_add(G_STRING, 0, 0, (long)"Keys", 2, 1, 4, 1);
	for (i = 0; i < n; i++)
		d_text(3, 3 + i, lines[i]);
	d_button(40, n + 4, 10, "OK", EXIT | DEFAULT);
	d_end();
	d_do_named("Keys", 0);
}
