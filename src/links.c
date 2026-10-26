/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * links.c - the addresses written in a message's text (see links.h).
 */
#include <string.h>
#include <stdlib.h>
#include "links.h"

static const char *const schemes[] = { "http://", "https://", "mailto:", "www." };

/* s[0..n) starts with prefix, in any case */
static int starts(const char *s, long n, const char *prefix)
{
	long k;
	for (k = 0; prefix[k]; k++) {
		char c = k < n ? s[k] : 0;
		if (c >= 'A' && c <= 'Z')
			c = (char)(c + 32);
		if (c != prefix[k])
			return 0;
	}
	return 1;
}

static int alnum(unsigned char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

/* a character an address goes on with: printable ASCII but the ones
   that end one in text (spaces, quotes, angle brackets) */
static int url_char(unsigned char c)
{
	return c > ' ' && c < 0x7F && c != '<' && c != '>' && c != '"' && c != '`';
}

/* how many times c is in s[0..n) */
static long count(const char *s, long n, char c)
{
	long k, m = 0;
	for (k = 0; k < n; k++)
		m += s[k] == c;
	return m;
}

/* the next ':' or '.' in s[i..n), or n: where a scheme ends ("http:",
   "https:", "mailto:", "www."). Found by memchr, which an ST runs some
   ten times faster than a C loop over every character */
static long next_mark(const char *s, long n, long i, long *colon, long *dot)
{
	const char *p;
	if (*colon < i)
		*colon = (p = memchr(s + i, ':', n - i)) != 0 ? p - s : n;
	if (*dot < i)
		*dot = (p = memchr(s + i, '.', n - i)) != 0 ? p - s : n;
	return *colon < *dot ? *colon : *dot;
}

int link_next(const char *s, long n, long from, long *at, long *len)
{
	long j, colon = -1, dot = -1;
	/* each ':' and '.': the scheme that would end there */
	for (j = next_mark(s, n, from, &colon, &dot); j < n; j = next_mark(s, n, j + 1, &colon, &dot)) {
		short k, k0, k1;
		long i, e, pre;
		if (s[j] == '.') {
			k0 = k1 = 3;		/* "www." */
		} else {
			k0 = 0;			/* "http:", "https:", "mailto:" */
			k1 = 2;
		}
		for (k = k0; k <= k1; k++) {
			pre = (long)strlen(schemes[k]);
			i = j - (long)(strchr(schemes[k], s[j]) - schemes[k]);
			if (i < from || !starts(s + i, n - i, schemes[k]))
				continue;
			/* an address starts a word: not "xhttp://" or "a.www." (after
			   a hyphen it does: Hebrew's "\xC3-www.atari.org", "at ...") */
			if (i > 0 && (alnum((unsigned char)s[i - 1]) || s[i - 1] == '.' || s[i - 1] == '/' ||
				      s[i - 1] == '@' || s[i - 1] == '_'))
				continue;
			for (e = i + pre; e < n && url_char((unsigned char)s[e]); e++)
				;
			/* the sentence's own marks after it are not the address's; a
			   bracket closing one opened in it is ("..._(Atari)") */
			while (e > i + pre) {
				char c = s[e - 1];
				if (c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == '?' || c == '\'' ||
				    c == '*')
					e--;
				else if ((c == ')' && count(s + i, e - i, '(') < count(s + i, e - i, ')')) ||
					 (c == ']' && count(s + i, e - i, '[') < count(s + i, e - i, ']')) ||
					 (c == '}' && count(s + i, e - i, '{') < count(s + i, e - i, '}')))
					e--;
				else
					break;
			}
			/* "www." needs a name after it; the others something */
			if (e - i <= pre || (k == 3 && !alnum((unsigned char)s[i + pre])))
				continue;
			*at = i;
			*len = e - i;
			return 1;
		}
	}
	return 0;
}

char *link_url(const char *s, long n)
{
	int www = starts(s, n, "www.");
	char *u = malloc(n + (www ? 8 : 1));
	if (!u)
		return 0;
	if (www)
		memcpy(u, "http://", 7);
	memcpy(u + (www ? 7 : 0), s, n);
	u[n + (www ? 7 : 0)] = 0;
	return u;
}

int link_wanted(const char *url)
{
	long n = (long)strlen(url);
	short k;
	for (k = 0; k < 4; k++)
		if (starts(url, n, schemes[k]) && n > (long)strlen(schemes[k]))
			return 1;
	return 0;
}
