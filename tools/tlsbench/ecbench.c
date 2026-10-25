/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* ecbench.c - what a TLS handshake computes, with BearSSL's i31 code (its
   br_i31_montymul as built: C, or i31_montmul_030.c) and its m31 code (C):
   RSA, P-256 and P-384 point multiplication and ECDSA checks, X25519.
   A checksum of every answer, to compare the two builds */
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "tos.h"
#include "bearssl.h"

unsigned long tos_hz200(void);
void nf_debug(const char *s);
static char out[4096];
static int olen;
static unsigned long sum;

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

static void add(const unsigned char *p, size_t n)
{
	while (n--)
		sum = sum * 31 + *p++;
}

static void (*fn)(void);
static unsigned long timeit(void (*f)(void))
{
	unsigned long t0 = tos_hz200(), t;
	int c = 0;
	fn = f;
	do {
		fn();
		c++;
		t = tos_hz200() - t0;
	} while (t < 200);
	return t * 5 / c;
}

/* RSA */
static unsigned char rn[512], rx[512], ry[512], re[3] = { 1, 0, 1 };
static size_t rlen;
static void rsa(void)
{
	br_rsa_public_key k = { rn, rlen, re, 3 };
	memcpy(ry, rx, rlen);
	br_rsa_i31_public(ry, rlen, &k);
}

/* point multiplication and ECDSA */
static const br_ec_impl *ec;
static int curve;
static unsigned char pt[133], pt0[133], scal[66], hash[48], sig[132], pub[133], priv[66];
static size_t ptlen, slen, siglen, qlen;
static br_ecdsa_vrfy vrfy;
static uint32_t okv;
static void ecmul(void)
{
	memcpy(pt, pt0, ptlen);
	ec->mul(pt, ptlen, scal, slen, curve);
}
static void ecdsa(void)
{
	br_ec_public_key k = { curve, pub, qlen };
	okv = vrfy(ec, hash, sizeof(hash) > 32 && curve == BR_EC_secp256r1 ? 32 : (curve == BR_EC_secp256r1 ? 32 : 48), &k, sig, siglen);
}

static void curve_set(int crv, size_t sl)
{
	size_t i;
	br_ec_private_key pk;
	curve = crv;
	slen = sl;
	for (i = 0; i < sl; i++)
		scal[i] = priv[i] = (unsigned char)(i * 13 + 7);
	priv[0] &= 0x7f;
	scal[0] &= 0x7f;
	pk.curve = crv;
	pk.x = priv;
	pk.xlen = sl;
	qlen = br_ec_compute_pub(&br_ec_prime_i31, 0, pub, &pk);
	ptlen = qlen;
	memcpy(pt0, pub, qlen);
	for (i = 0; i < sizeof(hash); i++)
		hash[i] = (unsigned char)(i * 3);
	siglen = br_ecdsa_i31_sign_raw(&br_ec_prime_i31, crv == BR_EC_secp256r1 ? &br_sha256_vtable : &br_sha384_vtable,
				       hash, &pk, sig);
}

int main(void)
{
	static const size_t sizes[] = { 256, 512 };
	unsigned i, s, seed = 12345;
	unsigned long t;
	say("TLS pieces, %s\r\n",
#ifdef ASM030
	    "br_i31_montymul in 68030 assembly"
#else
	    "BearSSL's C"
#endif
	   );
	for (s = 0; s < 2; s++) {
		rlen = sizes[s];
		for (i = 0; i < rlen; i++) {
			seed = seed * 1103515245u + 12345u; rn[i] = (unsigned char)(seed >> 16);
			seed = seed * 1103515245u + 12345u; rx[i] = (unsigned char)(seed >> 16);
		}
		rn[0] |= 0x80; rn[rlen - 1] |= 1; rx[0] &= 0x7f;
		t = timeit(rsa);
		add(ry, rlen);
		say("  RSA-%u check, i31: %lu ms\r\n", (unsigned)rlen * 8, t);
	}
	curve_set(BR_EC_secp256r1, 32);
	ec = &br_ec_p256_m31;
	t = timeit(ecmul); add(pt, ptlen);
	say("  P-256 point multiply, m31: %lu ms\r\n", t);
	ec = &br_ec_prime_i31;
	t = timeit(ecmul); add(pt, ptlen);
	say("  P-256 point multiply, i31: %lu ms\r\n", t);
	vrfy = br_ecdsa_i31_vrfy_raw;
	ec = &br_ec_p256_m31;
	t = timeit(ecdsa);
	say("  ECDSA P-256 check, m31: %lu ms (%s)\r\n", t, okv ? "valid" : "INVALID");
	ec = &br_ec_prime_i31;
	t = timeit(ecdsa);
	say("  ECDSA P-256 check, i31: %lu ms (%s)\r\n", t, okv ? "valid" : "INVALID");
	curve_set(BR_EC_secp384r1, 48);
	ec = &br_ec_prime_i31;
	t = timeit(ecmul); add(pt, ptlen);
	say("  P-384 point multiply, i31: %lu ms\r\n", t);
	t = timeit(ecdsa);
	say("  ECDSA P-384 check, i31: %lu ms (%s)\r\n", t, okv ? "valid" : "INVALID");
	curve = BR_EC_curve25519;
	ptlen = 32; slen = 32;
	memset(pt0, 0, 32); pt0[0] = 9;
	ec = &br_ec_c25519_m31;
	t = timeit(ecmul); add(pt, 32);
	say("  X25519, m31: %lu ms\r\n", t);
	ec = &br_ec_c25519_i31;
	t = timeit(ecmul); add(pt, 32);
	say("  X25519, i31: %lu ms\r\n", t);
	say("  checksum of the answers: %08lx\r\n", sum);
	say("BENCH DONE\r\n");
	return 0;
}
