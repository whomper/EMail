/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * imap.h - IMAP4rev1 client (RFC 3501) with the extensions a modern
 * server offers: UIDPLUS, MOVE, SPECIAL-USE, NAMESPACE.
 */
#ifndef IMAP_H
#define IMAP_H

#include "conn.h"

/* message flags (also used by the local cache) */
#define MF_SEEN      0x0001
#define MF_ANSWERED  0x0002
#define MF_FLAGGED   0x0004
#define MF_DELETED   0x0008
#define MF_DRAFT     0x0010
#define MF_FORWARDED 0x0020
#define MF_JUNK      0x0040
#define MF_ATTACH    0x0080	/* multipart/mixed: probably has attachments */
#define MF_CACHED    0x0100	/* body is in the local cache */
#define MF_FLAGSYNC  0x0200	/* read/flagged changed offline: tell the server */

/* folder roles */
#define FR_INBOX  1
#define FR_SENT   2
#define FR_TRASH  3
#define FR_DRAFTS 4
#define FR_JUNK   5
#define FR_ARCHIVE 6
#define FR_OUTBOX 7	/* local only */

typedef struct {
	CONN *c;
	unsigned short tag;
	char caps[600];
	char err[200];
	char delim;
	unsigned long exists, uidvalidity, uidnext;
	char selected[200];
	short readonly;
	SBUF resp;		/* the current response, literals inlined */
} IMAP;

typedef struct {
	unsigned long seq, uid, size;
	unsigned short flags;
	char internaldate[40];
	const char *hdr;	/* BODY[HEADER...] contents */
	long hdrlen;
	const char *body;	/* BODY[] contents */
	long bodylen;
} IMAPFETCH;

IMAP *imap_login(const char *host, unsigned short port, int sec, const char *user,
		 const char *pass, char *err, int errlen);
void  imap_logout(IMAP *im);
extern int imap_logout_nowait;	/* quitting: say LOGOUT, don't wait for the answer */
int   imap_has(IMAP *im, const char *cap);

/* LIST "" "*": cb(name as on the server, delimiter, role, noselect) */
int imap_list(IMAP *im, void (*cb)(void *ud, const char *name, char delim, int role, int noselect), void *ud);
int imap_select(IMAP *im, const char *mbox, int readonly);
/* UID FETCH set items; cb for every FETCH response */
int imap_fetch(IMAP *im, const char *uidset, const char *items,
	       void (*cb)(void *ud, IMAPFETCH *f), void *ud);
/* the same by message sequence numbers ("98:100"), for the newest N */
int imap_fetch_seq(IMAP *im, const char *seqset, const char *items,
		   void (*cb)(void *ud, IMAPFETCH *f), void *ud);
/* message and unseen counts of a mailbox without selecting it */
int imap_status(IMAP *im, const char *mbox, long *messages, long *unseen);
/* a message's parts, from its BODYSTRUCTURE: the leaves, in order, with
   their section ("1", "2.1"); of a multipart/alternative only the part
   EMail shows (the HTML or a multipart/related around it, else the plain
   text: decided 24 Oct 2026), so the others are neither shown nor listed */
typedef struct {
	char sec[16];		/* "1", "2.1" */
	char type[48];		/* "text/plain", lower case */
	char name[64];		/* the file name (filename= or name=), as sent */
	char charset[24];
	char enc[20];		/* "base64", "quoted-printable", "7bit"... */
	long size;		/* encoded bytes, as the server counts them */
	short attach;		/* Content-Disposition: attachment */
} IMAPPART;
#define IMAP_MAXPARTS 32
/* 1 and *n set; 0/-1 as run() */
int imap_bodystructure(IMAP *im, unsigned long uid, IMAPPART *parts, int max, int *n);
/* the header (BODY.PEEK[HEADER]) and the sections secs[0..n), each at most
   cap bytes (BODY.PEEK[s]<0.cap>), in one command: out[0] the header,
   out[1..n] the sections, malloc'ed (0: none came), len[] their sizes */
int imap_fetch_sections(IMAP *im, unsigned long uid, const char *const *secs, int n, long cap,
			char **out, long *len);
/* one section streamed: put(ud, bytes, n) for each piece as it comes,
   never all of it in memory (an attachment left on the server); put
   returns 0 to give up (the rest is read and dropped) */
int imap_fetch_stream(IMAP *im, unsigned long uid, const char *sec,
		      int (*put)(void *ud, const char *s, long n), void *ud);
/* a whole message (BODY.PEEK[]), malloc'ed; *body 0 when it is gone */
int imap_fetch_body(IMAP *im, unsigned long uid, char **body, long *len);
/* the same for n folders at once: the questions go out together and the
   answers come back in one wait. A folder the server refuses stays -1;
   -1 when the connection fails */
int imap_status_many(IMAP *im, const char *const *mboxes, int n, long *messages, long *unseen);
int imap_store(IMAP *im, const char *uidset, int add, unsigned short flags);
int imap_store_seq(IMAP *im, const char *seqset, int add, unsigned short flags);
int imap_copy(IMAP *im, const char *uidset, const char *dest);
/* UID SEARCH TEXT: the UIDs of the selected folder's messages with the
   text (UTF-8) in their headers or body; *uids is the caller's to free */
int imap_search(IMAP *im, const char *utf8, unsigned long **uids, long *n);
int imap_move(IMAP *im, const char *uidset, const char *dest);	/* MOVE or COPY+delete */
int imap_expunge(IMAP *im, const char *uidset);
int imap_append(IMAP *im, const char *mbox, unsigned short flags, const char *data, long len);
int imap_create(IMAP *im, const char *mbox);
int imap_delete(IMAP *im, const char *mbox);
int imap_rename(IMAP *im, const char *from, const char *to);
int imap_noop(IMAP *im);

/* "\"name\"" with escapes, into out */
void imap_quote(char *out, int size, const char *s);
unsigned short imap_parse_flags(const char *list, long n);

#endif
