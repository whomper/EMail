/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* main_cf.c - core.c as ColdFire code under qemu-m68k -cpu cfv4e: Linux's
 * own calls (common/tests/coldfire/rt.S), the Atari libc of the FireBee package's EMAIL.PRG
 * (build/cf/libc.o) on a heap of its own, as an accessory's */
#include <stdio.h>
#include <string.h>

long sys_write(long fd, const void *p, long n);
long sys_read(long fd, void *p, long n);
long sys_open(const char *path, long flags, long mode);
long sys_close(long fd);
void heap_add(void *p, unsigned long bytes);
unsigned long core_file(const char *kind, const unsigned char *d, long n);

short _is_acc;			/* (crt0.S's: libc.c's heap asks) */
static long heap[6 << 18];	/* 6 MB */
static unsigned char in[1 << 20];

int main(int argc, char **argv)
{
	int i;
	heap_add(heap, sizeof(heap));
	for (i = 2; i < argc; i++) {
		long fd = sys_open(argv[i], 0, 0), n = 0, r;
		const char *b = strrchr(argv[i], '/');
		char line[300];
		if (fd < 0) {
			sys_write(2, "can't open\n", 11);
			return 1;
		}
		while (n < (long)sizeof(in) && (r = sys_read(fd, in + n, sizeof(in) - n)) > 0)
			n += r;
		sys_close(fd);
		snprintf(line, sizeof(line), "%s %08lx\n", b ? b + 1 : argv[i], core_file(argv[1], in, n));
		sys_write(1, line, (long)strlen(line));
	}
	return 0;
}
