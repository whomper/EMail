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

int link_next(const char *s, long n, long from, long *at, long *len)
{
	long i;
	for (i = from; i < n; i++) {
		short k;
		long e, pre;
		/* an address starts a word: not "xhttp://" or "a.www." (after a
		   hyphen it does: Hebrew's "\xC3-www.atari.org", "at ...") */
		if (i > 0 && (alnum((unsigned char)s[i - 1]) || s[i - 1] == '.' || s[i - 1] == '/' ||
			      s[i - 1] == '@' || s[i - 1] == '_'))
			continue;
		for (k = 0; k < 4 && !starts(s + i, n - i, schemes[k]); k++)
			;
		if (k == 4)
			continue;
		pre = (long)strlen(schemes[k]);
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
		if (e - i <= pre || (k == 3 && !alnum((unsigned char)s[i + pre]))) {
			i += pre - 1;
			continue;
		}
		*at = i;
		*len = e - i;
		return 1;
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
