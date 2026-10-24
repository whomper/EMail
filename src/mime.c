/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * mime.c - reading internet messages (RFC 5322 + MIME).
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "mime.h"
#include "charset.h"
#include "util.h"

typedef unsigned char u8;

long mime_header_len(const char *raw, long len)
{
	long i;
	if (len >= 2 && raw[0] == '\r' && raw[1] == '\n')
		return 2;
	if (len >= 1 && raw[0] == '\n')
		return 1;
	for (i = 0; i + 1 < len; i++) {
		if (raw[i] != '\n')
			continue;
		if (raw[i + 1] == '\n')
			return i + 2;
		if (raw[i + 1] == '\r' && i + 2 < len && raw[i + 2] == '\n')
			return i + 3;
	}
	return len;
}

char *mime_header(const char *hdr, long n, const char *name)
{
	long nl = strlen(name), i = 0;
	while (i < n) {
		/* at the start of a line */
		if (i + nl < n && hdr[i + nl] == ':' && !strncasecmp(hdr + i, name, nl)) {
			SBUF b;
			long j = i + nl + 1;
			sb_init(&b);
			for (;;) {
				long s = j;
				while (j < n && hdr[j] != '\n')
					j++;
				sb_add(&b, hdr + s, j - s);
				if (b.len && b.s[b.len - 1] == '\r')
					b.s[--b.len] = 0;
				if (j + 1 < n && (hdr[j + 1] == ' ' || hdr[j + 1] == '\t')) {
					j++;	/* folded line: continue */
					continue;
				}
				break;
			}
			{
				char *t = str_trim(b.s ? b.s : (char *)"");
				char *r = strdup(t);
				sb_free(&b);
				return r;
			}
		}
		while (i < n && hdr[i] != '\n')
			i++;
		i++;
	}
	return 0;
}

char *mime_param(const char *value, const char *param)
{
	const char *p = value;
	long pl = strlen(param);
	if (!value)
		return 0;
	while ((p = strchr(p, ';'))) {
		const char *s, *e;
		int star = 0;
		p++;
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			p++;
		if (strncasecmp(p, param, pl))
			continue;
		s = p + pl;
		if (*s == '*') {	/* RFC 2231: name*=utf-8''%D7%A9... (or name*0=) */
			star = 1;
			s++;
			if (*s == '0') {
				s++;
				if (*s == '*')
					s++;
				else
					star = 0;
			}
		}
		while (*s == ' ')
			s++;
		if (*s != '=')
			continue;
		s++;
		while (*s == ' ')
			s++;
		if (*s == '"') {
			SBUF b;
			sb_init(&b);
			for (s++; *s && *s != '"'; s++) {
				if (*s == '\\' && s[1])
					s++;
				sb_addc(&b, *s);
			}
			return sb_steal(&b);
		}
		e = s;
		while (*e && *e != ';' && *e != ' ' && *e != '\t' && *e != '\r' && *e != '\n')
			e++;
		if (star) {
			/* charset'lang'percent-encoded */
			const char *q1 = memchr(s, '\'', e - s), *q2;
			char cs[32] = "us-ascii", *raw, *out;
			long rl = 0;
			if (q1 && (q2 = memchr(q1 + 1, '\'', e - q1 - 1))) {
				str_copy(cs, s, (q1 - s + 1) < 32 ? (q1 - s + 1) : 32);
				s = q2 + 1;
			}
			raw = malloc(e - s + 1);
			if (!raw)
				return 0;
			while (s < e) {
				if (*s == '%' && s + 2 < e + 0 + 1 && isxdigit((u8)s[1]) && isxdigit((u8)s[2])) {
					char h[3] = { s[1], s[2], 0 };
					raw[rl++] = (char)strtoul(h, 0, 16);
					s += 3;
				} else {
					raw[rl++] = *s++;
				}
			}
			out = cs_to_atari(raw, rl, cs_id(cs), 0);
			free(raw);
			return out;
		}
		return str_ndup(s, e - s);
	}
	return 0;
}

/* ---------------- dates ---------------- */

static const char months[] = "janfebmaraprmayjunjulaugsepoctnovdec";

static unsigned long days_from_civil(long y, short m, short d)
{
	long era, yoe, doy, doe;
	y -= m <= 2;
	era = y / 400;
	yoe = y - era * 400;
	doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return (unsigned long)(era * 146097 + doe - 719468);
}

unsigned long mime_date(const char *s)
{
	long day = 0, year = 0, hh = 0, mm = 0, ss = 0, tz = 0;
	short mon = 0, i;
	const char *p = s;
	char *e;
	if (!s)
		return 0;
	while (*p && !isdigit((u8)*p))	/* skip "Tue, " */
		p++;
	day = strtol(p, &e, 10);
	p = e;
	while (*p == ' ' || *p == '-')
		p++;
	for (i = 0; i < 12; i++)
		if (!strncasecmp(p, months + i * 3, 3))
			mon = i + 1;
	if (!mon)
		return 0;
	p += 3;
	while (*p == ' ' || *p == '-')
		p++;
	year = strtol(p, &e, 10);
	if (e == p)
		return 0;
	if (year < 50)
		year += 2000;
	else if (year < 100)
		year += 1900;
	p = e;
	while (*p == ' ')
		p++;
	hh = strtol(p, &e, 10);
	if (*e == ':') {
		mm = strtol(e + 1, &e, 10);
		if (*e == ':')
			ss = strtol(e + 1, &e, 10);
	}
	p = e;
	while (*p == ' ')
		p++;
	if (*p == '+' || *p == '-') {
		long z = strtol(p + 1, 0, 10);
		tz = (z / 100) * 60 + z % 100;
		if (*p == '-')
			tz = -tz;
	} else if (!strncasecmp(p, "EDT", 3)) {
		tz = -240;
	} else if (!strncasecmp(p, "EST", 3) || !strncasecmp(p, "CDT", 3)) {
		tz = !strncasecmp(p, "EST", 3) ? -300 : -300;
	} else if (!strncasecmp(p, "PDT", 3)) {
		tz = -420;
	} else if (!strncasecmp(p, "PST", 3)) {
		tz = -480;
	}
	if (year < 1970 || day < 1 || day > 31)
		return 0;
	return days_from_civil(year, mon, (short)day) * 86400UL + hh * 3600 + mm * 60 + ss - tz * 60;
}

/* ---------------- addresses ---------------- */

void addr_split(const char *addr, char *name, int nsize, char *email, int esize)
{
	const char *lt = strchr(addr, '<'), *gt;
	if (name)
		*name = 0;
	if (email)
		*email = 0;
	if (lt && (gt = strchr(lt, '>'))) {
		if (email) {
			long n = gt - lt - 1;
			str_copy(email, lt + 1, n + 1 < esize ? n + 1 : esize);
		}
		if (name) {
			long n = lt - addr;
			char *t;
			str_copy(name, addr, n + 1 < nsize ? n + 1 : nsize);
			t = str_trim(name);
			if (*t == '"') {
				t++;
				if (*t && t[strlen(t) - 1] == '"')
					t[strlen(t) - 1] = 0;
			}
			memmove(name, t, strlen(t) + 1);
		}
	} else {
		char tmp[256], *t;
		str_copy(tmp, addr, sizeof(tmp));
		t = str_trim(tmp);
		/* "a@b (Name)" */
		if (strchr(t, '(')) {
			char *o = strchr(t, '('), *c = strchr(o, ')');
			if (name && c) {
				*c = 0;
				str_copy(name, o + 1, nsize);
			}
			*o = 0;
			t = str_trim(t);
		}
		if (email)
			str_copy(email, t, esize);
	}
}

/* ---------------- HTML ---------------- */

static const struct {
	const char *name;
	unsigned short u;
} entities[] = {
	{ "nbsp", 0xA0 }, { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' },
	{ "apos", '\'' }, { "copy", 0xA9 }, { "reg", 0xAE }, { "trade", 0x2122 },
	{ "hellip", 0x2026 }, { "mdash", 0x2014 }, { "ndash", 0x2013 }, { "rsquo", 0x2019 },
	{ "lsquo", 0x2018 }, { "rdquo", 0x201D }, { "ldquo", 0x201C }, { "bull", 0x2022 },
	{ "middot", 0xB7 }, { "euro", 0x20AC }, { "pound", 0xA3 }, { "laquo", 0xAB },
	{ "raquo", 0xBB }, { "eacute", 0xE9 }, { "egrave", 0xE8 }, { "agrave", 0xE0 },
	{ "uuml", 0xFC }, { "ouml", 0xF6 }, { "auml", 0xE4 }, { "szlig", 0xDF },
	{ "ccedil", 0xE7 }, { "zwnj", 0x200C }, { "rlm", 0x200F }, { "lrm", 0x200E },
	{ "shy", 0x00AD }, { "times", 0xD7 }, { "deg", 0xB0 }, { 0, 0 }
};

static int tag_is(const char *t, long tl, const char *name)
{
	long n = strlen(name);
	return tl >= n && !strncasecmp(t, name, n) && (tl == n || !isalnum((u8)t[n]));
}

static int blank(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* the value of attribute name in a tag's text t[0..tl) (after "<"), its
   &amp; &quot; &#39; &lt; &gt; as the characters; blanks left out (an
   address) or made single spaces (words). malloc'ed, or NULL */
static char *attr(const char *t, long tl, const char *name, int words)
{
	long nl = (long)strlen(name), i, e, k = 0;
	char *u;
	for (i = 1; i + nl <= tl; i++)
		if (blank(t[i - 1]) && !strncasecmp(t + i, name, nl) && (i + nl == tl || blank(t[i + nl]) ||
									 t[i + nl] == '='))
			break;
	if (i + nl > tl)
		return 0;
	for (i += nl; i < tl && blank(t[i]); i++)
		;
	if (i >= tl || t[i] != '=')
		return 0;
	for (i++; i < tl && blank(t[i]); i++)
		;
	if (i < tl && (t[i] == '"' || t[i] == '\'')) {
		char q = t[i++];
		for (e = i; e < tl && t[e] != q; e++)
			;
	} else {
		for (e = i; e < tl && !blank(t[e]); e++)
			;
	}
	while (i < e && (u8)t[i] <= ' ')
		i++;
	while (e > i && (u8)t[e - 1] <= ' ')
		e--;
	if (!(u = malloc(e - i + 1)))
		return 0;
	while (i < e) {
		static const struct { const char *s; char c; } ent[] = {
			{ "&amp;", '&' }, { "&#38;", '&' }, { "&quot;", '"' }, { "&#39;", '\'' },
			{ "&lt;", '<' }, { "&gt;", '>' }, { "&nbsp;", ' ' }, { 0, 0 }
		};
		short j;
		for (j = 0; ent[j].s; j++) {
			long l = (long)strlen(ent[j].s);
			if (e - i >= l && !strncasecmp(t + i, ent[j].s, l))
				break;
		}
		if (ent[j].s) {
			u[k++] = ent[j].c;
			i += (long)strlen(ent[j].s);
		} else if ((u8)t[i] > ' ') {
			u[k++] = t[i++];
		} else {
			if (words && k && u[k - 1] != ' ')
				u[k++] = ' ';
			i++;
		}
	}
	u[k] = 0;
	return u;
}

/* an <a>'s href, when it is an address the reader opens: malloc'ed, or NULL */
static char *href(const char *t, long tl)
{
	char *u = attr(t, tl, "href", 0);
	if (u && !link_wanted(u)) {
		free(u);
		u = 0;
	}
	return u;
}

/* a quoted line's start (<blockquote>, depth bq): "> ", ">> " ... */
static void quote_mark(SBUF *b, short bq)
{
	short k;
	if (!bq || (b->len && b->s[b->len - 1] != '\n'))
		return;
	for (k = 0; k < bq; k++)
		sb_addc(b, '>');
	sb_addc(b, ' ');
}

/* the link of an <a> ends at the text's end b->len: kept when there is
   text on it, its blanks left out */
static void link_end(SBUF *b, MLINK **lk, short *nlk, long at, char *url)
{
	long e = b->len;
	MLINK *more = 0;
	while (at < e && (b->s[at] == ' ' || b->s[at] == '\n'))
		at++;
	while (e > at && (b->s[e - 1] == ' ' || b->s[e - 1] == '\n'))
		e--;
	if (e <= at || *nlk >= MIME_MAXLINKS ||
	    (!(*nlk & 15) && !(more = realloc(*lk, (*nlk + 16) * sizeof(MLINK))))) {
		free(url);
		return;
	}
	if (!(*nlk & 15))
		*lk = more;
	(*lk)[*nlk].at = at;
	(*lk)[*nlk].n = e - at;
	(*lk)[*nlk].url = url;
	(*nlk)++;
}

char *html_to_text(const char *h, long n, long *outlen)
{
	return html_to_text_links(h, n, outlen, 0, 0);
}

char *html_to_text_links(const char *h, long n, long *outlen, MLINK **lk, short *nlk)
{
	SBUF b;
	long i = 0, link_at = 0;
	short skip = 0, pre = 0, space = 0, nl = 2, bq = 0;
	char *link = 0;		/* the <a href> open, from link_at */
	sb_init(&b);

	while (i < n) {
		u8 c = (u8)h[i];
		if (c == '<') {
			long j = i + 1, tl;
			const char *t;
			short close = 0;
			if (i + 3 < n && !strncmp(h + i, "<!--", 4)) {
				const char *e = 0;
				long k;
				for (k = i + 4; k + 2 < n; k++)
					if (h[k] == '-' && h[k + 1] == '-' && h[k + 2] == '>') {
						e = h + k;
						break;
					}
				i = e ? (e - h) + 3 : n;
				continue;
			}
			while (j < n && h[j] != '>')
				j++;
			t = h + i + 1;
			if (*t == '/') {
				close = 1;
				t++;
			}
			tl = (h + j) - t;
			i = j + 1;
			if (tag_is(t, tl, "style") || tag_is(t, tl, "script") || tag_is(t, tl, "head") ||
			    tag_is(t, tl, "title")) {
				skip = close ? 0 : 1;
				continue;
			}
			if (skip)
				continue;
			if (tag_is(t, tl, "a")) {
				if (lk && link)
					link_end(&b, lk, nlk, link_at, link);
				link = lk && !close ? href(t, tl) : 0;
				if (link && nl)
					quote_mark(&b, bq);	/* the line's quote marks aren't the link's */
				link_at = b.len;
				continue;
			}
			if (!close && tag_is(t, tl, "img")) {
				/* a picture: its words (alt), so a picture that is a
				   link ("Shop now") has text to click */
				char *alt = attr(t, tl, "alt", 1);
				if (alt && *alt) {
					if (space && nl == 0)
						sb_addc(&b, ' ');
					space = 0;
					quote_mark(&b, bq);
					sb_adds(&b, alt);
					nl = 0;
				}
				free(alt);
				continue;
			}
			if (tag_is(t, tl, "blockquote")) {
				/* a quote (a reply's): its lines start "> ", as plain text quotes */
				while (nl < 2) {
					sb_addc(&b, '\n');
					nl++;
				}
				bq = close ? (bq > 0 ? bq - 1 : 0) : (bq < 8 ? bq + 1 : 8);
				space = 0;
				continue;
			}
			if (tag_is(t, tl, "pre")) {
				pre = !close;
				continue;
			}
			if (tag_is(t, tl, "br") || (close && (tag_is(t, tl, "div") || tag_is(t, tl, "tr") ||
			    tag_is(t, tl, "li") || tag_is(t, tl, "table")))) {
				if (nl < 1 || tag_is(t, tl, "br")) {
					sb_addc(&b, '\n');
					nl++;
				}
				space = 0;
				continue;
			}
			if (tag_is(t, tl, "p") || (tag_is(t, tl, "h1") || tag_is(t, tl, "h2") ||
			    tag_is(t, tl, "h3") || tag_is(t, tl, "h4"))) {
				while (nl < 2) {
					sb_addc(&b, '\n');
					nl++;
				}
				space = 0;
				continue;
			}
			if (!close && tag_is(t, tl, "li")) {
				if (nl < 1)
					sb_addc(&b, '\n');
				quote_mark(&b, bq);
				sb_adds(&b, "  - ");
				nl = 0;
				space = 0;
				continue;
			}
			if (!close && (tag_is(t, tl, "td") || tag_is(t, tl, "th")) && nl == 0) {
				sb_addc(&b, ' ');
				continue;
			}
			if (!close && tag_is(t, tl, "hr")) {
				if (nl < 1)
					sb_addc(&b, '\n');
				quote_mark(&b, bq);
				sb_adds(&b, "----------------------------------------\n");
				nl = 1;
				continue;
			}
			continue;
		}
		if (skip) {
			i++;
			continue;
		}
		if (c == '&') {
			char out[4];
			short k = 0, ok = 0;
			unsigned long u = 0;
			long j = i + 1;
			if (j < n && h[j] == '#') {
				j++;
				if (j < n && (h[j] == 'x' || h[j] == 'X'))
					u = strtoul(h + j + 1, 0, 16);
				else
					u = strtoul(h + j, 0, 10);
				while (j < n && h[j] != ';' && j - i < 10)
					j++;
				ok = j < n && h[j] == ';';
			} else {
				short e;
				for (e = 0; entities[e].name; e++) {
					long l = strlen(entities[e].name);
					if (j + l < n && !strncmp(h + j, entities[e].name, l) && h[j + l] == ';') {
						u = entities[e].u;
						j += l;
						ok = 1;
						break;
					}
				}
			}
			if (ok) {
				i = j + 1;
				if (u == 0xA0)
					u = ' ';
				k = (short)uni_to_atari(u, out);
				if (space && nl == 0)
					sb_addc(&b, ' ');
				space = 0;
				if (k) {
					quote_mark(&b, bq);
					sb_add(&b, out, k);
					nl = 0;
				}
				continue;
			}
		}
		if (!pre && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
			space = 1;
			i++;
			continue;
		}
		if (c == '\r') {		/* <pre>'s: the text's lines end '\n' */
			i++;
			continue;
		}
		if (space && nl == 0)
			sb_addc(&b, ' ');
		space = 0;
		if (c != '\n')
			quote_mark(&b, bq);
		sb_addc(&b, (char)c);
		nl = c == '\n' ? nl + 1 : 0;
		i++;
	}
	if (link)
		link_end(&b, lk, nlk, link_at, link);
	/* trim trailing blank lines */
	while (b.len && (b.s[b.len - 1] == '\n' || b.s[b.len - 1] == ' '))
		b.s[--b.len] = 0;
	if (outlen)
		*outlen = b.len;
	return sb_steal(&b);
}

/* ---------------- structure ---------------- */

typedef struct {
	const char *raw;
	MSG *m;
	SBUF text;
	short have_plain, have_html;
} WALK;

static char *decode_body(const char *body, long n, short enc, long *outlen)
{
	if (enc == ENC_BASE64)
		return base64_decode(body, n, outlen);
	if (enc == ENC_QP)
		return qp_decode(body, n, 0, outlen);
	*outlen = n;
	return str_ndup(body, n);
}

static void add_text(WALK *w, const char *body, long n, short enc, const char *charset, int html)
{
	long dl, al, tl, base;
	char *dec = decode_body(body, n, enc, &dl), *at, *txt, *s, *d;
	if (!dec)
		return;
	at = cs_to_atari(dec, dl, cs_id(charset), &al);
	free(dec);
	if (!at)
		return;
	if (html) {
		MLINK *lk = 0;
		short nlk = 0, k;
		txt = html_to_text_links(at, al, &tl, &lk, &nlk);
		free(at);
		w->m->html = 1;
		/* its links, where the text goes (it has no CRs to drop) */
		base = w->text.len + (w->text.len && tl ? 2 : 0);
		for (k = 0; k < nlk; k++) {
			MLINK *more = 0;
			if (!txt || w->m->nlinks >= MIME_MAXLINKS ||
			    (!(w->m->nlinks & 15) &&
			     !(more = realloc(w->m->links, (w->m->nlinks + 16) * sizeof(MLINK))))) {
				free(lk[k].url);
				continue;
			}
			if (more)
				w->m->links = more;
			w->m->links[w->m->nlinks] = lk[k];
			w->m->links[w->m->nlinks++].at += base;
		}
		free(lk);
	} else {
		txt = at;
		tl = al;
	}
	if (!txt)
		return;
	/* drop CRs */
	for (s = d = txt; s < txt + tl; s++)
		if (*s != '\r')
			*d++ = *s;
	tl = d - txt;
	if (w->text.len && tl)
		sb_adds(&w->text, "\n\n");
	sb_add(&w->text, txt, tl);
	free(txt);
}

static void add_part(WALK *w, const char *name, const char *type, long off, long len, short enc)
{
	MIMEPART *p;
	if (w->m->nparts >= MIME_MAXPARTS)
		return;
	p = &w->m->parts[w->m->nparts++];
	p->name = strdup(name && *name ? name : "attachment");
	p->type = strdup(type);
	p->off = off;
	p->len = len;
	p->enc = enc;
	p->size = enc == ENC_BASE64 ? len * 3 / 4 : len;
	p->summary = 0;
}

static void walk(WALK *w, long start, long end, int depth, int in_alt);

int mime_is_calendar(const MIMEPART *p)
{
	long n = (long)strlen(p->name);
	return str_istarts(p->type, "text/calendar") || str_istarts(p->type, "application/ics") ||
	       (n > 4 && str_istarts(p->name + n - 4, ".ics"));
}

/* an invitation's SUMMARY (RFC 5545: "SUMMARY;LANGUAGE=en:...", folded
   onto lines that start with a space, \, \; \n escaped), in the Atari
   character set; NULL when it has none */
static char *ics_summary(const char *raw, const MIMEPART *p)
{
	long n, i, k = 0;
	char *d = decode_body(raw + p->off, p->len, p->enc, &n), *v, *out = 0;
	if (!d)
		return 0;
	for (i = 0; i < n; i++) {
		if ((i == 0 || d[i - 1] == '\n') && n - i > 7 && str_istarts(d + i, "SUMMARY") &&
		    (d[i + 7] == ':' || d[i + 7] == ';'))
			break;
	}
	if (i < n && (v = malloc(n - i + 1)) != 0) {
		while (i < n && d[i] != ':' && d[i] != '\n')
			i++;
		for (i++; i < n; i++) {
			if (d[i] == '\r')
				continue;
			if (d[i] == '\n') {
				if (i + 1 < n && (d[i + 1] == ' ' || d[i + 1] == '\t')) {
					i++;		/* folded: goes on */
					continue;
				}
				break;
			}
			if (d[i] == '\\' && i + 1 < n) {
				i++;
				v[k++] = d[i] == 'n' || d[i] == 'N' ? ' ' : d[i];
			} else {
				v[k++] = d[i];
			}
		}
		v[k] = 0;
		if (k)
			out = cs_to_atari(v, k, CS_UTF8, 0);
		free(v);
	}
	free(d);
	return out;
}

static void walk_multipart(WALK *w, long start, long end, const char *boundary, int depth, int alt)
{
	char delim[90];
	long dl, i, part_start = -1;
	const char *r = w->raw;
	long best_s = -1, best_e = -1;
	short best_html = 2;

	snprintf(delim, sizeof(delim), "--%s", boundary);
	dl = strlen(delim);
	for (i = start; i < end; ) {
		long eol = i;
		while (eol < end && r[eol] != '\n')
			eol++;
		if (eol - i >= dl && !strncmp(r + i, delim, dl)) {
			int last = (eol - i >= dl + 2 && r[i + dl] == '-' && r[i + dl + 1] == '-');
			if (part_start >= 0) {
				long pe = i;
				/* the CRLF before the delimiter belongs to it */
				if (pe > part_start && r[pe - 1] == '\n')
					pe--;
				if (pe > part_start && r[pe - 1] == '\r')
					pe--;
				if (alt) {
					/* multipart/alternative: remember the best text part */
					long hl = mime_header_len(r + part_start, pe - part_start);
					char *ct = mime_header(r + part_start, hl, "Content-Type");
					short is_html = ct && str_istr(ct, "text/html") ? 1 : 0;
					short is_plain = !ct || str_istr(ct, "text/plain");
					short is_multi = ct && str_istr(ct, "multipart/");
					if (ct && str_istr(ct, "text/calendar"))	/* an invitation: kept, for ECal */
						walk(w, part_start, pe, depth + 1, 0);
					else if (is_multi || is_plain || is_html) {
						/* the HTML (or a multipart/related
						   around it) before the plain text, as
						   Erez chose (24 Oct 2026): its links
						   go by their names; of two alike the
						   last, the richer (RFC 2046) */
						short rank = is_plain && !is_html ? 1 : 0;
						if (rank <= best_html || best_s < 0) {
							best_html = rank;
							best_s = part_start;
							best_e = pe;
						}
					}
					free(ct);
				} else {
					walk(w, part_start, pe, depth + 1, 0);
				}
			}
			if (last) {
				part_start = -1;
				break;
			}
			part_start = eol + 1;
		}
		i = eol + 1;
	}
	if (part_start >= 0 && part_start < end && !alt)	/* unterminated last part */
		walk(w, part_start, end, depth + 1, 0);
	if (alt && best_s >= 0)
		walk(w, best_s, best_e, depth + 1, 1);
}

static void walk(WALK *w, long start, long end, int depth, int in_alt)
{
	const char *r = w->raw + start;
	long n = end - start, hl = mime_header_len(r, n);
	char *ct = mime_header(r, hl, "Content-Type");
	char *cte = mime_header(r, hl, "Content-Transfer-Encoding");
	char *cd = mime_header(r, hl, "Content-Disposition");
	char type[64] = "text/plain";
	short enc = ENC_7BIT;
	int attach;
	(void)in_alt;

	if (ct) {
		long k = strcspn(ct, "; \t\r\n");
		str_copy(type, ct, k + 1 < 64 ? k + 1 : 64);
		for (k = 0; type[k]; k++)
			type[k] = (char)tolower((u8)type[k]);
	}
	if (cte) {
		if (str_istarts(cte, "base64"))
			enc = ENC_BASE64;
		else if (str_istarts(cte, "quoted-printable"))
			enc = ENC_QP;
	}
	attach = cd && str_istarts(cd, "attachment");

	if (!strncmp(type, "multipart/", 10) && depth < 8) {
		char *bnd = mime_param(ct, "boundary");
		if (bnd) {
			walk_multipart(w, start + hl, end, bnd, depth,
				       !strcmp(type, "multipart/alternative"));
			free(bnd);
		}
	} else if (!attach && (!strcmp(type, "text/plain") || !strcmp(type, "text/html"))) {
		char *cs = mime_param(ct, "charset");
		add_text(w, r + hl, n - hl, enc, cs ? cs : "us-ascii", type[5] == 'h');
		free(cs);
	} else {
		char *name = cd ? mime_param(cd, "filename") : 0;
		if (!name && ct)
			name = mime_param(ct, "name");
		if (name && strstr(name, "=?")) {
			char *d = hdr_decode(name);
			free(name);
			name = d;
		}
		if (!name && !strcmp(type, "message/rfc822"))
			name = strdup("message.eml");
		if (!name && !strcmp(type, "text/calendar"))
			name = strdup("invite.ics");
		add_part(w, name, type, start + hl, n - hl, enc);
		free(name);
		if (w->m->nparts && mime_is_calendar(&w->m->parts[w->m->nparts - 1]))
			w->m->parts[w->m->nparts - 1].summary = ics_summary(w->raw, &w->m->parts[w->m->nparts - 1]);
	}
	free(ct);
	free(cte);
	free(cd);
}

static char *dec_hdr(const char *raw, long hl, const char *name)
{
	char *v = mime_header(raw, hl, name), *d;
	if (!v)
		return strdup("");
	d = hdr_decode(v);
	free(v);
	return d ? d : strdup("");
}

static char *raw_hdr(const char *raw, long hl, const char *name)
{
	char *v = mime_header(raw, hl, name);
	return v ? v : strdup("");
}

MSG *mime_parse(const char *raw, long len)
{
	MSG *m = calloc(1, sizeof(MSG));
	WALK w;
	long hl;
	if (!m)
		return 0;
	hl = mime_header_len(raw, len);
	m->from = dec_hdr(raw, hl, "From");
	m->to = dec_hdr(raw, hl, "To");
	m->cc = dec_hdr(raw, hl, "Cc");
	m->reply_to = dec_hdr(raw, hl, "Reply-To");
	m->subject = dec_hdr(raw, hl, "Subject");
	m->date = dec_hdr(raw, hl, "Date");
	m->message_id = raw_hdr(raw, hl, "Message-ID");
	m->references = raw_hdr(raw, hl, "References");
	m->in_reply_to = raw_hdr(raw, hl, "In-Reply-To");
	m->list_post = raw_hdr(raw, hl, "List-Post");

	w.raw = raw;
	w.m = m;
	sb_init(&w.text);
	walk(&w, 0, len, 0, 0);
	m->textlen = w.text.len;
	m->text = sb_steal(&w.text);
	/* with memory short any of these can be missing, and every caller
	   reads them: no message rather than half of one (on the Atari a
	   missing one is a read at address 0, two bombs) */
	if (!m->from || !m->to || !m->cc || !m->reply_to || !m->subject || !m->date ||
	    !m->message_id || !m->references || !m->in_reply_to || !m->list_post || !m->text) {
		mime_free(m);
		return 0;
	}
	{
		short i;
		for (i = 0; i < m->nparts; i++)
			if (!m->parts[i].name || !m->parts[i].type) {
				mime_free(m);
				return 0;
			}
	}
	return m;
}

void mime_free(MSG *m)
{
	short i;
	if (!m)
		return;
	free(m->from);
	free(m->to);
	free(m->cc);
	free(m->reply_to);
	free(m->subject);
	free(m->date);
	free(m->message_id);
	free(m->references);
	free(m->in_reply_to);
	free(m->list_post);
	free(m->text);
	for (i = 0; i < m->nlinks; i++)
		free(m->links[i].url);
	free(m->links);
	for (i = 0; i < m->nparts; i++) {
		free(m->parts[i].name);
		free(m->parts[i].type);
		free(m->parts[i].summary);
	}
	free(m);
}

char *mime_part_data(const char *raw, const MIMEPART *p, long *outlen)
{
	return decode_body(raw + p->off, p->len, p->enc, outlen);
}
