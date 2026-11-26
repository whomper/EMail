/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* stubs_cf.c - what core.c's ColdFire build has no use for: no files (the
 * driver reads its own through Linux), no clock but a count, and no TLS
 * (the test's connections are plain, common's fuzz network) */
#include "plat.h"
#include "tls.h"

int pf_open(const char *path, int mode) { (void)path, (void)mode; return -1; }
long pf_read(int h, void *buf, long n) { (void)h, (void)buf, (void)n; return -1; }
long pf_write(int h, const void *buf, long n) { (void)h, (void)buf; return n; }
void pf_close(int h) { (void)h; }
long pf_seek(int h, long off) { (void)h, (void)off; return -1; }
long pf_size(const char *path) { (void)path; return -1; }
int pf_remove(const char *path) { (void)path; return -1; }
int pf_rename(const char *from, const char *to) { (void)from, (void)to; return -1; }
void pf_now(PFTIME *t) { (void)t; }
static unsigned long ms;
unsigned long pf_ms(void) { return ms += 5; }
void (*pf_idle)(void);

int tls_load_anchors(const char *path, char *err, int errlen) { (void)path, (void)err, (void)errlen; return 0; }
int tls_anchor_count(void) { return 0; }
TLS *tls_start(int sock, const char *host, int flags, int *retry, char *err, int errlen)
{ (void)sock, (void)host, (void)flags, (void)retry, (void)err, (void)errlen; return 0; }
long tls_read(TLS *t, void *buf, long max) { (void)t, (void)buf, (void)max; return -1; }
long tls_write(TLS *t, const void *buf, long n) { (void)t, (void)buf, (void)n; return -1; }
void tls_close(TLS *t) { (void)t; }
const char *tls_describe(TLS *t) { (void)t; return ""; }
char tls_sigs_path[200];
void (*tls_log)(const char *msg);
/* libc.c's acc_core (common 9fd1335): the test is no accessory, so only
   the cookie inquiry is reached; no cookie, no supervisor call */
void *_BasePage;
long get_cookie(long id) { (void)id; return 0; }
long tos_supexec(void *f) { (void)f; return 0; }
