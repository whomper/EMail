/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * A message as it arrives (the AERA guidelines, 7.1): the headers and
 * their encodings, the MIME structure, the text to show (HTML made text,
 * charsets made Atari text), every attachment decoded, the addresses
 * split.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mime.h"

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n)
{
	char *raw = malloc(n + 1), name[100], email[100];
	MSG *m;
	long hl, len;
	int i;
	if (!raw)
		return 0;
	memcpy(raw, d, n);
	raw[n] = 0;
	m = mime_parse(raw, (long)n);
	if (m) {
		for (i = 0; i < m->nparts; i++)
			free(mime_part_data(raw, &m->parts[i], &len));
		if (m->from)
			addr_split(m->from, name, sizeof(name), email, sizeof(email));
		/* the links the reader draws: on the text, and the text's own */
		for (i = 0; i < m->nlinks; i++)
			if (m->links[i].at < 0 || m->links[i].n <= 0 || m->links[i].at + m->links[i].n > m->textlen)
				__builtin_trap();
		{
			long at, ln, from = 0;
			while (m->text && link_next(m->text, m->textlen, from, &at, &ln)) {
				free(link_url(m->text + at, ln));
				from = at + ln;
			}
		}
		mime_free(m);
	}
	hl = mime_header_len(raw, (long)n);
	if (hl > 0) {
		char *ct = mime_header(raw, hl, "Content-Type");
		if (ct) {
			free(mime_param(ct, "boundary"));
			free(mime_param(ct, "charset"));
			free(ct);
		}
	}
	free(html_to_text(raw, (long)n, &len));
	{
		MLINK *lk = 0;
		short nl = 0;
		char *t = html_to_text_links(raw, (long)n, &len, &lk, &nl);
		for (i = 0; i < nl; i++) {
			if (!t || lk[i].at < 0 || lk[i].n <= 0 || lk[i].at + lk[i].n > len)
				__builtin_trap();
			free(lk[i].url);
		}
		free(lk);
		free(t);
	}
	free(raw);
	return 0;
}
