/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* rsacheck.c - rsa030.c (with mont_ref.c) against BearSSL on the host:
   150 RSA public operations, 2048 to 4096 bits, short moduli too */
#include <stdio.h>
#include <string.h>
#include "bearssl.h"
#include "rsa030.h"
int main(void)
{
	static unsigned char n[512], x0[512], a[512], b[512], e[3] = { 1, 0, 1 };
	static const size_t sizes[] = { 256, 384, 512 };
	unsigned s, i, seed = 12345;
	int bad = 0, round;
	for (s = 0; s < 3; s++)
		for (round = 0; round < 50; round++) {
			size_t len = sizes[s];
			for (i = 0; i < len; i++) {
				seed = seed * 1103515245u + 12345u; n[i] = (unsigned char)(seed >> 16);
				seed = seed * 1103515245u + 12345u; x0[i] = (unsigned char)(seed >> 16);
			}
			n[0] |= 0x80; n[len - 1] |= 1; x0[0] &= 0x7f;
			if (round == 1) n[0] = 0x40 | (n[0] & 0x3f);	/* a modulus a bit short */
			x0[0] = n[0] >> 1;
			memcpy(a, x0, len); memcpy(b, x0, len);
			{ br_rsa_public_key k = { n, len, e, 3 }; br_rsa_i31_public(a, len, &k); }
			if (!rsa030_public(b, len, n, len, e, 3) || memcmp(a, b, len)) { bad++; printf("differ: %u bits, round %d\n", (unsigned)len * 8, round); }
		}
	printf("%s\n", bad ? "BAD" : "all the same");
	return bad;
}
