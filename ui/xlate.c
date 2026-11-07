/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * xlate.c - Options > Translate (Erez, 7 Nov 2026): with its check mark
 * each message shown is sent whole, its subject and text, to the Gateway's
 * translation service (translategate, port 8480: common/src/translate.c)
 * and shown translated into the language Settings names; the status line
 * says "Translating..." until it is. Without the check mark nothing here
 * runs and nothing is sent. The reader's right-click "Translate..." on
 * chosen words is the other use: their translation in a box of its own,
 * the letter as it is.
 *
 * The text goes a piece at a time (a few paragraphs), a turn between
 * pieces (main.c calls xlate_step when no event waits), so the window
 * stays alive; another message, or Translate switched off, drops what is
 * left; Esc stops a piece on its way. Translations are kept in memory
 * while EMail runs (the last few), never on the disk.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ui.h"
#include "../src/mail.h"
#include "util.h"
#include "charset.h"
#include "conn.h"
#include "translate.h"
#include "../src/plat.h"

/* the test build says on the console each time the Gateway is asked
   (tests/hatari/translate_test.py: never with Translate off) */
#ifdef DRAW_TRACE
#define TRACE(what) pf_debug(what)
#else
#define TRACE(what) ((void)0)
#endif

#define TR_PORT 8480		/* translategate's (Erez, 7 Nov 2026) */
#define KEEP 4			/* translations kept in memory */

static TRANSLATE t;
static int running;
static char *src;		/* the message's subject and text, UTF-8 */
static SBUF got;		/* the translation so far, UTF-8 */
static FOLDER *t_folder;
static unsigned long t_uid;
static int told;		/* the Gateway's silence said once in an alert */

static struct {
	FOLDER *f;
	unsigned long uid;
	char to[TR_LANG];
	char *text;		/* Atari text: the subject, a blank line, the text */
	long n;
} kept[KEEP];
static int next_keep;

/* the Gateway: the host the accounts reach their mail through */
static const char *gateway(void)
{
	int i;
	if (cur_acct && cur_acct->host[0])
		return cur_acct->host;
	for (i = 0; i < naccts; i++)
		if (accts[i]->host[0])
			return accts[i]->host;
	return "";
}

static void drop(void)
{
	running = 0;
	free(src);
	src = 0;
	sb_free(&got);
}

int xlate_running(void)
{
	return running;
}

/* the reader shows the translation: the first paragraph is the subject */
static void show(const char *atari, long n)
{
	const char *e = 0;
	long i;
	char subject[200];
	for (i = 0; i + 1 < n; i++)
		if (atari[i] == '\n' && atari[i + 1] == '\n') {
			e = atari + i;
			break;
		}
	if (!e) {
		reader_translation(cur_msg ? cur_msg->subject : "", atari, n);
		return;
	}
	str_copy(subject, atari, (int)(e - atari + 1 < (long)sizeof(subject) ? e - atari + 1 : (long)sizeof(subject)));
	reader_translation(subject, e + 2, n - (e + 2 - atari));
}

/* the message shown now: translated, if Translate is on (reader.c, once
   the message is drawn as it came) */
void xlate_shown(void)
{
	long n, sn, un;
	char *u, *su;
	const char *host = gateway();
	int k;
	drop();
	if (!opt.translate || !cur_msg || !cur_raw || reader_missing || !cur_folder)
		return;
	for (k = 0; k < KEEP; k++)
		if (kept[k].text && kept[k].f == cur_folder && kept[k].uid == cur_uid &&
		    !strcmp(kept[k].to, opt.translate_to)) {
			show(kept[k].text, kept[k].n);
			return;
		}
	if (!host[0])
		return;
	u = mime_text_utf8(cur_raw, cur_rawlen, &un);
	su = atari_to_utf8(cur_msg->subject, (long)strlen(cur_msg->subject), &sn);
	if (!u || !su) {
		free(u);
		free(su);
		return;
	}
	n = sn + 2 + un;
	src = malloc(n + 1);
	if (!src) {
		free(u);
		free(su);
		return;
	}
	/* the subject, a blank line, the text: the Gateway gives back as many
	   paragraphs as it got, so the first is the subject's */
	memcpy(src, su, sn);
	src[sn] = '\n';
	src[sn + 1] = '\n';
	memcpy(src + sn + 2, u, un);
	src[n] = 0;
	free(u);
	free(su);
	sb_init(&got);
	TRACE("TRANSLATE message");
	translate_begin(&t, host, TR_PORT, opt.translate_to, 0, src, n);
	t_folder = cur_folder;
	t_uid = cur_uid;
	running = 1;
	status("Translating...");
}

/* the message is gone (another shown, the reader cleared): what is left of
   its translation is dropped */
void xlate_cancel(void)
{
	if (running)
		status("");
	drop();
}

/* one piece, in a turn when no event waits (main.c) */
void xlate_step(void)
{
	int r;
	if (!running)
		return;
	if (!cur_msg || cur_folder != t_folder || cur_uid != t_uid) {
		xlate_cancel();
		return;
	}
	busy(1);
	r = translate_step(&t, &got);
	busy(0);
	conn_stop = 0;
	if (r == 1)
		return;		/* the next piece in the next turn */
	if (r < 0) {
		int stopped = !strcmp(t.err, "stopped");
		drop();
		status("");
		if (!stopped && !told) {
			told = 1;
			alert(1, "[1][The translation service on the|Gateway doesn't answer: is it on?|(%.40s)][ OK ]", t.err);
		} else if (!stopped) {
			status("Not translated: the Gateway doesn't answer.");
		}
		return;
	}
	/* done */
	{
		long an;
		char *a = cs_to_atari(got.s ? got.s : "", got.len, CS_UTF8, &an);
		int k = next_keep;
		running = 0;
		free(src);
		src = 0;
		sb_free(&got);
		told = 0;
		status("");
		if (!a)
			return;
		free(kept[k].text);
		kept[k].f = cur_folder;
		kept[k].uid = cur_uid;
		str_copy(kept[k].to, opt.translate_to, sizeof(kept[k].to));
		kept[k].text = a;
		kept[k].n = an;
		next_keep = (k + 1) % KEEP;
		show(a, an);
	}
}

/* Options > Translate switched */
void xlate_toggle(void)
{
	opt.translate = !opt.translate;
	store_save_settings();
	menu_update();
	if (opt.translate) {
		xlate_shown();
	} else {
		xlate_cancel();
		reader_original();
	}
}

/* the reader's right-click "Translate...": the chosen words, Atari text, in
   a box of their own; the letter stays as it is */
void xlate_words(const char *atari, long n)
{
	long un, an;
	char *u, *a, from[TR_LANG], err[160];
	int same = 0;
	SBUF out;
	const char *host = gateway();
	if (!host[0]) {
		alert(1, "[1][There is no account yet: the|translation service is on the|Gateway the accounts use.][ OK ]");
		return;
	}
	u = atari_to_utf8(atari, n, &un);
	if (!u)
		return;
	if (un > TR_PIECE_DEFAULT)
		un = translate_split(u, un, TR_PIECE_DEFAULT);
	sb_init(&out);
	busy(1);
	status("Translating...");
	from[0] = 0;
	err[0] = 0;
	TRACE("TRANSLATE words");
	if (!translate_piece(host, TR_PORT, opt.translate_to, 0, u, un, &out, from, &same, err, sizeof(err))) {
		busy(0);
		status("");
		free(u);
		sb_free(&out);
		if (strcmp(err, "stopped"))
			alert(1, "[1][The translation service on the|Gateway doesn't answer: is it on?|(%.40s)][ OK ]", err);
		conn_stop = 0;
		return;
	}
	busy(0);
	status("");
	free(u);
	a = cs_to_atari(out.s ? out.s : "", out.len, CS_UTF8, &an);
	sb_free(&out);
	if (a) {
		words_box(same ? "Already in that language" : "Translation", a, an);
		free(a);
	}
}
