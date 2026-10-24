/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * plat.h - what the mail core needs from the machine: files, folders,
 * the clock, a millisecond timer and TCP. Implemented for TOS/GEM in
 * atari/ and for Linux in host/ (used by the tests and mail-cli).
 */
#ifndef PLAT_H
#define PLAT_H

#include <stddef.h>

#ifdef MAIL_TOS
#define PF_SEP '\\'
#else
#define PF_SEP '/'
#endif

/* ---- files ---- */
#define PF_READ   0
#define PF_WRITE  1	/* create or truncate */
#define PF_APPEND 2
#define PF_UPDATE 3	/* an old file written over in place */

int  pf_open(const char *path, int mode);	/* handle >= 0, < 0 on error */
long pf_read(int h, void *buf, long n);
long pf_write(int h, const void *buf, long n);
void pf_close(int h);
long pf_seek(int h, long off);			/* from the start; new position or < 0 */
long pf_size(const char *path);			/* -1 if missing */
unsigned long pf_info(const char *path, long *size);	/* date and time, 0 if missing; *size, -1 */
int  pf_exists(const char *path);
int  pf_mkdir(const char *path);		/* 1 made, 0 already there, < 0 error */
int  pf_remove(const char *path);
int  pf_rename(const char *from, const char *to);
/* the Atari: the piece of ST RAM that reads and writes to TT RAM go
   through, taken now (an accessory with its heap in TT RAM, before its
   first event call); otherwise it is taken at the first such read */
void pf_bounce_init(void);
/* call cb for every plain file in dir; stop early if cb returns non-zero */
int  pf_list(const char *dir, int (*cb)(const char *name, long size, void *ud), void *ud);

/* whole-file helpers: pf_load returns a malloc'ed NUL-terminated copy */
char *pf_load(const char *path, long *len);
int   pf_save(const char *path, const void *data, long len);
/* an open file's size, the position left at the start, or < 0: with it
   pf_load opens first and looks at the folder only when that fails */
extern long (*pf_size_open)(int h);
long pf_handle_size(int h);

/* a bulk job's memory (guidelines 6.5): TT RAM when there is some, else a
   share of what ST RAM has free; 0 when there is too little. *got: its size */
void *pf_bulk(long *got);
void  pf_bulk_free(void *p);

/* ---- time ---- */
typedef struct {
	short year, mon, day, hour, min, sec;
} PFTIME;

void pf_now(PFTIME *t);		/* local wall clock */
/* a variable from the program's environment (TZ under MiNT or MagiC),
   or NULL */
const char *pf_getenv(const char *name);
/* the memory EMail runs in, for the About box: where ("Running in TT
   RAM", "Program in ST RAM, data in TT RAM") and what is free */
void pf_memory(char *where, int wsize, char *avail, int asize);
unsigned long pf_ms(void);	/* monotonic milliseconds (5 ms steps on TOS) */
void pf_debug(const char *s);	/* emulator console / stderr */
/* raw unpredictable bytes (timer jitter on the Atari, /dev/urandom on
   Linux); tls.c hashes them with a seed kept on disk */
void pf_entropy(unsigned char *buf, int n);

/* ---- TCP ---- */
int  net_init(void);			/* 1 if a TCP/IP stack is present */
const char *net_stack(void);		/* "STinG", "MiNTnet", "POSIX" ... */
/* connect (resolving names); handle >= 0, or < 0 with err filled in */
int  net_open(const char *host, unsigned short port, char *err, int errlen);
long net_write(int h, const void *buf, long n);	/* bytes sent or < 0 */
/* non-blocking: > 0 bytes read, 0 nothing yet, < 0 closed or failed */
long net_read(int h, void *buf, long max);
void net_close(int h);
/* close once what was sent has gone out, waiting at most this long */
void net_close_wait(int h, int seconds);

/* the UI can show progress and keep the screen alive while we wait */
extern void (*pf_idle)(void);

#endif
