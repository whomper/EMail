/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* bench.c - rsa030_public against BearSSL's br_rsa_i31_public: the same
   answers, and how long each takes, for RSA-2048, 3072 and 4096 */
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "tos.h"
#include "bearssl.h"
#include "rsa030.h"

unsigned long tos_hz200(void);
void nf_debug(const char *s);
static char out[4096];
static int olen;

static void say(const char *fmt, ...)
{
	char line[200];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	Cconws(line);
	nf_debug(line);
	if (olen + (int)strlen(line) < (int)sizeof(out)) {
		strcpy(out + olen, line);
		olen += strlen(line);
	}
}

static unsigned char n[512], x0[512], a[512], b[512], e[3] = { 1, 0, 1 };
static size_t len;
static int which;

static void run(void)
{
	memcpy(which ? b : a, x0, len);
	if (which)
		rsa030_public(b, len, n, len, e, 3);
	else {
		br_rsa_public_key k = { n, len, e, 3 };
		br_rsa_i31_public(a, len, &k);
	}
}

static unsigned long timeit(void)
{
	unsigned long t0 = tos_hz200(), t;
	int c = 0;
	do {
		run();
		c++;
		t = tos_hz200() - t0;
	} while (t < 200);
	return t * 5 / c;
}

int main(void)
{
	static const size_t sizes[] = { 256, 384, 512 };
	unsigned s, i, seed = 12345;
	say("RSA public operation, e=65537: BearSSL i31 against the 68030 code\r\n");
	for (s = 0; s < 3; s++) {
		unsigned long ti, ta;
		int same = 1, round;
		len = sizes[s];
		for (round = 0; round < 3; round++) {
			for (i = 0; i < len; i++) {
				seed = seed * 1103515245u + 12345u;
				n[i] = (unsigned char)(seed >> 16);
				seed = seed * 1103515245u + 12345u;
				x0[i] = (unsigned char)(seed >> 16);
			}
			n[0] |= 0x80;
			n[len - 1] |= 1;
			x0[0] &= 0x7f;
			which = 0; run();
			which = 1; run();
			if (memcmp(a, b, len))
				same = 0;
		}
		which = 0; ti = timeit();
		which = 1; ta = timeit();
		say("  RSA-%u: i31 %lu ms, 68030 %lu ms, %s\r\n", (unsigned)(len * 8), ti, ta,
		    same ? "the same answers" : "DIFFERENT ANSWERS");
	}
	say("BENCH DONE\r\n");
	{
		long h = Fcreate("RSA030.TXT", 0);
		if (h >= 0) {
			Fwrite((int)h, olen, out);
			Fclose((int)h);
		}
	}
	return 0;
}
