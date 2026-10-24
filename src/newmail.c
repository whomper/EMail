/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * newmail.c - how much new mail waits in an account's Inbox, for
 * EMAILCHK.ACC (see newmail.h).
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "plat.h"
#include "util.h"
#include "imap.h"
#include "pop3.h"
#include "store.h"
#include "newmail.h"

/* the Inbox's UIDVALIDITY and UIDNEXT when EMail last synced it: the
   first line of its INDEX.DAT ("MAILIDX version uidvalidity uidnext
   ..."), read alone. 0 when there is none */
static int inbox_seen(ACCOUNT *a, unsigned long *validity, unsigned long *next)
{
	FINFO *in;
	char dir[200], path[220], line[100];
	char *e;
	long n;
	int h;
	*validity = *next = 0;
	folders_load(a);
	in = folder_role(a, FR_INBOX);
	if (!in || in->local)
		return 0;
	folder_path(a, in, dir, sizeof(dir));
	path_join(path, sizeof(path), dir, "INDEX.DAT");
	h = pf_open(path, PF_READ);
	if (h < 0)
		return 0;
	n = pf_read(h, line, sizeof(line) - 1);
	pf_close(h);
	if (n <= 8)
		return 0;
	line[n] = 0;
	if (strncmp(line, "MAILIDX ", 8))
		return 0;
	strtoul(line + 8, &e, 10);		/* the index's version */
	*validity = strtoul(e, &e, 10);
	*next = strtoul(e, &e, 10);
	return *next > 0;
}

typedef struct {
	unsigned long base;
	long count;
} UNSEEN;

static void unseen_cb(void *ud, IMAPFETCH *f)
{
	UNSEEN *u = ud;
	if (f->uid >= u->base && !(f->flags & MF_SEEN))
		u->count++;
}

static int imap_new(ACCOUNT *a, NEWMAIL *n, char *err, int errlen)
{
	unsigned long validity, base;
	IMAP *im;
	UNSEEN u;
	int r;
	if (!inbox_seen(a, &validity, &base))
		validity = base = 0;
	im = imap_login(acct_host(a, 0), acct_port(a, 0), acct_sec(a, 0), acct_user(a, 0), acct_pass(a, 0),
			err, errlen);
	if (!im)
		return 0;
	r = imap_select(im, "INBOX", 1);
	if (r <= 0) {
		snprintf(err, errlen, "%s", im->err);
		imap_logout(im);
		return 0;
	}
	if (!n->first)
		n->first = im->uidnext;
	/* EMail's mirror is of another Inbox (renumbered), or there is none:
	   what came after the checker's first look */
	if (!base || validity != im->uidvalidity)
		base = n->first;
	n->uidnext = im->uidnext;
	u.base = base;
	u.count = 0;
	r = 1;
	if (im->exists && (!im->uidnext || im->uidnext > base)) {
		char set[24];
		snprintf(set, sizeof(set), "%lu:*", base);
		r = imap_fetch(im, set, "(FLAGS)", unseen_cb, &u);
		if (r <= 0)
			snprintf(err, errlen, "%s", im->err);
	}
	imap_logout(im);
	if (r <= 0)
		return 0;
	n->count = u.count;
	return 1;
}

typedef struct {
	const char *known;	/* "\n" UIDL.DAT "\n" */
	long count;
} POPNEW;

static void pop_cb(void *ud, long num, const char *uid, long size)
{
	POPNEW *p = ud;
	char key[100];
	snprintf(key, sizeof(key), "\n%s\n", uid);
	if (!strstr(p->known, key))
		p->count++;
}

static int pop_new(ACCOUNT *a, NEWMAIL *n, char *err, int errlen)
{
	char path[220], *k;
	SBUF kb;
	POPNEW c;
	POP3 *p;
	int r;
	p = pop3_login(acct_host(a, 0), acct_port(a, 0), acct_sec(a, 0), acct_user(a, 0), acct_pass(a, 0),
		       err, errlen);
	if (!p)
		return 0;
	path_join(path, sizeof(path), a->dir, "UIDL.DAT");
	sb_init(&kb);
	sb_adds(&kb, "\n");
	if ((k = pf_load(path, 0)) != 0) {
		sb_adds(&kb, k);
		free(k);
	}
	sb_adds(&kb, "\n");
	c.known = kb.s ? kb.s : "\n\n";
	c.count = 0;
	r = pop3_list(p, pop_cb, &c);
	if (r <= 0)
		snprintf(err, errlen, "%s", p->err);
	pop3_quit(p);
	sb_free(&kb);
	if (r <= 0)
		return 0;
	n->count = c.count;
	return 1;
}

int newmail_count(ACCOUNT *a, NEWMAIL *n, char *err, int errlen)
{
	int r;
	err[0] = 0;
	r = a->pop ? pop_new(a, n, err, errlen) : imap_new(a, n, err, errlen);
	if (r && !n->count)
		n->told = 0;		/* the next one is news again */
	return r;
}

int newmail_fresh(const NEWMAIL *n)
{
	if (n->count <= 0)
		return 0;
	return n->count > n->told || n->uidnext > n->told_next;
}

void newmail_told(NEWMAIL *n)
{
	n->told = n->count;
	n->told_next = n->uidnext;
}

int newmail_load(const char *workdir)
{
	while (naccts > 0) {
		ACCOUNT *a = accts[--naccts];
		accts[naccts] = 0;
		free(a);
	}
	return store_init(workdir);
}
