/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* mont_ref.c - mont030.S and dbl030 in C, to check rsa030.c on the host
   (rsacheck.c) */
#include <stdint.h>
#include <string.h>
uint32_t mont030(uint32_t *buf, const uint32_t *x, const uint32_t *y, const uint32_t *m, uint32_t m0i, long len)
{
	uint64_t dh = 0;
	long u, v;
	uint32_t *d = buf + 1;
	for (u = 0; u < len; u++) {
		uint32_t xu = x[u], f = (d[0] + xu * y[0]) * m0i;
		uint64_t r1 = 0, r2 = 0, z, zh;
		for (v = 0; v < len; v++) {
			uint32_t t;
			z = (uint64_t)d[v] + (uint64_t)xu * y[v] + r1;
			r1 = z >> 32;
			t = (uint32_t)z;
			z = (uint64_t)t + (uint64_t)f * m[v] + r2;
			r2 = z >> 32;
			d[v - 1] = (uint32_t)z;	/* d[-1] is buf[0] */
		}
		zh = dh + r1 + r2;
		d[len - 1] = (uint32_t)zh;
		dh = zh >> 32;
	}
	return (uint32_t)dh;
}
void dbl030(uint32_t *r, const uint32_t *n, long k)
{
	uint32_t c = 0, b = 0;
	long i;
	for (i = 0; i < k; i++) {
		uint32_t t = r[i];
		r[i] = t << 1 | c;
		c = t >> 31;
	}
	if (!c) {
		for (i = k - 1; i >= 0 && r[i] == n[i]; i--)
			;
		if (i >= 0 && r[i] < n[i])
			return;
	}
	for (i = 0; i < k; i++) {
		uint32_t t = r[i] - n[i] - b;
		b = (r[i] < n[i]) || (r[i] - n[i] < b);
		r[i] = t;
	}
}
