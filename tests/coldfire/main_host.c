/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* main_host.c - core.c on the build machine: each file read, its line said */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned long core_file(const char *kind, const unsigned char *d, long n);

int main(int argc, char **argv)
{
	int i;
	for (i = 2; i < argc; i++) {
		FILE *f = fopen(argv[i], "rb");
		unsigned char *d;
		long n;
		const char *b = strrchr(argv[i], '/');
		if (!f) {
			perror(argv[i]);
			return 1;
		}
		fseek(f, 0, SEEK_END);
		n = ftell(f);
		fseek(f, 0, SEEK_SET);
		d = malloc(n + 1);
		if (!d || fread(d, 1, n, f) != (size_t)n)
			return 1;
		fclose(f);
		printf("%s %08lx\n", b ? b + 1 : argv[i], core_file(argv[1], d, n));
		free(d);
	}
	return 0;
}
