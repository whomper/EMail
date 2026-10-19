/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * plat_tos.c - files, folders and time on GEMDOS.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "tos.h"
#include "bigbuf.h"
#include "../src/plat.h"

void (*pf_idle)(void);

int pf_open(const char *path, int mode)
{
	long h;
	if (mode == PF_WRITE)
		return (int)Fcreate(path, 0);
	if (mode == PF_UPDATE)
		return (int)Fopen(path, 1);
	h = Fopen(path, mode == PF_APPEND ? 2 : 0);
	if (h < 0 && mode == PF_APPEND)
		h = Fcreate(path, 0);
	else if (h >= 0 && mode == PF_APPEND)
		Fseek(0, (short)h, 2);
	return (int)h;
}

/* TT RAM goes to the disk as it is only when the hard disk driver says it
   bounces DMA itself (_FRB, bigbuf.h); else through a piece of ST RAM.
   The program and its heap may be in TT RAM (PRGFLAGS 7), not only
   pf_bulk's buffer. */
#define TT_RAM 0x01000000UL
#define BOUNCE (16L * 1024)

static char *bounce;
static int direct = -1;

static int via_st(const void *buf)
{
	if ((unsigned long)buf < TT_RAM)
		return 0;
	if (direct < 0)
		direct = big_direct();
	if (!direct && !bounce && (long)(bounce = Mxalloc(BOUNCE, 0)) <= 0)
		bounce = 0;		/* none to spare: as before, straight */
	return !direct && bounce;
}

long pf_read(int h, void *buf, long n)
{
	long got = 0, k, r;
	if (!via_st(buf))
		return Fread((short)h, n, buf);
	while (got < n) {
		k = n - got < BOUNCE ? n - got : BOUNCE;
		if ((r = Fread((short)h, k, bounce)) < 0)
			return got ? got : r;
		memcpy((char *)buf + got, bounce, r);
		got += r;
		if (r < k)
			break;
	}
	return got;
}

long pf_write(int h, const void *buf, long n)
{
	long put = 0, k, r;
	if (!via_st(buf))
		return Fwrite((short)h, n, buf);
	while (put < n) {
		k = n - put < BOUNCE ? n - put : BOUNCE;
		memcpy(bounce, (const char *)buf + put, k);
		if ((r = Fwrite((short)h, k, bounce)) < 0)
			return put ? put : r;
		put += r;
		if (r < k)
			break;
	}
	return put;
}
void pf_close(int h) { Fclose((short)h); }
long pf_seek(int h, long off) { return Fseek(off, (short)h, 0); }

long pf_handle_size(int h)
{
	long n = Fseek(0, (short)h, 2);
	if (n < 0 || Fseek(0, (short)h, 0) < 0)
		return -1;
	return n;
}

/* 256 KB in TT RAM; in ST RAM a quarter of what is free, so the heap,
   GEM and the accessories keep the rest (a 1 MB ST) */
void *pf_bulk(long *got)
{
	long want = 256L * 1024, st = (long)Mxalloc(-1L, 0), tt = (long)Mxalloc(-1L, 1);
	int in_tt;
	if (st == -32) {		/* TOS 1.x: no Mxalloc, no TT RAM */
		st = (long)Malloc(-1L);
		tt = 0;
	}
	if (tt < want + 64L * 1024 && st / 4 < want)
		want = st / 4;
	return big_alloc(want, got, &in_tt);
}

void pf_bulk_free(void *p) { big_free(p); }

static DTA dta;

long pf_size(const char *path)
{
	void *old = Fgetdta();
	long r;
	Fsetdta(&dta);
	r = Fsfirst(path, 0x07) == 0 ? (long)dta.d_length : -1;
	Fsetdta(old);
	return r;
}

/* the date and the size from one look at the folder (guidelines 6.5) */
unsigned long pf_info(const char *path, long *size)
{
	void *old = Fgetdta();
	unsigned long r = 0;
	Fsetdta(&dta);
	*size = -1;
	if (Fsfirst(path, 0x07) == 0) {
		*size = (long)dta.d_length;
		r = ((unsigned long)dta.d_date << 16) | dta.d_time;
		if (!r)
			r = 1;
	}
	Fsetdta(old);
	return r;
}

int pf_exists(const char *path)
{
	void *old = Fgetdta();
	long r;
	Fsetdta(&dta);
	r = Fsfirst(path, 0x17);
	Fsetdta(old);
	return r == 0;
}

int pf_mkdir(const char *path)
{
	long r = Dcreate(path);
	return r == 0 ? 1 : (r == -36 || pf_exists(path)) ? 0 : (int)r;
}

int pf_remove(const char *path) { return (int)Fdelete(path); }
int pf_rename(const char *from, const char *to) { return (int)Frename(from, to); }

int pf_list(const char *dir, int (*cb)(const char *, long, void *), void *ud)
{
	char pat[200];
	DTA d;
	void *old = Fgetdta();
	long r;
	int n = 0;
	strncpy(pat, dir, sizeof(pat) - 6);
	pat[sizeof(pat) - 6] = 0;
	if (*pat && pat[strlen(pat) - 1] != '\\')
		strcat(pat, "\\");
	strcat(pat, "*.*");
	Fsetdta(&d);
	for (r = Fsfirst(pat, 0x01); r == 0; r = Fsnext()) {
		if (d.d_attrib & 0x18)
			continue;
		n++;
		if (cb(d.d_fname, (long)d.d_length, ud))
			break;
		Fsetdta(&d);	/* the callback may have used the DTA */
	}
	Fsetdta(old);
	return n;
}

void pf_now(PFTIME *t)
{
	unsigned short d = (unsigned short)Tgetdate();
	unsigned short tm = (unsigned short)Tgettime();
	t->year = 1980 + (d >> 9);
	t->mon = (d >> 5) & 15;
	t->day = d & 31;
	t->hour = tm >> 11;
	t->min = (tm >> 5) & 63;
	t->sec = (tm & 31) * 2;
}

/* the environment: NAME=value strings after each other, an empty one
   last, at p_env in the basepage (offset 0x2C) */
const char *pf_getenv(const char *name)
{
	const char *e = *(const char **)((const char *)_BasePage + 0x2C);
	size_t n = strlen(name);
	if (!e)
		return 0;
	while (*e) {
		if (!strncmp(e, name, n) && e[n] == '=')
			return e + n + 1;
		e += strlen(e) + 1;
	}
	return 0;
}

/* milliseconds, right even where the 200 Hz timer runs faster (tos.c) */
unsigned long pf_ms(void)
{
	return tos_ms();
}

/* Timer jitter: the MFP's timer C counts every 26 us and the video
 * chip's line counter every 32 us; read in a loop whose length depends
 * on the previous readings, they drift against the CPU clock, the bus
 * and interrupts. Each byte folds in several readings. */
#pragma GCC diagnostic ignored "-Warray-bounds"	/* fixed hardware addresses */
static unsigned char *ent_buf;
static int ent_n;

static void sup_entropy(void)
{
	volatile unsigned char *tcdr = (volatile unsigned char *)0xfffffa23L;
	volatile unsigned long *hz200 = (volatile unsigned long *)0x4baL;
	unsigned long acc = *hz200;
	int i, k;
	for (i = 0; i < ent_n; i++) {
		for (k = 0; k < 8; k++) {
			volatile int spin, j = (int)(acc & 31) + 8;
			for (spin = 0; spin < j; spin++)
				;
			acc = (acc << 5) ^ (acc >> 27) ^ *tcdr ^ (*hz200 << 3);
		}
		ent_buf[i] ^= (unsigned char)(acc ^ (acc >> 8) ^ (acc >> 16) ^ (acc >> 24));
	}
}

void pf_entropy(unsigned char *buf, int n)
{
	int i;
	unsigned short d = (unsigned short)Tgetdate(), tm = (unsigned short)Tgettime();
	for (i = 0; i < n; i++)
		buf[i] = (unsigned char)(i * 151);
	buf[0] ^= (unsigned char)tm;
	buf[1] ^= (unsigned char)(tm >> 8);
	buf[2] ^= (unsigned char)d;
	buf[3] ^= (unsigned char)(d >> 8);
	ent_buf = buf;
	ent_n = n;
	Supexec(sup_entropy);
	/* XBIOS Random(), and where the stack happens to be */
	for (i = 0; i + 2 < n; i += 3) {
		unsigned long r = (unsigned long)trap14_w(17);
		buf[i] ^= (unsigned char)r;
		buf[i + 1] ^= (unsigned char)(r >> 8);
		buf[i + 2] ^= (unsigned char)(r >> 16);
	}
	buf[n - 1] ^= (unsigned char)((unsigned long)&i >> 3);
}

void pf_debug(const char *s)
{
	nf_debug(s);
	nf_debug("\n");
}

/* ---------------- memory ---------------- */

extern long heap_tt, heap_st;

/* "12.3 MB" or "640 KB" */
static void mb(char *out, int size, long bytes)
{
	if (bytes >= 1024L * 1024)
		snprintf(out, size, "%ld.%ld MB", bytes / (1024L * 1024), bytes % (1024L * 1024) * 10 / (1024L * 1024));
	else
		snprintf(out, size, "%ld KB", bytes / 1024);
}

void pf_memory(char *where, int wsize, char *avail, int asize)
{
	long st = (long)Mxalloc(-1L, 0), tt = (long)Mxalloc(-1L, 1);
	int prog_tt = (unsigned long)_BasePage >= 0x01000000UL;
	const char *data = heap_tt && heap_st ? "TT and ST RAM" : heap_tt ? "TT RAM" : "ST RAM";
	char a[16], b[16];
	if (st == -32) {		/* TOS 1.x: no Mxalloc, no TT RAM */
		st = (long)Malloc(-1L);
		tt = 0;
	}
	if (tt < 0)
		tt = 0;
	if (prog_tt && heap_tt && !heap_st)
		snprintf(where, wsize, "Running in TT RAM");
	else if (!prog_tt && !heap_tt)
		snprintf(where, wsize, tt ? "Running in ST RAM (TT RAM not used)" : "Running in ST RAM");
	else
		snprintf(where, wsize, "Program in %s, data in %s", prog_tt ? "TT RAM" : "ST RAM", data);
	mb(a, sizeof(a), st);
	if (tt || heap_tt) {
		mb(b, sizeof(b), tt);
		snprintf(avail, asize, "Free: %s TT RAM, %s ST RAM", b, a);
	} else {
		snprintf(avail, asize, "Free: %s ST RAM", a);
	}
}
