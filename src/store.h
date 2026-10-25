/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * store.h - EMail's data on disk: settings and accounts (EMAIL.INF),
 * the folder list of each account, and per folder a header index plus
 * cached message files. Like the GFA Troll, IMAP folders are a mirror
 * of the server (headers always, bodies once read); POP3 accounts keep
 * their mail in local folders. All names are 8.3 so plain TOS copes:
 *
 *   EMAIL.INF                  settings and accounts
 *   ADDRESS.TXT                address book, one "Name <a@b>" per line
 *   EMAIL\ACCT1\FOLDERS.LST     folders of account 1
 *   EMAIL\ACCT1\F1A2B3C4\INDEX.DAT     headers of one folder, with
 *                                       each body's place in the pack
 *   EMAIL\ACCT1\F1A2B3C4\MESSAGES.DAT  the folder's message bodies
 */
#ifndef STORE_H
#define STORE_H

#include "imap.h"

#define MAXACCT   8
#define MAXFOLDER 64

typedef struct {
	char server[160];	/* name on the IMAP server (modified UTF-7) */
	char disp[64];		/* shown name, Atari charset, without parents */
	char dir[12];		/* directory under the account */
	short role;		/* FR_* */
	short noselect;
	short depth;		/* hierarchy level for indenting */
	short local;		/* local-only folder (Outbox, POP Inbox...) */
	long total, unread;
	unsigned long synced;	/* runtime: pf_ms() of the last sync, 0 never */
	short opened;		/* runtime: its directory was used this session */
} FINFO;

typedef struct {
	char name[32];		/* account label */
	char fullname[64];	/* your name in From: (Atari charset) */
	char email[96];
	short pop;		/* 0 IMAP, 1 POP3 */
	char host[64];
	unsigned short port;
	char user[96], pass[64];
	short leave;		/* POP3: keep mail on the server */
	char smtphost[64];
	unsigned short smtpport;
	char smtpuser[96], smtppass[64];	/* empty user: same as above; "-": no login */
	char sentname[96];	/* IMAP Sent folder override */
	/* Direct connection: the provider's own servers, reached with TLS */
	char dhost[64];
	unsigned short dport;
	char dsmtphost[64];
	unsigned short dsmtpport;
	short dsec, dsmtpsec;	/* 0: by the port (993/995/465 TLS, else STARTTLS) */
	/* the Direct connection's own logins, kept apart from the gateway's */
	char duser[96], dpass[64];
	char dsmtpuser[96], dsmtppass[64];

	/* runtime */
	short id;		/* 1..MAXACCT */
	char dir[200];
	short nfolders;
	FINFO folders[MAXFOLDER];
	IMAP *im;		/* kept open while working, like the GFA Troll */
	unsigned long im_used;
	/* the last connection failed: work from the cache until Check mail
	   (or the timed check) connects again */
	short cut;
	char cuterr[160];
	short listed;		/* the folder list was read from the server this session */
	short fdirty;		/* the folder list changed since it was saved */
} ACCOUNT;

/* where a screen font keeps the Hebrew letters */
#define HEB_ATARI 0		/* the Atari character set, 0xC2-0xDC (TOS, EmuTOS) */
#define HEB_ISO   1		/* ISO-8859-8 / windows-1255 places, 0xE0-0xFA */
#define HEB_DOS   2		/* DOS code page 862, 0x80-0x9A */

typedef struct {
	short tz;		/* minutes east of UTC */
	short check;		/* minutes between automatic checks, 0 = off */
	short page;		/* messages loaded at a time (more on request) */
	short keepcache;	/* keep message bodies on disk when quitting */
	short log;		/* write EMAIL.LOG */
	short hebrew;		/* Hebrew keyboard on at start */
	short offline;
	short wrap;		/* compose wrap column */
	short main_x, main_y, main_w, main_h;	/* main window, 0 = not yet placed */
	short pane_w, pane_h;	/* folders pane width, message list height (pixels) */
	short ed_x, ed_y, ed_w, ed_h;		/* the editor window */
	short font_id, font_pt;	/* text font: VDI font id (1 = system) and size */
	short hebfont;		/* where the font has its Hebrew letters: HEB_* */
	short falcon;		/* Direct connection: TLS on the Atari (and its DSP), no gateway */
	short dsp;		/* Direct connection may use the DSP (else the 68030 does it all) */
	char tzname[40];	/* the time zone, an IANA name ("Asia/Jerusalem") */
	short colour;		/* colours, on screens with 16 or more */
	char signature[256];	/* below new messages, every account's; \n separated, Atari charset */
	char workdir[200];
	short peoplefile;	/* EContact not loaded: names from its CONTACTS.TXT (guidelines 4.1) */
	char peopledir[200];	/* EContact's folder, where CONTACTS.TXT is */
	char browser[200];	/* EWeb's program, started on a link when no browser runs */
	short dialogs3d;	/* dialogs in the AES's 3D look, where it has one (guidelines 5.6) */
} OPTIONS;

typedef struct {
	unsigned long uid;
	unsigned short flags;
	long size;
	unsigned long date;	/* seconds since 1970, UTC */
	char *from, *subject, *to;
	long off, plen;		/* the body's record in MESSAGES.DAT; off -1: none */
} HDR;

#define PACK_HEAD 10		/* a record: "EM", the UID, the length, the message */
#define MF_FLAGSCAN 0x4000	/* runtime, while pack_scan looks for its record */

typedef struct {
	ACCOUNT *acct;
	FINFO *fi;
	char dir[200];
	unsigned long uidvalidity, uidnext;
	long window;		/* IMAP: how many of the newest messages to mirror */
#define MIRROR_ALL 0x7FFFFFFFL	/* window: all of them (Download all messages) */
	long exists;		/* IMAP: messages in the folder on the server */
	HDR *h;
	long n, cap;
	short dirty;
	short refs;		/* the places it is open (one FOLDER per folder) */
	unsigned gen;		/* counts changes to h[], for views of it */
	long gaps;		/* bytes of dropped records in the pack */
	long pend_base;		/* where the pack ends: the waiting records go there */
	char *pend;		/* bodies waiting to be appended (pf_bulk) */
	long pend_len, pend_cap;
	short bulk;		/* a local folder's job that flushes itself (POP) */
} FOLDER;

extern OPTIONS opt;
extern ACCOUNT *accts[MAXACCT];
extern short naccts;

/* settings */
/* the time zone: minutes east of UTC now, or at a moment (seconds since
   1970, UTC), summer time included */
int  zone_offset_now(void);
int  zone_offset_at(unsigned long utc);	/* at a UTC time in seconds */
long zone_utc_now(void);		/* minutes since 1970, UTC */
int  store_init(const char *workdir);	/* loads EMAIL.INF (creates dirs) */
/* store_init in steps, for work in the background (EMAILCHK.ACC: the
   guidelines, 2.2): store_begin reads the file (0: there is none), each
   store_step takes a few of its lines, 1 while there are more */
int  store_begin(const char *workdir);
int  store_step(void);
int  store_step_big(void);	/* the next step makes an account: a long one */
extern short store_inf_accounts;	/* the [account]s in it, those that didn't fit too */
int  store_save_settings(void);
/* EMAIL.INF had passwords that EMAIL.KEY can't open (a lost or new key):
   they are empty and must be typed again */
extern int store_lost_passwords;
extern int store_plain_passwords;	/* EMAIL.INF still has some as typed */
ACCOUNT *acct_new(void);
/* the servers in use: the gateway's (plain), or in a Direct connection the
   provider's (TLS). smtp: 0 incoming, 1 outgoing */
const char *acct_host(ACCOUNT *a, int smtp);
unsigned short acct_port(ACCOUNT *a, int smtp);
int acct_sec(ACCOUNT *a, int smtp);		/* SEC_* (conn.h) */
/* fill in a provider's direct servers from the e-mail address; 1 if known */
int acct_preset(ACCOUNT *a);
/* a login Google refused: its servers known by what they said (a greeting,
   the refusal), then the message that says Gmail takes an app password */
int google_said(const char *s);
extern const char google_app_password[];
/* the login in use: the gateway's or, in Direct connection, the Direct connection's own.
   smtp: 0 incoming, 1 outgoing; NULL user: the server needs no login */
const char *acct_user(ACCOUNT *a, int smtp);
const char *acct_pass(ACCOUNT *a, int smtp);
/* a login set never filled in starts as a copy of the other one */
void acct_fill_logins(ACCOUNT *a);
void acct_delete(ACCOUNT *a);
void acct_dirs(ACCOUNT *a);

/* folders of an account */
int  folders_load(ACCOUNT *a);
int  folders_save(ACCOUNT *a);
void folders_touch(ACCOUNT *a);		/* changed: saved by folders_flush */
int  folders_flush(ACCOUNT *a);		/* saved if it changed */
FINFO *folder_find(ACCOUNT *a, const char *server);
FINFO *folder_role(ACCOUNT *a, short role);
FINFO *folder_add(ACCOUNT *a, const char *server, char delim, short role, short noselect, short local);
/* a new name on the server for a folder; its cache directory stays */
void folder_set_server(FINFO *f, const char *server, char delim);
void folder_path(ACCOUNT *a, FINFO *f, char *out, int size);

/* header index of one folder. A folder open already is shared: the same
   FOLDER, with one more to close; each close saves what changed */
FOLDER *fold_open(ACCOUNT *a, FINFO *fi);
int  fold_save(FOLDER *f);
int  fold_done(FOLDER *f);		/* the end of a job: saved if anything changed */
void fold_close(FOLDER *f);
HDR *fold_add(FOLDER *f, unsigned long uid);	/* keeps uid order */
HDR *fold_get(FOLDER *f, unsigned long uid);
void fold_remove(FOLDER *f, unsigned long uid);	/* also its cached body */
void fold_clear(FOLDER *f);
void hdr_set(HDR *h, const char *from, const char *subject, const char *to);
void fold_count(FOLDER *f);		/* local folders: updates fi->total / unread */
/* fill a header from the raw header block of a message */
void hdr_from_raw(HDR *h, const char *raw, long len);

/* message bodies, in the folder's pack. msg_save keeps one from the
   server in memory until the folder is saved; a local folder's goes to
   disk at once. msg_load checks the record: NULL if it isn't there or
   isn't right (an IMAP folder fetches it again) */
void msg_path(FOLDER *f, unsigned long uid, char *out, int size);	/* the old .EML */
int  msg_save(FOLDER *f, unsigned long uid, const char *data, long len);
char *msg_load(FOLDER *f, unsigned long uid, long *len);
int  msg_export(FOLDER *f, unsigned long uid, const char *path);	/* one body as a file */
int  pack_flush(FOLDER *f);		/* what waits in memory, to disk now */
void body_drop(FOLDER *f, HDR *h);	/* the body forgotten: fetched again when wanted */
/* bodies of IMAP folders (quit without keepcache); each(): the folder being cleared, or 0 */
void cache_clear_all(void (*each)(const char *account, const char *folder), int all);
/* all = 0: only the folders opened this session (on quit: the others
   were cleared at the end of an earlier one and their directories are
   left alone, see fold_open) */

/* address book */
int  abook_add(const char *addr);	/* "Name <a@b>", skips duplicates */
int  abook_add_list(char addr[][160], int n);	/* the same, the book read once */
char *abook_load(long *len);

/* dates for display, in the configured time zone */
void date_str(unsigned long t, char *out, int size, int with_year);

/* 0 on an Atari that can't make a Direct connection (a 68000): FALCON=1
   is then kept in EMAIL.INF but the gateway is used (store.c) */
extern int falcon_usable;

#endif
