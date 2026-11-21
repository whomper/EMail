/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * printmsg.c - File > Print... (^P), and Print... on a message's menu
 * (Erez, 21 Nov 2026): the messages a command works on, the selected
 * ones or the one shown, each from a new page, in one job through the
 * GDOS (common/ui/print.c). A page carries From, To, Cc, Date and
 * Subject with their labels in bold, the attachments by name and size
 * (not the attachments themselves), a rule, then the text wrapped to
 * the paper; "Page n of m" at its foot, counted for each message.
 *
 * The page is a grid of P_COLS columns by P_ROWS rows inside the
 * margins, the same on every printer, so the pages are counted before
 * the printer dialog asks which of them to print; the system font is
 * sized to the grid on the printer. A message not on the Atari is
 * downloaded first; Esc or Undo stops between pages. The words are the
 * family's, EPDF's (Erez, 21 Nov 2026).
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui.h"
#include "../src/mail.h"
#include "util.h"
#include "bidi.h"
#include "busy.h"
#include "conn.h"
#include "print.h"

#define P_COLS 80		/* a line of mail, and two more */
#define P_ROWS 60		/* the text's rows; the foot two below */

enum { PL_TEXT, PL_HEAD, PL_ATTACH, PL_RULE };

typedef struct {
	long at;		/* in the page text */
	short n, kind, label;	/* label: a header line's "Name:" */
} PLINE;

typedef struct {
	char *text;		/* header lines, attachment lines, the body */
	PLINE *l;
	long n;
} PDOC;

static void pdoc_free(PDOC *d)
{
	free(d->text);
	free(d->l);
	d->text = 0;
	d->l = 0;
	d->n = 0;
}

static int add_pl(PDOC *d, long *cap, long at, long n, short kind, short label)
{
	if (d->n == *cap) {
		PLINE *more = realloc(d->l, (*cap + 64) * sizeof(PLINE));
		if (!more)
			return 0;
		d->l = more;
		*cap += 64;
	}
	d->l[d->n].at = at;
	d->l[d->n].n = (short)(n > P_COLS ? P_COLS : n);
	d->l[d->n].kind = kind;
	d->l[d->n].label = label;
	d->n++;
	return 1;
}

/* the text of [s, s+n) wrapped to the page, each line kind */
static int wrap_into(PDOC *d, long *cap, long s, long n, short kind, short label)
{
	long nb, i, max = n / 8 + 8, *br = malloc(max * sizeof(long));
	int ok = 1;
	if (!br)
		return 0;
	nb = n ? wrap_text(d->text + s, n, P_COLS, br, max) : 0;
	if (!nb)
		ok = add_pl(d, cap, s, 0, kind, label);
	for (i = 0; ok && i < nb; i++) {
		long a = br[i], e = i + 1 < nb ? br[i + 1] : n;
		while (e > a && (d->text[s + e - 1] == '\n' || d->text[s + e - 1] == ' '))
			e--;
		ok = add_pl(d, cap, s + a, e - a, kind, i == 0 ? label : 0);
	}
	free(br);
	return ok;
}

static void head_line(SBUF *b, const char *label, const char *value)
{
	if (!value || !value[0])
		return;
	sb_adds(b, label);
	sb_adds(b, value);
	sb_addc(b, '\n');
}

/* m laid out for the page: 1, or 0 without the memory */
static int pdoc_make(PDOC *d, const MSG *m, unsigned long date)
{
	SBUF b;
	long cap = 0, hl, al, i, s;
	short k;
	memset(d, 0, sizeof(*d));
	sb_init(&b);
	head_line(&b, "From: ", m->from);
	head_line(&b, "To: ", m->to);
	head_line(&b, "Cc: ", m->cc);
	if (date) {
		char t[24];
		date_str(date, t, sizeof(t), 1);
		head_line(&b, "Date: ", t);
	}
	sb_adds(&b, "Subject: ");
	sb_adds(&b, m->subject && m->subject[0] ? m->subject : "(no subject)");
	sb_addc(&b, '\n');
	hl = b.len;
	for (k = 0; k < m->nparts; k++) {
		const MIMEPART *p = &m->parts[k];
		char sz[16];
		if (p->size < 1024)
			snprintf(sz, sizeof(sz), "%s bytes", num(p->size));
		else
			snprintf(sz, sizeof(sz), "%s KB", num((p->size + 512) / 1024));
		sb_printf(&b, "\xAF %s  (%s, %s%s)\n", p->name, p->type, sz, p->left ? ", on the server" : "");
	}
	al = b.len;
	/* the text, its tabs to the next eighth column */
	for (i = 0, s = 0; i < m->textlen; i++) {
		char c = m->text[i];
		if (c == '\n')
			s = -1;
		if (c == '\t') {
			do
				sb_addc(&b, ' ');
			while (++s % 8);
			continue;
		}
		sb_addc(&b, c == '\r' ? ' ' : c);
		s++;
	}
	if (!b.s) {
		sb_free(&b);
		return 0;
	}
	d->text = b.s;
	/* the header lines: "Label: value", the label in bold */
	for (s = 0; s < hl; ) {
		const char *e = memchr(d->text + s, '\n', hl - s), *c;
		long n = e ? e - (d->text + s) : hl - s;
		c = memchr(d->text + s, ':', n);
		if (!wrap_into(d, &cap, s, n, PL_HEAD, c ? (short)(c - (d->text + s) + 1) : 0))
			goto none;
		s += n + 1;
	}
	for (s = hl; s < al; ) {
		const char *e = memchr(d->text + s, '\n', al - s);
		long n = e ? e - (d->text + s) : al - s;
		if (!add_pl(d, &cap, s, n, PL_ATTACH, 0))
			goto none;
		s += n + 1;
	}
	if (!add_pl(d, &cap, 0, 0, PL_RULE, 0) || !wrap_into(d, &cap, al, b.len - al, PL_TEXT, 0))
		goto none;
	return 1;
none:
	pdoc_free(d);
	return 0;
}

static short pages_of(const PDOC *d)
{
	return (short)(d->n ? (d->n + P_ROWS - 1) / P_ROWS : 1);
}

/* the message with this UID, downloaded if it isn't on the Atari, laid
   out: 1, or 0 with mail_err (or nothing: memory) */
static int load(unsigned long uid, PDOC *d)
{
	HDR *h = fold_get(cur_folder, uid);
	char *raw;
	long len;
	MSG *m;
	int ok;
	mail_err[0] = 0;
	if (!h || (raw = mail_fetch(cur_folder, h, &len)) == 0)
		return 0;
	m = mime_parse(raw, len);
	ok = m && pdoc_make(d, m, h->date);
	if (m)
		mime_free(m);
	free(raw);
	return ok;
}

/* the printer's grid and font */
static short px0, py0, pcw, pch, fw, fh;

static void grid(PRINTJOB *pj)
{
	short mx = pj->w / 12, my = pj->ht / 16, hgt, cwr, chr;
	px0 = mx;
	py0 = my;
	pcw = (short)((pj->w - 2 * mx) / P_COLS);
	pch = (short)((pj->ht - 2 * my) / (P_ROWS + 2));
	if (pcw < 1)
		pcw = 1;
	if (pch < 1)
		pch = 1;
	vst_font(pj->h, 1);
	vst_color(pj->h, 1);
	vswr_mode(pj->h, 1);
	vst_alignment(pj->h, 0, 5);		/* left, the cell's top */
	/* the system font as large as the grid takes it */
	for (hgt = pch; hgt > 2; hgt--) {
		vst_height(pj->h, hgt, &cwr, &chr);
		if (cwr <= pcw && chr <= pch)
			break;
	}
	fw = cwr > 0 ? cwr : pcw;
	fh = chr > 0 ? chr : pch;
}

static void put(PRINTJOB *pj, short col, short row, const char *s, short n, short bold)
{
	if (n <= 0)
		return;
	vst_effects(pj->h, bold ? 1 : 0);
	v_gtext_n(pj->h, (short)(px0 + col * fw), (short)(py0 + row * pch), s, n);
}

static void page(PRINTJOB *pj, const PDOC *d, short pg, short pages)
{
	long i, end = (long)(pg + 1) * P_ROWS;
	short row = 0;
	char t[32], vis[BIDI_MAX];
	for (i = (long)pg * P_ROWS; i < d->n && i < end; i++, row++) {
		const PLINE *l = &d->l[i];
		const char *s = d->text + l->at;
		switch (l->kind) {
		case PL_HEAD:
		{
			short lb = l->label > l->n ? l->n : l->label;
			put(pj, 0, row, s, lb, 1);
			put(pj, lb, row, s + lb, (short)(l->n - lb), 0);
		}
			break;
		case PL_ATTACH: {
			/* "» name" in bold, "(type, size)" after it */
			const char *paren = 0;
			short k;
			for (k = l->n - 1; k > 2 && !paren; k--)
				if (s[k] == '(' && s[k - 1] == ' ' && s[k - 2] == ' ')
					paren = s + k;
			k = paren ? (short)(paren - s) : l->n;
			put(pj, 0, row, s, k, 1);
			put(pj, k, row, s + k, (short)(l->n - k), 0);
			break;
		}
		case PL_RULE: {
			short p[4];
			p[0] = px0;
			p[2] = (short)(px0 + P_COLS * fw);
			p[1] = p[3] = (short)(py0 + row * pch + pch / 2);
			v_pline(pj->h, 2, p);
			break;
		}
		default:
			/* a line with Hebrew in visual order, a Hebrew one to
			   the right */
			if (l->n > 0 && l->n <= BIDI_MAX && bidi_has_rtl(s, l->n)) {
				short rtl = bidi_is_rtl(s, l->n);
				bidi_visual(s, l->n, rtl, vis);
				put(pj, rtl ? (short)(P_COLS - l->n) : 0, row, vis, l->n, 0);
			} else {
				put(pj, 0, row, s, l->n, 0);
			}
		}
	}
	snprintf(t, sizeof(t), "Page %d of %d", pg + 1, pages);
	put(pj, (short)((P_COLS - strlen(t)) / 2), P_ROWS + 1, t, (short)strlen(t), 0);
}

static int stop_asked(void)
{
	busy_idle();		/* Esc or Undo: conn_stop */
	return conn_stop;
}

void cmd_print(void)
{
	unsigned long *u;
	short *pages;
	long n, k, total = 0, g;
	short pg, c;
	int ok = 1, stopped = 0;
	char what[60], name[64];
	PRINTJOB pj;
	PDOC d;
	if (!cur_folder)
		return;
	if (!vq_gdos()) {
		alert(1, "[1][Printing needs NVDI (or another GDOS)|with a printer driver set up.][ OK ]");
		return;
	}
	if ((u = list_targets(&n)) == 0)
		return;
	pages = malloc(n * sizeof(short));
	if (!pages) {
		free(u);
		alert(1, "[1][Not enough memory to print.][ OK ]");
		return;
	}
	/* each laid out once to count its pages (downloaded if it must be) */
	busy(1);
	conn_stop = 0;
	for (k = 0; ok && k < n; k++) {
		snprintf(what, sizeof(what), n > 1 ? "Preparing to print: %s of %s" : "Preparing to print",
			 num(k + 1), num(n));
		status(what);
		ok = load(u[k], &d);
		if (ok) {
			pages[k] = pages_of(&d);
			total += pages[k];
			pdoc_free(&d);
		}
		if (stop_asked())
			ok = 0, mail_err[0] = 0, stopped = 1;
	}
	busy(0);
	status("");
	if (!ok) {
		if (stopped)
			status("Printing stopped.");
		else
			alert(1, "[1][%s][ OK ]", mail_err[0] ? mail_err : "Not enough memory to print|this message.");
		goto done;
	}
	if (total > 9999)
		total = 9999;
	{
		HDR *h = fold_get(cur_folder, u[0]);
		snprintf(name, sizeof(name), "%.60s", n == 1 && h && h->subject && h->subject[0] ? h->subject : "EMail");
	}
	print_setup(&pj, (short)total);
	if (print_dialog(&pj, name) != 1) {
		print_end(&pj);
		goto done;
	}
	busy(1);
	if (!print_open(&pj)) {
		busy(0);
		print_end(&pj);
		alert(1, "[1][The printer can't be opened.|Check NVDI's printer driver|and that the printer is on.][ OK ]");
		goto done;
	}
	grid(&pj);
	conn_stop = 0;
	for (c = 0; ok && c < pj.copies; c++) {
		for (k = 0, g = 1; ok && k < n; g += pages[k], k++) {
			if (g + pages[k] - 1 < pj.first || g > pj.last)
				continue;	/* none of its pages asked for */
			if (!load(u[k], &d)) {
				ok = 0;
				break;
			}
			for (pg = 0; ok && pg < pages[k]; pg++) {
				long at = g + pg;
				if (at < pj.first || at > pj.last)
					continue;
				snprintf(what, sizeof(what), "Printing page %s of %s", num(at - pj.first + 1),
					 num(pj.last - pj.first + 1));
				status(what);
				page(&pj, &d, pg, pages[k]);
				print_page(&pj, at == pj.last && c == pj.copies - 1);
				if (stop_asked())
					ok = 0, stopped = 1;
			}
			pdoc_free(&d);
		}
	}
	print_end(&pj);		/* what was drawn goes out, a stop too */
	busy(0);
	/* the family's words, as EPDF's (Erez, 21 Nov 2026) */
	status(ok ? "Printed." : stopped ? "Printing stopped." : "");
	if (!ok && !stopped)
		alert(1, "[1][%s][ OK ]", mail_err[0] ? mail_err : "Not enough memory to print|this message.");
done:
	free(pages);
	free(u);
	menu_update();
}
