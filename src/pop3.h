/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pop3.h - POP3 client (RFC 1939) with UIDL, for accounts that download
 * mail into local folders.
 */
#ifndef POP3_H
#define POP3_H

#include "conn.h"

typedef struct {
	CONN *c;
	char err[200];
	SBUF line;
} POP3;

POP3 *pop3_login(const char *host, unsigned short port, int sec, const char *user,
		 const char *pass, char *err, int errlen);
void  pop3_quit(POP3 *p);
#define POP3_MAXMSG 65535L	/* more on the server: refused (memory) */
int   pop3_stat(POP3 *p, long *count, long *size);
/* cb(number, unique id, size) for each message on the server */
int   pop3_list(POP3 *p, void (*cb)(void *ud, long num, const char *uid, long size), void *ud);
int   pop3_retr(POP3 *p, long num, SBUF *out);	/* whole message, CRLF lines */
/* the header and the first lines of the body, at most cap bytes kept;
   *cut when some were dropped */
int   pop3_top(POP3 *p, long num, long lines, long cap, SBUF *out, int *cut);
int   pop3_dele(POP3 *p, long num);

#endif
