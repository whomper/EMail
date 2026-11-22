/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * core.c - EMail's mail core as the FireBee edition runs it (Erez, 22 Nov
 * 2026: "Make the other apps native on the FireBee too"): the same driver
 * built twice, for the build machine and, with EMAILFB.PRG's own ColdFire
 * objects (build/cf/), for qemu-m68k -cpu cfv4e. Each input, and 300
 * changed copies of it (bytes changed, cut, repeated: the same for both),
 * goes through the MIME parser (the parts, each one decoded, the text and
 * its links, the headers, HTML made text, the addresses, the date) or,
 * through common's fuzz network, the IMAP client (the login, the folders,
 * a folder's messages, a whole message). Everything they give is summed
 * (FNV-1a) into one line for each input; the two builds must give the
 * same lines (make cftest).
 *
 *     core mime|imap FILE...
 */
#include <stdlib.h>
#include <string.h>
#include "mime.h"
#include "imap.h"
#include "fuzz_net.h"

static unsigned long h;

static void sum(const void *p, long n)
{
	const unsigned char *c = p;
	while (n-- > 0) {
		h ^= *c++;
		h = (h * 16777619UL) & 0xffffffffUL;
	}
}

static void nsum(long v)
{
	unsigned char b[4];
	b[0] = (unsigned char)(v >> 24), b[1] = (unsigned char)(v >> 16);
	b[2] = (unsigned char)(v >> 8), b[3] = (unsigned char)v;
	sum(b, 4);
}

static void str(const char *s)
{
	if (s)
		sum(s, (long)strlen(s) + 1);
	else
		nsum(-1);
}

static void mime(const char *d, long n)
{
	char *raw = malloc(n + 1), name[100], email[100];
	MSG *m;
	long hl, len;
	int i;
	if (!raw)
		return;
	memcpy(raw, d, n);
	raw[n] = 0;
	m = mime_parse(raw, n);
	nsum(m != 0);
	if (m) {
		str(m->from), str(m->to), str(m->cc), str(m->reply_to), str(m->subject), str(m->date);
		str(m->message_id), str(m->references), str(m->in_reply_to), str(m->list_post);
		nsum(m->textlen), nsum(m->html), nsum(m->nparts), nsum(m->nlinks);
		if (m->text)
			sum(m->text, m->textlen);
		for (i = 0; i < m->nlinks; i++)
			nsum(m->links[i].at), nsum(m->links[i].n), str(m->links[i].url);
		for (i = 0; i < m->nparts; i++) {
			MIMEPART *p = &m->parts[i];
			char *data;
			str(p->name), str(p->type), str(p->summary), str(p->left);
			nsum(p->off), nsum(p->len), nsum(p->enc), nsum(p->size);
			nsum(mime_is_calendar(p)), nsum(mime_is_pdf(p));
			len = -1;
			data = mime_part_data(raw, p, &len);
			nsum(len);
			if (data)
				sum(data, len);
			free(data);
		}
		if (m->from) {
			addr_split(m->from, name, sizeof(name), email, sizeof(email));
			str(name), str(email);
		}
		if (m->date)
			nsum((long)mime_date(m->date));
		mime_free(m);
	}
	hl = mime_header_len(raw, n);
	nsum(hl);
	if (hl > 0) {
		char *ct = mime_header(raw, hl, "Content-Type"), *v;
		str(ct);
		if (ct) {
			str(v = mime_param(ct, "boundary")), free(v);
			str(v = mime_param(ct, "charset")), free(v);
			free(ct);
		}
	}
	{
		char *t = html_to_text(raw, n, &len);
		nsum(t != 0);
		if (t)
			nsum(len), sum(t, len);
		free(t);
	}
	{
		char *t = mime_text_utf8(raw, n, &len);
		nsum(t != 0);
		if (t)
			nsum(len), sum(t, len);
		free(t);
	}
	free(raw);
}

static void folder(void *ud, const char *name, char delim, int role, int noselect)
{
	(void)ud;
	str(name), nsum(delim), nsum(role), nsum(noselect);
}

static void fetched(void *ud, IMAPFETCH *f)
{
	(void)ud;
	nsum((long)f->seq), nsum((long)f->uid), nsum((long)f->size), nsum(f->flags);
	str(f->internaldate);
	nsum(f->hdrlen), nsum(f->bodylen);
	if (f->hdr)
		sum(f->hdr, f->hdrlen);
	if (f->body)
		sum(f->body, f->bodylen);
}

/* tests/fuzz/fuzz_imap.c's ways through the client, every answer summed */
static void imap(const unsigned char *d, long n)
{
	char err[200];
	IMAP *im;
	int sel;
	if (n < 1)
		return;
	sel = d[0];
	fuzz_net_set(d + 1, n - 1);
	im = imap_login("imap.example.org", 143, 0, "user@example.org", (sel & 0x80) ? "p\xe4ss" : "secret",
			err, sizeof(err));
	nsum(im != 0);
	if (!im) {
		str(err);
		return;
	}
	str(im->caps), nsum(im->delim);
	switch (sel & 7) {
	case 0:
		nsum(imap_list(im, folder, 0));
		break;
	case 1: {
		long m = -1, u = -1;
		nsum(imap_status(im, "INBOX", &m, &u)), nsum(m), nsum(u);
		break;
	}
	case 2: {
		const char *const boxes[] = { "INBOX", "Sent", "Archive" };
		long m[3] = { -1, -1, -1 }, u[3] = { -1, -1, -1 };
		int i;
		nsum(imap_status_many(im, boxes, 3, m, u));
		for (i = 0; i < 3; i++)
			nsum(m[i]), nsum(u[i]);
		break;
	}
	case 3: {
		int r = imap_select(im, "INBOX", 1);
		nsum(r);
		if (r > 0)
			nsum(imap_fetch(im, "1:*", "(UID FLAGS RFC822.SIZE INTERNALDATE BODY.PEEK[HEADER.FIELDS (FROM SUBJECT DATE)])",
				       fetched, 0));
		break;
	}
	case 4: {
		int r = imap_select(im, "INBOX", 0);
		nsum(r);
		if (r > 0)
			nsum(imap_fetch_seq(im, "98:100", "(UID FLAGS)", fetched, 0));
		break;
	}
	case 5: {
		char *body = 0;
		long len = 0;
		int r = imap_select(im, "INBOX", 1);
		nsum(r);
		if (r > 0) {
			nsum(r = imap_fetch_body(im, 42, &body, &len));
			if (r > 0 && body)
				nsum(len), sum(body, len);
			free(body);
		}
		break;
	}
	case 6:
		nsum(imap_append(im, "Sent", MF_SEEN, "Subject: hi\r\n\r\nbody\r\n", 22));
		nsum(imap_move(im, "1:3", "Archive"));
		break;
	default:
		nsum(imap_store(im, "1", 1, MF_SEEN | MF_FLAGGED));
		nsum(imap_expunge(im, "1"));
		nsum(imap_noop(im));
	}
	nsum((long)im->exists), nsum((long)im->uidvalidity), nsum((long)im->uidnext), str(im->selected);
	imap_logout_nowait = 1;
	imap_logout(im);
}

/* the changed copies: a small generator of its own, the same everywhere */
static unsigned long seed;

static unsigned long rnd(void)
{
	seed = (seed * 1103515245UL + 12345UL) & 0xffffffffUL;
	return seed >> 8;
}

static long change(unsigned char *t, const unsigned char *d, long n, long cap)
{
	long i, k;
	memcpy(t, d, n);
	switch (rnd() % 4) {
	case 0:				/* bytes changed */
		for (k = rnd() % 8 + 1; k-- && n;)
			t[rnd() % n] = (unsigned char)rnd();
		break;
	case 1:				/* cut short */
		if (n)
			n = rnd() % n;
		break;
	case 2:				/* a piece repeated */
		if (n) {
			long at = rnd() % n, ln = rnd() % 64 + 1;
			if (at + ln > n)
				ln = n - at;
			if (n + ln <= cap) {
				memmove(t + at + ln, t + at, n - at);
				n += ln;
			}
		}
		break;
	default:			/* a line's end taken out, or one put in */
		for (i = rnd() % (n ? n : 1); i < n; i++)
			if (t[i] == '\n' || t[i] == '\r') {
				t[i] = (rnd() & 1) ? ' ' : '\n';
				break;
			}
	}
	return n;
}

#define COPIES 300

/* each input and its copies: one line, "name sum" */
unsigned long core_file(const char *kind, const unsigned char *d, long n)
{
	unsigned char *t = malloc(n + 64 + 1);
	int k;
	h = 2166136261UL;
	seed = (unsigned long)n;
	if (!t)
		return 0;
	for (k = 0; k <= COPIES; k++) {
		long m = k ? change(t, d, n, n + 64) : (memcpy(t, d, n), n);
		nsum(m);
		if (!strcmp(kind, "imap"))
			imap(t, m);
		else
			mime((const char *)t, m);
	}
	free(t);
	return h;
}
