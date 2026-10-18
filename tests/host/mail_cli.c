/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * mail_cli.c - drives EMail's mail core from a terminal, for the
 * integration tests (tests/integration.sh). Not part of the Atari
 * program; text is printed as UTF-8.
 *
 *   mail-cli DIR check N            check account N (1-based)
 *   mail-cli DIR folders N          list folders
 *   mail-cli DIR sync N FOLDER      mirror one IMAP folder
 *   mail-cli DIR more N FOLDER      mirror the next page of older messages
 *   mail-cli DIR getall N FOLDER    mirror all of it, and download every body
 *   mail-cli DIR again N FOLDER UID download the message again
 *   mail-cli DIR path N FOLDER UID  where the message's body is kept
 *   mail-cli DIR list N FOLDER      headers in the local index
 *   mail-cli DIR show N FOLDER UID  read a message
 *   mail-cli DIR send N FILE        FILE is editor text in UTF-8
 *   mail-cli DIR delete N FOLDER UID
 *   mail-cli DIR move N FOLDER UID DEST
 *   mail-cli DIR flag N FOLDER UID +seen|-seen|+flagged
 *   mail-cli DIR mkdir N NAME       NAME in UTF-8
 *   mail-cli DIR rmdir N FOLDER
 *   mail-cli DIR clearcache
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "plat.h"
#include "store.h"
#include "mail.h"
#include "mime.h"
#include "compose.h"
#include "charset.h"
#include "util.h"
#include "conn.h"

static void say(const char *atari)
{
	char *u = atari_to_utf8(atari, strlen(atari), 0);
	fputs(u, stdout);
	free(u);
}

static void st(const char *m)
{
	if (*m && getenv("MAIL_DEBUG"))
		fprintf(stderr, "[%s]\n", m);
}

static FINFO *find_folder(ACCOUNT *a, const char *name)
{
	short i;
	for (i = 0; i < a->nfolders; i++)
		if (!strcmp(a->folders[i].server, name))
			return &a->folders[i];
	for (i = 0; i < a->nfolders; i++)
		if (!strcasecmp(a->folders[i].disp, name))
			return &a->folders[i];
	return 0;
}

static int fail(const char *what)
{
	fprintf(stderr, "error: %s: %s\n", what, mail_err);
	return 1;
}

int main(int argc, char **argv)
{
	ACCOUNT *a = 0;
	const char *cmd;
	int r = 0;

	if (argc < 3) {
		fprintf(stderr, "usage: mail-cli DIR command ...\n");
		return 2;
	}
	store_init(argv[1]);
	atexit(conn_log_flush);
	signal(SIGPIPE, SIG_IGN);	/* a dropped connection is an error, not the end */		/* every way out, error returns too */
	if (getenv("MAIL_LOG")) {
		path_join(conn_logfile, sizeof(conn_logfile), argv[1], "EMAIL.LOG");
	}
	net_init();
	mail_status = st;
	cmd = argv[2];
	if (argc > 3) {
		int n = atoi(argv[3]);
		if (n < 1 || n > naccts) {
			fprintf(stderr, "no account %d\n", n);
			return 2;
		}
		a = accts[n - 1];
		folders_load(a);
	}

	if (!strcmp(cmd, "check") && a) {
		long n = 0;
		if (!mail_check(a, &n))
			return fail("check");
		printf("new: %ld\n", n);
	} else if (!strcmp(cmd, "folders") && a) {
		short i;
		if (!a->pop && !mail_refresh_folders(a))
			return fail("folders");
		for (i = 0; i < a->nfolders; i++) {
			FINFO *f = &a->folders[i];
			printf("%-24s role=%d local=%d noselect=%d depth=%d total=%ld unread=%ld disp=", f->server,
			       f->role, f->local, f->noselect, f->depth, f->total, f->unread);
			say(f->disp);
			printf("\n");
		}
	} else if (!strcmp(cmd, "sync") && a && argc > 4) {
		FINFO *fi = find_folder(a, argv[4]);
		long n;
		if (!fi && !mail_refresh_folders(a))
			return fail("folders");
		fi = find_folder(a, argv[4]);
		if (!fi) {
			fprintf(stderr, "no folder %s\n", argv[4]);
			return 1;
		}
		if (!mail_sync_folder(a, fi, &n))
			return fail("sync");
		{
			FOLDER *f = fold_open(a, fi);
			printf("synced: %ld total, %ld unread, %ld new, %ld loaded\n", fi->total, fi->unread, n,
			       f ? f->n : 0L);
			fold_close(f);
		}
	} else if (!strcmp(cmd, "more") && a && argc > 4) {
		FINFO *fi = find_folder(a, argv[4]);
		long n;
		FOLDER *f;
		if (!fi)
			return 1;
		if (!mail_load_more(a, fi, &n))
			return fail("more");
		f = fold_open(a, fi);
		printf("added: %ld, loaded %ld of %ld\n", n, f->n, f->exists);
		fold_close(f);
	} else if (!strcmp(cmd, "getall") && a && argc > 4) {
		FINFO *fi = find_folder(a, argv[4]);
		FOLDER *f;
		long i, got = 0, had = 0;
		if (!fi)
			return 1;
		if (!mail_mirror_all(a, fi))
			return fail("getall");
		f = fold_open(a, fi);
		for (i = 0; i < f->n; i++) {
			if (f->h[i].flags & MF_CACHED)
				had++;
			else if (!mail_download(f, &f->h[i], 0, 0))
				return fail("getall");
			else
				got++;
		}
		printf("loaded %ld of %ld, downloaded %ld, had %ld\n", f->n, f->exists, got, had);
		fold_close(f);
	} else if ((!strcmp(cmd, "again") || !strcmp(cmd, "pack") || !strcmp(cmd, "garble")) && a && argc > 5) {
		FINFO *fi = find_folder(a, argv[4]);
		FOLDER *f;
		HDR *h;
		char path[220];
		if (!fi)
			return 1;
		f = fold_open(a, fi);
		h = fold_get(f, strtoul(argv[5], 0, 10));
		if (!h) {
			fprintf(stderr, "no message %s\n", argv[5]);
			return 1;
		}
		if (!strcmp(cmd, "pack")) {
			/* the folder's pack, and where this message's record is */
			path_join(path, sizeof(path), f->dir, "MESSAGES.DAT");
			printf("%s %ld %ld\n", path, h->off, h->plen);
		} else if (!strcmp(cmd, "garble")) {
			/* a copy that came wrong, as a bad line would leave it */
			static const char bad[] = "From: x\r\n\r\ngarbled";
			if (msg_save(f, h->uid, bad, (long)sizeof(bad) - 1) < 0)
				return fail("garble");
			printf("ok\n");
		} else if (!mail_download(f, h, 1, 0)) {
			return fail("again");
		} else {
			printf("ok\n");
		}
		fold_close(f);
	} else if (!strcmp(cmd, "drop") && a && argc > 5) {
		/* the first N bodies forgotten: gaps in the pack, which the
		   close compacts once they are more than half of it */
		FINFO *fi = find_folder(a, argv[4]);
		FOLDER *f;
		long i, k = atol(argv[5]);
		if (!fi)
			return 1;
		f = fold_open(a, fi);
		for (i = 0; i < f->n && k > 0; i++)
			if (f->h[i].off >= 0) {
				body_drop(f, &f->h[i]);
				f->dirty = 1;
				k--;
			}
		fold_close(f);
		printf("ok\n");
	} else if (!strcmp(cmd, "list") && a && argc > 4) {
		FINFO *fi = find_folder(a, argv[4]);
		FOLDER *f;
		long i;
		if (!fi) {
			fprintf(stderr, "no folder %s\n", argv[4]);
			return 1;
		}
		f = fold_open(a, fi);
		for (i = 0; i < f->n; i++) {
			HDR *h = &f->h[i];
			char d[32];
			date_str(h->date, d, sizeof(d), 1);
			printf("%lu\t%c%c%c\t%ld\t%s\t", h->uid, h->flags & MF_SEEN ? ' ' : 'N',
			       h->flags & MF_FLAGGED ? '!' : ' ', h->flags & MF_ATTACH ? '@' : ' ', h->size, d);
			say(h->from);
			printf("\t");
			say(h->subject);
			printf("\n");
		}
		fold_close(f);
	} else if (!strcmp(cmd, "show") && a && argc > 5) {
		FINFO *fi = find_folder(a, argv[4]);
		FOLDER *f;
		HDR *h;
		char *raw;
		long len;
		MSG *m;
		short i;
		if (!fi)
			return 1;
		f = fold_open(a, fi);
		h = fold_get(f, strtoul(argv[5], 0, 10));
		if (!h) {
			fprintf(stderr, "no message %s\n", argv[5]);
			return 1;
		}
		raw = mail_fetch(f, h, &len);
		if (!raw)
			return fail("fetch");
		m = mime_parse(raw, len);
		printf("From: ");
		say(m->from);
		printf("\nTo: ");
		say(m->to);
		printf("\nSubject: ");
		say(m->subject);
		printf("\nDate: ");
		say(m->date);
		printf("\n");
		for (i = 0; i < m->nparts; i++) {
			printf("Attachment: ");
			say(m->parts[i].name);
			printf(" (%s, %ld bytes)\n", m->parts[i].type, m->parts[i].size);
		}
		printf("\n");
		say(m->text);
		printf("\n");
		mime_free(m);
		free(raw);
		fold_close(f);
	} else if (!strcmp(cmd, "sendnow") && a && argc > 4) {
		/* as Send now in the editor: from memory, the Outbox if it can't go */
		long len, al, rl;
		char *u = pf_load(argv[4], &len), *at, *raw, err[200];
		int sent = 0;
		if (!u)
			return 1;
		at = cs_to_atari(u, len, CS_UTF8, &al);
		raw = compose_build(a, at, al, 0, 0, err, sizeof(err), &rl);
		if (!raw) {
			fprintf(stderr, "error: %s\n", err);
			return 1;
		}
		r = mail_send(a, raw, rl, &sent);
		if (r == -1 && mail_queue(a, raw, rl))
			printf("queued: %s\n", mail_err);
		printf("sent: %d result: %d\n", sent, r);
		free(u);
		free(at);
		free(raw);
	} else if (!strcmp(cmd, "send") && a && argc > 4) {
		long len, al, rl;
		char *u = pf_load(argv[4], &len), *at, *raw, err[200];
		int sent = 0;
		if (!u)
			return 1;
		at = cs_to_atari(u, len, CS_UTF8, &al);
		raw = compose_build(a, at, al, 0, 0, err, sizeof(err), &rl);
		if (!raw) {
			fprintf(stderr, "error: %s\n", err);
			return 1;
		}
		if (!mail_queue(a, raw, rl))
			return fail("queue");
		if (mail_send_outbox(a, &sent) < 0)
			return fail("send");
		printf("sent: %d\n", sent);
		free(u);
		free(at);
		free(raw);
	} else if ((!strcmp(cmd, "delete") || !strcmp(cmd, "move") || !strcmp(cmd, "flag")) && a && argc > 5) {
		FINFO *fi = find_folder(a, argv[4]);
		FOLDER *f;
		HDR *h;
		if (!fi)
			return 1;
		f = fold_open(a, fi);
		h = fold_get(f, strtoul(argv[5], 0, 10));
		if (!h) {
			fprintf(stderr, "no message %s\n", argv[5]);
			return 1;
		}
		if (!strcmp(cmd, "delete"))
			r = mail_delete(f, h);
		else if (!strcmp(cmd, "move") && argc > 6)
			r = find_folder(a, argv[6]) ? mail_move(f, h, find_folder(a, argv[6])) : 0;
		else if (argc > 6) {
			unsigned short fl = strstr(argv[6], "seen") ? MF_SEEN : MF_FLAGGED;
			r = mail_flag(f, h, fl, argv[6][0] == '+') && mail_push_flags(f);
		}
		fold_close(f);
		folders_save(a);
		if (!r)
			return fail(cmd);
		printf("ok\n");
		r = 0;
	} else if (!strcmp(cmd, "allread") && a && argc > 4) {
		FINFO *fi = find_folder(a, argv[4]);
		FOLDER *f = fi ? fold_open(a, fi) : 0;	/* open, as on the screen */
		int ok = f && mail_mark_all_read(a, fi, f);
		if (f) {
			f->dirty = 1;			/* and saved after, as on the screen */
			fold_close(f);
		}
		if (!ok)
			return fail("allread");
		printf("ok\n");
	} else if (!strcmp(cmd, "mkdir") && a && argc > 4) {
		char *at = cs_to_atari(argv[4], strlen(argv[4]), CS_UTF8, 0);
		if (!mail_folder_create(a, at))
			return fail("mkdir");
		printf("ok\n");
		free(at);
	} else if (!strcmp(cmd, "rename") && a && argc > 5) {
		FINFO *fi = find_folder(a, argv[4]);
		char *at = cs_to_atari(argv[5], strlen(argv[5]), CS_UTF8, 0);
		char to[160];
		if (!fi || !mail_folder_rename(a, fi, at, to, sizeof(to)))
			return fail("rename");
		printf("ok %s\n", to);
		free(at);
	} else if (!strcmp(cmd, "rmdir") && a && argc > 4) {
		FINFO *fi = find_folder(a, argv[4]);
		if (!fi || !mail_folder_delete(a, fi))
			return fail("rmdir");
		printf("ok\n");
	} else if (!strcmp(cmd, "clearcache")) {
		short i;
		for (i = 0; i < naccts; i++)
			folders_load(accts[i]);
		cache_clear_all(0, 1);
		printf("ok\n");
	} else {
		fprintf(stderr, "bad command\n");
		r = 2;
	}
	mail_disconnect_all();
	return r;
}
