/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * links.h - the links in a message's text, for the reader to open: web
 * addresses (http://, https://, www.) and mail ones (mailto:) written in
 * the text, and HTML's <a href> (mime.c keeps those, with the text they
 * are on).
 */
#ifndef LINKS_H
#define LINKS_H

/* a link on the text at[0..n) of a message, opening url */
typedef struct {
	long at, n;
	char *url;
} MLINK;

#define MIME_MAXLINKS 400	/* a message's <a href>s kept, at most */

/* the next address written in s[from..n): 1, with its start and length */
int link_next(const char *s, long n, long from, long *at, long *len);

/* what to open for the address s[0..n): itself, with "http://" before a
   bare "www."; malloc'ed */
char *link_url(const char *s, long n);

/* 1: an address the reader opens (http:, https:, mailto:, www.) */
int link_wanted(const char *url);

#endif
