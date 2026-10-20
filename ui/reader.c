/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * reader.c - the message window: headers, attachments (click one to
 * save it; double-click an invitation to add it to ECal) and the text,
 * word-wrapped to the window. Hebrew paragraphs are laid out right to
 * left; quoted lines are drawn light.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui.h"
#include "theme.h"
#include "../src/mail.h"
#include "util.h"
#include "../src/plat.h"
#include "av.h"

MSG *cur_msg;
char *cur_raw;
long cur_rawlen;
unsigned long cur_uid;

enum { L_BODY, L_HEAD, L_ATTACH, L_RULE };

typedef struct {
	const char *s;
	long n;
	short kind, attach, label, rtl;
	short quote;		/* body: the quote depth of the line it is part of */
} LINE;

static char *head;		/* header lines, \n separated */
static char *body;		/* text with tabs expanded */
static long bodylen;
static LINE *lines;
static long nlines;

static void free_layout(void)
{
	free(lines);
	lines = 0;
	nlines = 0;
	w_reader.total = 0;
}

static long selection;		/* several messages selected: how many */

void reader_clear(void)
{
	reader_missing = 0;
	selection = 0;
	free_layout();
	mime_free(cur_msg);
	cur_msg = 0;
	free(cur_raw);
	cur_raw = 0;
	free(head);
	head = 0;
	free(body);
	body = 0;
	cur_uid = 0;
	w_reader.top = 0;
	win_title(&w_reader, "Message");
	win_info(&w_reader, "");
	win_sliders(&w_reader);
	win_redraw(&w_reader, 0);
}

static void add_line(const char *s, long n, short kind, short attach, short label)
{
	LINE *l = &lines[nlines++];
	l->s = s;
	l->n = n;
	l->kind = kind;
	l->attach = attach;
	l->label = label;
	l->rtl = kind == L_BODY ? line_rtl(s, n) : 0;
	l->quote = 0;
}

/* how deep a line is quoted: the '>' at its start, spaces between them */
static short quote_depth(const char *s, long n)
{
	short d = 0;
	long k;
	for (k = 0; k < n && (s[k] == '>' || s[k] == ' '); k++)
		d += s[k] == '>';
	return d;
}

/* lay the message out for the current window width */
static void layout(void)
{
	short width = w_reader.work.w / cw - 1;
	long hl = head ? strlen(head) : 0, max, *br, nb, i;
	const char *p;
	free_layout();
	if (!cur_msg)
		return;
	if (width < 20)
		width = 20;
	max = bodylen / 8 + hl / 8 + cur_msg->nparts + 64;
	br = malloc(max * sizeof(long));
	lines = malloc((max + 8) * sizeof(LINE));
	if (!br || !lines) {
		free(br);
		free_layout();
		return;
	}
	/* header lines: "Label: value", wrapped with an indent */
	for (p = head; p && *p; ) {
		const char *e = strchr(p, '\n');
		long n = e ? e - p : (long)strlen(p);
		const char *colon = memchr(p, ':', n);
		short label = colon ? (short)(colon - p + 1) : 0;
		nb = wrap_text(p, n, width, br, max - nlines - 8);
		for (i = 0; i < nb; i++) {
			long s = br[i], end = i + 1 < nb ? br[i + 1] : n;
			add_line(p + s, end - s, L_HEAD, 0, i == 0 ? label : 0);
		}
		p = e ? e + 1 : p + n;
	}
	for (i = 0; i < cur_msg->nparts; i++)
		add_line(cur_msg->parts[i].name, strlen(cur_msg->parts[i].name), L_ATTACH, (short)i, 0);
	add_line("", 0, L_RULE, 0, 0);
	nb = wrap_text(body, bodylen, width, br, max - nlines - 2);
	{
		short depth = 0;
		for (i = 0; i < nb; i++) {
			long s = br[i], end = i + 1 < nb ? br[i + 1] : bodylen;
			/* a line of the text starts here, or this is the rest of
			   one wrapped: it keeps the depth (and colour) of its start */
			if (s == 0 || body[s - 1] == '\n')
				depth = quote_depth(body + s, (i + 1 < nb ? br[i + 1] : bodylen) - s);
			while (end > s && (body[end - 1] == '\n' || body[end - 1] == ' '))
				end--;
			add_line(body + s, end - s, L_BODY, 0, 0);
			lines[nlines - 1].quote = depth;
		}
	}
	free(br);
	w_reader.total = nlines;
	win_sliders(&w_reader);
}

static void resized(WIN *w)
{
	(void)w;
	layout();
}

static char *expand_tabs(const char *s, long n, long *outlen)
{
	SBUF b;
	long i, col = 0;
	sb_init(&b);
	for (i = 0; i < n; i++) {
		if (s[i] == '\t') {
			do
				sb_addc(&b, ' ');
			while (++col % 8);
		} else {
			sb_addc(&b, s[i]);
			col = s[i] == '\n' ? 0 : col + 1;
		}
	}
	*outlen = b.len;
	return sb_steal(&b);
}

int reader_missing;

void reader_selection(long n)
{
	reader_clear();
	selection = n;
	{
		char t[40];
		snprintf(t, sizeof(t), "%s messages", num(n));
		win_title(&w_reader, t);
	}
	win_redraw(&w_reader, 0);
}

static char *dup0(const char *s)
{
	return strdup(s ? s : "");
}

/* a message that isn't in the cache while EMail can't connect: say so in
   the reader, with what the list knows about it */
static MSG *missing_note(HDR *h, ACCOUNT *a)
{
	MSG *m = calloc(1, sizeof(MSG));
	SBUF t;
	if (!m)
		return 0;
	m->from = dup0(h->from);
	m->to = dup0(h->to);
	m->subject = dup0(h->subject);
	m->cc = dup0(0);
	m->reply_to = dup0(0);
	m->date = dup0(0);
	m->message_id = dup0(0);
	m->references = dup0(0);
	m->in_reply_to = dup0(0);
	m->list_post = dup0(0);
	sb_init(&t);
	sb_adds(&t, "This message hasn't been downloaded yet.\n\n");
	if (opt.offline)
		sb_adds(&t, "EMail is working offline: switch it off in the Options menu to read it.\n");
	else
		sb_printf(&t, "%s is not connected, so EMail shows what it has in the cache.\n"
			      "Check mail (^K, or click the account's name) to connect again.\n\n%s\n",
			  a->name, a->cuterr);
	m->textlen = t.len;
	m->text = sb_steal(&t);
	if (!m->text || !m->from || !m->subject) {
		mime_free(m);
		return 0;
	}
	return m;
}

/* ---------------- people in the header lines ----------------
 * From, To and Cc show each person by the name EContact has for them,
 * else as the address; a name's address is its tooltip (tips[]) */
typedef struct {
	long at, n;			/* the name, in head */
	char addr[PEOPLE_ADDR];
} TIP;
static TIP *tips;
static int ntips, maxtips;
static unsigned long head_date;

static void add_people(SBUF *b, const char *label, const char *list)
{
	const char *p = list;
	int first = 1;
	if (!list[0])
		return;
	sb_adds(b, label);
	while (*p) {
		char one[256], mail[PEOPLE_ADDR];
		const char *name;
		short q = 0, ang = 0, n = 0;
		while (*p == ' ' || *p == ',')
			p++;
		while (*p && (q || ang || *p != ',')) {
			if (*p == '"')
				q = !q;
			else if (!q && *p == '<')
				ang = 1;
			else if (!q && *p == '>')
				ang = 0;
			if (n < (short)sizeof(one) - 1)
				one[n++] = *p;
			p++;
		}
		one[n] = 0;
		if (!n)
			continue;
		addr_split(one, 0, 0, mail, sizeof(mail));
		if (!first)
			sb_adds(b, ", ");
		first = 0;
		if (!mail[0]) {
			sb_adds(b, one);
			continue;
		}
		name = people_name(mail);
		if (name && *name) {
			if (ntips == maxtips) {
				TIP *more = realloc(tips, (maxtips + 8) * sizeof(TIP));
				if (more) {
					tips = more;
					maxtips += 8;
				}
			}
			if (ntips < maxtips) {
				tips[ntips].at = b->len;
				tips[ntips].n = (long)strlen(name);
				str_copy(tips[ntips].addr, mail, sizeof(tips[0].addr));
				ntips++;
			}
			sb_adds(b, name);
		} else {
			sb_adds(b, mail);
		}
	}
	sb_adds(b, "\n");
}

/* the header lines of cur_msg, into head */
static void make_head(void)
{
	SBUF hb;
	free(head);
	ntips = 0;
	sb_init(&hb);
	add_people(&hb, "From: ", cur_msg->from);
	add_people(&hb, "To: ", cur_msg->to);
	add_people(&hb, "Cc: ", cur_msg->cc);
	if (head_date) {
		char d[24];
		date_str(head_date, d, sizeof(d), 1);
		sb_printf(&hb, "Date: %s\n", d);
	}
	sb_adds(&hb, "Subject: ");
	sb_adds(&hb, cur_msg->subject);
	sb_adds(&hb, "\n");
	head = sb_steal(&hb);
}

/* EContact's names came, or changed: the header lines again */
void reader_names_came(void)
{
	long top = w_reader.top;
	if (!cur_msg || !head)
		return;
	make_head();
	layout();
	win_scroll_to(&w_reader, top);
	win_redraw(&w_reader, 0);
}

/* the tooltip over a name in the header lines: its address */
int reader_tip(short mx, short my, char *out, int size)
{
	long i;
	short col;
	int k;
	if (!cur_msg || !head || w_reader.work.w <= 0 || mx < w_reader.work.x || my < w_reader.work.y ||
	    mx >= w_reader.work.x + w_reader.work.w || my >= w_reader.work.y + w_reader.work.h)
		return 0;
	i = w_reader.top + (my - w_reader.work.y) / ch;
	if (i < 0 || i >= nlines || lines[i].kind != L_HEAD)
		return 0;
	col = (mx - w_reader.work.x - cw / 2) / cw;
	if (col < 0 || col >= lines[i].n)
		return 0;
	for (k = 0; k < ntips; k++) {
		long at = (lines[i].s - head) + col;
		if (at >= tips[k].at && at < tips[k].at + tips[k].n) {
			str_copy(out, tips[k].addr, size);
			return 1;
		}
	}
	return 0;
}

void reader_show(HDR *h)
{
	long len;
	char *raw;
	if (!cur_folder || !h)
		return;
	busy(1);
	status("Opening the message...");
	mail_err[0] = 0;
	raw = mail_fetch(cur_folder, h, &len);
	busy(0);
	reader_missing = 0;
	if (!raw && mail_unreachable) {
		MSG *m = missing_note(h, cur_folder->acct);
		if (m) {
			reader_clear();
			reader_missing = 1;
			cur_msg = m;
			cur_uid = h->uid;
			goto show;
		}
	}
	if (!raw) {
		status("");
		alert(1, "[1][%s][ OK ]", mail_err[0] ? mail_err : "Can't read this message.");
		return;
	}
	reader_clear();
	cur_raw = raw;
	cur_rawlen = len;
	cur_uid = h->uid;
	cur_msg = mime_parse(raw, len);
	if (!cur_msg) {
		status("");
		alert(1, "[1][Not enough memory to show|this message.][ OK ]");
		return;
	}
show:
	head_date = h->date;
	make_head();
	body = expand_tabs(cur_msg->text, cur_msg->textlen, &bodylen);
	{
		char t[100];
		snprintf(t, sizeof(t), "%.90s", cur_msg->subject[0] ? cur_msg->subject : "(no subject)");
		win_title(&w_reader, t);
		short k, invite = 0;
		for (k = 0; k < cur_msg->nparts; k++)
			invite |= mime_is_calendar(&cur_msg->parts[k]);
		if (cur_msg->html)
			win_info(&w_reader, "HTML shown as text");
		else if (invite)
			win_info(&w_reader, "double-click the invitation for ECal");
		else if (cur_msg->nparts)
			win_info(&w_reader, "click an attachment to save it");
		else
			win_info(&w_reader, "");
	}
	w_reader.top = 0;
	busy(1);
	layout();
	busy(0);
	status("");
	win_redraw(&w_reader, 0);
}

static void draw(WIN *w, GRECT *clip)
{
	long i, rows = win_rows(w) + 1;
	short cols = w->work.w / cw - 1, x0 = w->work.x + cw / 2;
	fill(clip, 0);
	if (!cur_msg && selection > 1) {
		static const char *how[] = {
			"Message menu, right-click or keys for all of them:",
			"^U unread  ^G flag  ^M move  Del delete",
			"A click without Shift or Control selects one again."
		};
		char t[60];
		short k;
		snprintf(t, sizeof(t), "%s messages selected.", num(selection));
		text_at(x0, w->work.y + ch, t, strlen(t), cols, TX_BOLD);
		for (k = 0; k < 3; k++)
			text_at(x0, w->work.y + (3 + k) * ch, how[k], strlen(how[k]), cols, 0);
		return;
	}
	if (!cur_msg) {
		text_at(x0, w->work.y + ch, "No message selected.", 20, cols, TX_LIGHT);
		return;
	}
	for (i = w->top; i < nlines && i < w->top + rows; i++) {
		LINE *l = &lines[i];
		short y = w->work.y + (short)((i - w->top) * ch);
		switch (l->kind) {
		case L_HEAD:
			if (l->label) {
				short v = l->label;
				if (v < l->n && l->s[v] == ' ')
					v++;
				text_pen_at(pen[P_DIM], x0, y, l->s, l->label, cols, TX_BOLD);
				text_at(x0 + (l->label + 1) * cw, y, l->s + v, l->n - v, cols - l->label - 1,
					/* names and addresses read left to right; a
					   Hebrew subject is a Hebrew sentence */
					strncmp(l->s, "Subject:", 8) ? TX_LTR : 0);
			} else {
				text_at(x0, y, l->s, l->n, cols, TX_LTR);
			}
			break;
		case L_ATTACH: {
			/* "» name  (type, size)": the name laid out on its own,
			   so a Hebrew file name doesn't turn the line around */
			char t[80], sz[16];
			MIMEPART *p = &cur_msg->parts[l->attach];
			const char *nm = p->name;
			short pre = 0, nl;
			if (mime_is_calendar(p)) {	/* "» Invitation: its summary" */
				text_at(x0 + 2 * cw, y, "Invitation:", 11, cols - 2, TX_BOLD);
				pre = 12;
				if (p->summary)
					nm = p->summary;
			}
			nl = (short)strlen(nm);
			if (nl > cols - 4 - pre)
				nl = cols - 4 - pre;
			if (nl < 0)
				nl = 0;
			if (p->size < 1024)
				snprintf(sz, sizeof(sz), "%s bytes", num(p->size));
			else
				snprintf(sz, sizeof(sz), "%s KB", num((p->size + 512) / 1024));
			snprintf(t, sizeof(t), "  (%s, %s)", p->type, sz);
			text_at(x0, y, "\xAF", 1, cols, TX_BOLD);
			text_at(x0 + (2 + pre) * cw, y, nm, nl, nl, TX_BOLD);
			text_at(x0 + (2 + pre + nl) * cw, y, t, strlen(t), cols - 2 - pre - nl, 0);
			break;
		}
		case L_RULE:
			hline(w->work.x, w->work.x + w->work.w - 1, y + ch / 2);
			break;
		default:
			if (colour && l->quote) {
				/* quoted text by depth: blue, green, brown, as in
				   Thunderbird; a wrapped line's rest as its start */
				short d = l->quote;
				text_pen_at(pen[d == 1 ? P_Q1 : d == 2 ? P_Q2 : P_Q3], x0, y, l->s, l->n, cols, TX_RIGHT);
			} else {
				text_at(x0, y, l->s, l->n, cols, TX_RIGHT | (l->quote ? TX_LIGHT : 0));
			}
		}
	}
}

/* an 8.3 name for saving: the ASCII letters of the original */
static void short_name(const char *name, char *out)
{
	const char *dot = strrchr(name, '.');
	short n = 0, e = 0;
	const char *p;
	for (p = name; *p && p != dot && n < 8; p++) {
		unsigned char c = (unsigned char)*p;
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
			out[n++] = (char)c;
		else if (c >= 'a' && c <= 'z')
			out[n++] = (char)(c - 32);
	}
	if (!n) {
		strcpy(out, "ATTACH");
		n = 6;
	}
	if (dot) {
		out[n++] = '.';
		for (p = dot + 1; *p && e < 3; p++) {
			unsigned char c = (unsigned char)*p;
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
				out[n++] = (char)(c >= 'a' ? c - 32 : c);
				e++;
			}
		}
	}
	out[n] = 0;
}

void reader_save_attachment(short i)
{
	static char path[160], name[16];
	char full[200], *data;
	short button;
	long len;
	if (!cur_msg || i < 0 || i >= cur_msg->nparts)
		return;
	if (!path[0]) {
		str_copy(path, opt.workdir, sizeof(path) - 5);
		strcat(path, "\\*.*");
	}
	short_name(cur_msg->parts[i].name, name);
	if (!fsel_exinput(path, name, &button, "Save attachment as") || !button || !name[0])
		return;
	str_copy(full, path, sizeof(full));
	{
		char *bs = strrchr(full, '\\');
		if (bs)
			bs[1] = 0;
	}
	strncat(full, name, sizeof(full) - strlen(full) - 1);
	busy(1);
	statusf("Saving %s...", name);
	data = mime_part_data(cur_raw, &cur_msg->parts[i], &len);
	if (!data || pf_save(full, data, len) < 0)
		alert(1, "[1][Can't write|%s][ OK ]", full);
	free(data);
	busy(0);
	status("");
}

#define AV_STARTED 0x4738

/* ECal's AV_STARTED for our VA_START, up to 3 s; the rest wait as in
   any modal loop (EContact's answers are read at once) */
static int ecal_answered(short id)
{
	short m[8], tenths = 30;
	AEVENT e;
	while (tenths-- > 0) {
		if (!(evnt_multi_(MU_MESAG | MU_TIMER, 0, 0, 0, 100, m, &e) & MU_MESAG))
			continue;
		if (m[0] == AV_STARTED && m[1] == id)
			return 1;
		ui_modal_message(m);
	}
	return 0;
}

/* an invitation to ECal (the guidelines, 4.1): written whole as
   INVITE.ICS in EMail's folder, closed, then named to ECal (the program
   or the accessory) with VA_START. ECal reads it, answers AV_STARTED and
   opens its New Event dialog filled from it; the next one overwrites it */
void reader_to_ecal(short i)
{
	static const char *const names[2] = { "ECAL    ", "ECALST  " };
	char path[220], cmd[230], *data;
	long len;
	short id = -1, k;
	int ok;
	if (!cur_msg || i < 0 || i >= cur_msg->nparts)
		return;
	for (k = 0; k < 2 && id < 0; k++)
		id = appl_find(names[k]);
	if (id < 0) {
		alert(1, "[1][ECal isn't running: open it|and try again, or save the|invitation.][ OK ]");
		return;
	}
	path_join(path, sizeof(path), opt.workdir, "INVITE.ICS");
	busy(1);
	data = mime_part_data(cur_raw, &cur_msg->parts[i], &len);
	ok = data && pf_save(path, data, len) >= 0;
	free(data);
	busy(0);
	if (!ok) {
		alert(1, "[1][Can't write|%s][ OK ]", path);
		return;
	}
	snprintf(cmd, sizeof(cmd), strchr(path, ' ') ? "'%s'" : "%s", path);
	av_send(names[k - 1], cmd);
	status(ecal_answered(id) ? "The invitation is in ECal: confirm it there"
				 : "ECal didn't answer: try again when it's free");
}

static void click(WIN *w, short mx, short my, short clicks, short kstate)
{
	long i = w->top + (my - w->work.y) / ch;
	(void)mx;
	(void)kstate;
	if (i < 0 || i >= nlines || lines[i].kind != L_ATTACH)
		return;
	if (!mime_is_calendar(&cur_msg->parts[lines[i].attach]))
		reader_save_attachment(lines[i].attach);
	else if (clicks >= 2)			/* (a click alone opens no dialog: the second comes) */
		reader_to_ecal(lines[i].attach);
	else
		status("Double-click the invitation to add it to ECal; right-click to save it");
}

void message_menu(short mx, short my, int with_open);

static void rclick(WIN *w, short mx, short my)
{
	long i = w->top + (my - w->work.y) / ch;
	if (i >= 0 && i < nlines && lines[i].kind == L_ATTACH) {
		static const char *const lab[] = { "Save attachment...", "Add to ECal" };
		short a = lines[i].attach, inv = mime_is_calendar(&cur_msg->parts[a]);
		/* an invitation: Add to ECal first, beside Save */
		short r = popup(mx, my, inv ? lab + 1 : lab, inv ? 2 : 1);
		if (inv && r == 0)
			reader_to_ecal(a);
		else if (r == (inv ? 1 : 0))
			reader_save_attachment(a);
		return;
	}
	message_menu(mx, my, 0);
}

static int key(WIN *w, short kstate, short k)
{
	long rows = win_rows(w);
	switch (KEY_SCAN(k)) {
	case 0x48: win_scroll_to(w, w->top - ((kstate & 3) ? rows : 1)); return 1;
	case 0x50: win_scroll_to(w, w->top + ((kstate & 3) ? rows : 1)); return 1;
	case 0x39: win_scroll_to(w, w->top + rows - 1); return 1;	/* space */
	case 0x0e: win_scroll_to(w, w->top - rows + 1); return 1;	/* backspace */
	case 0x47: win_scroll_to(w, (kstate & 3) ? w->total : 0); return 1;
	}
	return 0;
}

void reader_init(void);
void reader_init(void)
{
	w_reader.h = -1;
	w_reader.pane = 1;
	w_reader.draw = draw;
	w_reader.click = click;
	w_reader.rclick = rclick;
	w_reader.key = key;
	w_reader.resized = resized;
	strcpy(w_reader.title, "Message");
}
