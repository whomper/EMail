/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * migrate_eml.c - until EMail 1.41 a folder kept each message body it
 * had in a file of its own (0000002A.EML). The first time a folder is
 * opened after the update, its files are read, gathered in memory and
 * appended to the folder's pack (MESSAGES.DAT) in one go; once the
 * index says where they are, the files are deleted. Erez reads mail
 * offline, so nothing is dropped: a file that can't be read now stays,
 * and is tried again the next time.
 *
 * TEMPORARY: this file goes once Erez has run the release that brought
 * the pack (1.42) on his machines. To take it out: this file and
 * migrate_eml.h, the call in fold_open (store.c), "migrate_eml" in the
 * Makefile's CORE, and tests/migrate_test.sh with its line in the
 * Makefile's itest.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "mail.h"
#include "migrate_eml.h"

static int legacy(const HDR *h)
{
	return (h->flags & MF_CACHED) && h->off < 0;
}

void migrate_eml(FOLDER *f)
{
	char path[220], line[120];
	long i, n = 0, k = 0;
	for (i = 0; i < f->n; i++)
		if (legacy(&f->h[i]))
			n++;
	if (!n)
		return;
	if (mail_status) {
		snprintf(line, sizeof(line), "%s: moving %ld messages into one file (once)...",
			 f->fi ? f->fi->disp : "", n);
		mail_status(line);
	}
	f->bulk = 1;			/* all of them in one append */
	for (i = 0; i < f->n; i++) {
		HDR *h = &f->h[i];
		char *raw;
		long len;
		if (!legacy(h))
			continue;
		msg_path(f, h->uid, path, sizeof(path));
		raw = pf_load(path, &len);
		if (!raw) {
			if (pf_size(path) < 0)
				h->flags &= ~MF_CACHED;	/* gone: from the server if it is wanted */
			continue;		/* there but unreadable now: next time */
		}
		if (msg_save(f, h->uid, raw, len) < 0) {
			h->flags |= MF_CACHED;	/* still in its file: next time */
			h->off = -1;
		} else {
			k++;
		}
		free(raw);
	}
	f->bulk = 0;
	/* the pack and then the index; only then the files go */
	if (k && fold_save(f) == 0)
		for (i = 0; i < f->n; i++) {
			HDR *h = &f->h[i];
			if (h->off >= 0 && h->flags & MF_CACHED) {
				msg_path(f, h->uid, path, sizeof(path));
				if (pf_size(path) >= 0)
					pf_remove(path);
			}
		}
	if (mail_status)
		mail_status("");
}
