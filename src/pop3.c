/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pop3.c - POP3 client.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pop3.h"
#include "charset.h"
#include "store.h"

static int reply(POP3 *p)
{
	const char *s;
	if (conn_getline(p->c, &p->line) < 0) {
		str_copy(p->err, p->c->err, sizeof(p->err));
		return -1;
	}
	s = p->line.s;
	if (!strncmp(s, "+OK", 3))
		return 1;
	if (!strncmp(s, "-ERR", 4))
		for (s += 4; *s == ' '; s++)
			;
	str_copy(p->err, s, sizeof(p->err));
	return 0;
}

static POP3 *login_once(const char *host, unsigned short port, int sec, const char *user,
			const char *pass, char *err, int errlen)
{
	POP3 *p = calloc(1, sizeof(POP3));
	int r, google;
	if (!p) {
		str_copy(err, "out of memory", errlen);
		return 0;
	}
	sb_init(&p->line);
	p->c = conn_open("POP3", host, port, 0, sec, err, errlen);
	if (!p->c) {
		free(p);
		return 0;
	}
	r = reply(p);
	if (r <= 0) {
		if (r < 0)			/* stopped, timed out, closed */
			snprintf(err, errlen, "%s: %s", host, p->err);
		else
			snprintf(err, errlen, "%s did not greet as a POP3 server", host);
		pop3_quit(p);
		return 0;
	}
	google = google_said(p->line.s);	/* "+OK Gpop ready" */
	if (sec == SEC_STARTTLS) {
		conn_cmd(p->c, 0, "STLS");
		if (reply(p) <= 0) {
			snprintf(err, errlen, "%s doesn't offer STLS: use port 995", host);
			pop3_quit(p);
			return 0;
		}
		if (!conn_starttls(p->c, host, err, errlen)) {
			conn_close(p->c);
			p->c = 0;
			pop3_quit(p);
			return 0;
		}
	}
	conn_log_login("POP3", user, pass);
	conn_cmd(p->c, 1, "USER %s", user);
	r = reply(p);
	if (r > 0) {
		conn_cmd(p->c, 1, "PASS %s", pass);
		r = reply(p);
	} else if (r == 0) {
		/* some servers only take SASL PLAIN: they refuse USER. A refused
		   PASS is the answer: GMX hangs up after it, Yahoo stops answering */
		char raw[300], *b64;
		long ul = strlen(user), pl = strlen(pass);
		if (ul + pl + 2 <= (long)sizeof(raw)) {
			raw[0] = 0;
			memcpy(raw + 1, user, ul + 1);
			memcpy(raw + 2 + ul, pass, pl);
			b64 = base64_encode(raw, ul + pl + 2, 0, 0);
			conn_cmd(p->c, 1, "AUTH PLAIN %s", b64);
			free(b64);
			r = reply(p);
		}
	}
	if (r <= 0 && (google || google_said(p->err))) {
		str_copy(err, google_app_password, errlen);
		pop3_quit(p);
		return 0;
	}
	if (r <= 0) {
		snprintf(err, errlen, "login failed: %s", p->err);
		pop3_quit(p);
		return 0;
	}
	return p;
}

POP3 *pop3_login(const char *host, unsigned short port, int sec, const char *user,
		 const char *pass, char *err, int errlen)
{
	POP3 *p = login_once(host, port, sec, user, pass, err, errlen);
	{
		int tries = 0;	/* ChaCha20 alone, then ECDSA */
		while (!p && sec == SEC_STARTTLS && conn_tls_retry && tries++ < 2)
			p = login_once(host, port, sec, user, pass, err, errlen);
	}
	return p;
}

void pop3_quit(POP3 *p)
{
	if (!p)
		return;
	if (p->c) {
		conn_cmd(p->c, 0, "QUIT");
		p->c->timeout_ms = 3000;
		reply(p);
		conn_close(p->c);
	}
	sb_free(&p->line);
	free(p);
}

int pop3_stat(POP3 *p, long *count, long *size)
{
	char *e;
	int r;
	conn_cmd(p->c, 0, "STAT");
	r = reply(p);
	if (r <= 0)
		return r;
	*count = strtol(p->line.s + 3, &e, 10);
	*size = strtol(e, 0, 10);
	/* the count sizes pop3_list's tables: one the Atari can't hold (or a
	   negative one) is refused, never multiplied past 32 bits */
	if (*count < 0 || *count > POP3_MAXMSG) {
		snprintf(p->err, sizeof(p->err), "the server says it has %ld messages: too many for EMail", *count);
		return -1;
	}
	return 1;
}

/* read a multi-line answer; cb gets each line (dot-unstuffed) */
static int multiline(POP3 *p, void (*cb)(void *ud, const char *s, long n), void *ud)
{
	for (;;) {
		const char *s;
		if (conn_getline(p->c, &p->line) < 0) {
			str_copy(p->err, p->c->err, sizeof(p->err));
			return -1;
		}
		s = p->line.s;
		if (s[0] == '.') {
			if (!s[1])
				return 1;
			s++;
		}
		cb(ud, s, p->line.len - (s - p->line.s));
	}
}

typedef struct {
	long *sizes;
	long nsizes;
	long *num;
	char (*uid)[72];
	long n;
} LISTUD;

static void size_line(void *ud, const char *s, long n)
{
	LISTUD *l = ud;
	long num = strtol(s, 0, 10);
	const char *sp = strchr(s, ' ');
	(void)n;
	if (num > 0 && num <= l->nsizes && sp)
		l->sizes[num - 1] = strtol(sp + 1, 0, 10);
}

static void uidl_line(void *ud, const char *s, long n)
{
	LISTUD *l = ud;
	long num = strtol(s, 0, 10);
	const char *sp = strchr(s, ' ');
	(void)n;
	if (num <= 0 || !sp || l->n >= l->nsizes)
		return;
	l->num[l->n] = num;
	str_copy(l->uid[l->n], sp + 1, sizeof(l->uid[0]));
	l->n++;
}

/* LIST and UIDL are read completely before cb runs: cb may RETR */
int pop3_list(POP3 *p, void (*cb)(void *, long, const char *, long), void *ud)
{
	LISTUD l;
	long count, size, i;
	int r = pop3_stat(p, &count, &size);
	if (r <= 0)
		return r;
	memset(&l, 0, sizeof(l));
	l.nsizes = count;
	l.sizes = calloc(count + 1, sizeof(long));
	l.num = calloc(count + 1, sizeof(long));
	l.uid = calloc(count + 1, sizeof(l.uid[0]));
	if (!l.sizes || !l.num || !l.uid) {
		free(l.sizes);
		free(l.num);
		free(l.uid);
		str_copy(p->err, "out of memory", sizeof(p->err));
		return -1;
	}
	conn_cmd(p->c, 0, "LIST");
	if ((r = reply(p)) > 0)
		r = multiline(p, size_line, &l);
	if (r > 0) {
		conn_cmd(p->c, 0, "UIDL");
		if ((r = reply(p)) > 0) {
			r = multiline(p, uidl_line, &l);
		} else if (r == 0) {
			/* no UIDL: fall back to message numbers */
			for (i = 1; i <= count; i++) {
				l.num[l.n] = i;
				snprintf(l.uid[l.n++], sizeof(l.uid[0]), "n%ld", i);
			}
			r = 1;
		}
	}
	if (r > 0)
		for (i = 0; i < l.n; i++)
			cb(ud, l.num[i], l.uid[i], l.num[i] <= count ? l.sizes[l.num[i] - 1] : 0);
	free(l.sizes);
	free(l.num);
	free(l.uid);
	return r;
}

static void add_line(void *ud, const char *s, long n)
{
	SBUF *b = ud;
	sb_add(b, s, n);
	sb_add(b, "\r\n", 2);
}

int pop3_retr(POP3 *p, long num, SBUF *out)
{
	int r;
	conn_cmd(p->c, 0, "RETR %ld", num);
	r = reply(p);
	if (r <= 0)
		return r;
	return multiline(p, add_line, out);
}

int pop3_dele(POP3 *p, long num)
{
	conn_cmd(p->c, 0, "DELE %ld", num);
	return reply(p);
}
