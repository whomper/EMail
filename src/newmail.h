/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * newmail.h - the new mail waiting in an account's Inbox, as EMail would
 * show it when started, for EMAILCHK.ACC (the accessory that checks for
 * EMail while it isn't running):
 *   IMAP  the unread messages that came after EMail last looked: UIDs
 *         from the Inbox's UIDNEXT in its INDEX.DAT on (the server's at
 *         the checker's first look when EMail has none);
 *   POP3  the messages on the server that EMail hasn't downloaded: not
 *         in the account's UIDL.DAT.
 * It changes nothing, here or on the server: the Inbox is EXAMINEd, only
 * flags are fetched, POP3 deletes nothing. EMail's files are read, one
 * open each, never written.
 *
 *   NEWMAIL nm[MAXACCT];          zeroed once; kept from check to check
 *   if (newmail_count(a, &nm[i], err, sizeof(err)))  nm[i].count
 *   if (newmail_fresh(&nm[i]))    mail came since the last newmail_told
 *   newmail_told(&nm[i]);         the user was told about it
 */
#ifndef NEWMAIL_H
#define NEWMAIL_H

#include "store.h"

typedef struct {
	unsigned long first;	/* IMAP: the server's UIDNEXT at the first check */
	unsigned long uidnext;	/* IMAP: the server's at the last check */
	unsigned long told_next;	/* IMAP: uidnext when the user was last told */
	long count;		/* new messages at the last check */
	long told;		/* count when the user was last told */
} NEWMAIL;

/* 1 with n->count, 0 with err (the connection, the login, the server) */
int newmail_count(ACCOUNT *a, NEWMAIL *n, char *err, int errlen);

/* 1: there is new mail, and some of it came since newmail_told */
int newmail_fresh(const NEWMAIL *n);
void newmail_told(NEWMAIL *n);

/* called with each stage's name as a check goes ("login", "fetch"...):
   EMAILCHK.ACC's AES_PROFILE build names its stretches with it */
extern void (*newmail_stage)(const char *stage);

/* EMAIL.INF read again (store_init) for the checker: the accounts it had
   freed first. workdir: EMail's folder. The number of accounts */
int newmail_load(const char *workdir);

#endif
