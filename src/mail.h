/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * mail.h - what the user can do, independent of the GEM interface:
 * check mail, mirror IMAP folders, read, flag, delete, move, send.
 */
#ifndef MAIL_H
#define MAIL_H

#include "store.h"

extern char mail_err[200];
/* the last call failed because EMail works offline or the account is not
   connected (see ACCOUNT.cut), not because of the message or folder */
extern int mail_unreachable;
/* the UI shows progress here (may be NULL) */
extern void (*mail_status)(const char *msg);

int  mail_connect(ACCOUNT *a);			/* IMAP: log in or reuse */
void mail_disconnect(ACCOUNT *a);
void mail_disconnect_all(void);

int  mail_refresh_folders(ACCOUNT *a);		/* IMAP LIST */
int  mail_sync_folder(ACCOUNT *a, FINFO *fi, long *newmsgs);
/* mirror opt.page more (older) messages of an IMAP folder */
int  mail_load_more(ACCOUNT *a, FINFO *fi, long *added);
/* inbox (IMAP sync or POP3 download), then send the Outbox */
int  mail_check(ACCOUNT *a, long *newmsgs);
/* of those new messages, how many outside the Inbox, and where: the
   first such folder's name, and in how many folders */
extern long mail_new_other;
extern char mail_new_where[64];
extern short mail_new_folders;

/* the raw message: from the cache, else downloaded; marks it read */
char *mail_fetch(FOLDER *f, HDR *h, long *len);
/* the message into the cache, from the server (IMAP), without marking it
   read; again: even if it is there, as it may have come wrong; what: the
   progress line's label ("Inbox: downloading 3 of 40"), or 0 */
int  mail_download(FOLDER *f, HDR *h, int again, const char *what);
/* mirror every message of an IMAP folder from now on, not opt.page more
   at a time (their headers; the bodies are mail_download's) */
int  mail_mirror_all(ACCOUNT *a, FINFO *fi);
int  mail_flag(FOLDER *f, HDR *h, unsigned short flags, int add);
/* read/flag changes not yet on the server, and sending them (one STORE
   per flag); 1 when sent or when they must wait for a connection */
int  mail_flags_pending(FOLDER *f);
int  mail_push_flags(FOLDER *f);
int  mail_delete(FOLDER *f, HDR *h);
int  mail_mark_all_read(ACCOUNT *a, FINFO *fi, FOLDER *open);	/* the whole folder, on the server too; open: the
   folder if the screen has it open (it is changed itself), or NULL */
int  mail_move(FOLDER *f, HDR *h, FINFO *dest);

int  mail_queue(ACCOUNT *a, const char *raw, long len);	/* into the Outbox */
int  mail_send_outbox(ACCOUNT *a, int *sent);
/* raw at once, straight from memory, with what waits in the Outbox:
   1, or -1 when raw did not go (mail_err says why: queue it), -2 when
   it went but one from the Outbox did not */
int  mail_send(ACCOUNT *a, const char *raw, long len, int *sent);

int  mail_folder_create(ACCOUNT *a, const char *name);
/* a new name (Atari text) under the same parent; newserver gets its name
   on the server */
int  mail_folder_rename(ACCOUNT *a, FINFO *fi, const char *name, char *newserver, int size);	/* Atari charset */
int  mail_folder_delete(ACCOUNT *a, FINFO *fi);

#endif
