/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * main.c - EMail: start-up, the menu bar, the event loop and the
 * commands behind menus and keys.
 */
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui.h"
#include "theme.h"
#include "../src/mail.h"
#include "../src/compose.h"
#include "conn.h"
#include "secret.h"
#include "busy.h"
#include "tls.h"
#include "people_file.h"
#include "../src/plat.h"
#include "util.h"
#include "crash.h"
#include "help.h"
#include "tlsovl.h"
#include "av.h"
#include "clip.h"
#include "notify.h"
#include "dd.h"
#include "aboutbox.h"
#include "mbar.h"
#include "dialog.h"
#include "../src/version.h"

short vdi_h, cw, ch, scr_w, scr_h, desk_x, desk_y, desk_w, desk_h, planes;
short gl_wchar, gl_hchar;
short ap_id;
short hebrew_kbd;
static unsigned long last_check;

void folders_init(void);
void list_init(void);
void reader_init(void);
void editor_init(void);

/* ---------------- help ---------------- */

/* the Help key: the user guide, EMAIL.HYP, in EHypView or ST-Guide
   (common/ui/help.c), else an alert saying how to get a viewer, which
   can show the keys instead */
static void help_key_pressed(void)
{
	if (help_key(opt.workdir, "EMAIL.HYP", 0, "Keys"))
		dlg_keys();
}

/* ---------------- menu ---------------- */


/* the menu bar, built by the AERA programs' shared builder
   (common/ui/mbar.c): titles, items, shortcuts, commands */
static const MBAR_ITEM m_desk[] = {
	{ "About EMail...", 0, C_ABOUT },
	{ 0, 0, 0 }
};
static const MBAR_ITEM m_file[] = {
	{ "Check mail", "\x01^N", C_CHECK },
	{ "Send Outbox", 0, C_SENDQ },
	{ "-", 0, 0 },
	{ "Save attachment...", 0, C_SAVEATT },
	{ "-", 0, 0 },
	{ "Close window", "^U", C_CLOSE },
	{ "Next window", "^W", C_NEXTWIN },
	{ "Quit", "^Q", C_QUIT },
	{ 0, 0, 0 }
};
/* a command on messages is in their menu, New message too (the
   guidelines, 3.1) */
static const MBAR_ITEM m_msg[] = {
	{ "New message", "^N", C_NEW },
	{ "-", 0, 0 },
	{ "Reply", "^R", C_REPLY },
	{ "Reply to all", "\x01^R", C_REPLYALL },
	{ "Forward", "\x01^F", C_FORWARD },
	{ "-", 0, 0 },
	{ "Mark as read", 0, C_MARKREAD },
	{ "Mark as unread", "\x01^U", C_UNREAD },
	{ "Flag", "^G", C_FLAG },
	{ "Move to...", "^M", C_MOVE },
	{ "Delete", "Del", C_DELETE },
	{ "Download again", 0, C_REFETCH },
	{ "-", 0, 0 },
	{ "Send now", "\x01^D", C_SEND },
	{ "Put in Outbox", "^S", C_SAVEOUT },
	{ "Attach file...", "^T", C_ATTACH },
	{ "Address book...", "^B", C_ABOOK },
	{ 0, 0, 0 }
};
static const MBAR_ITEM m_edit[] = {
	{ "Cut", "^X", C_CUT },
	{ "Copy", "^C", C_COPY },
	{ "Paste", "^V", C_PASTE },
	{ "-", 0, 0 },
	{ "Select all", "^A", C_SELALL },
	{ 0, 0, 0 }
};
static const MBAR_ITEM m_fold[] = {
	{ "Show folders", 0, C_FOLDERS },
	{ "Refresh folder list", 0, C_REFRESH },
	{ "New folder...", 0, C_NEWFOLDER },
	{ "Rename folder...", 0, C_RENFOLDER },
	{ "Delete folder...", 0, C_DELFOLDER },
	{ "-", 0, 0 },
	{ "Download missing messages", 0, C_GETALL },
	{ 0, 0, 0 }
};
static const MBAR_ITEM m_opts[] = {
	{ "Accounts...", 0, C_ACCOUNTS },
	{ "Settings...", "^,", C_SETTINGS },
	{ "Font...", 0, C_FONT },
	{ "-", 0, 0 },
	{ "Work offline", 0, C_OFFLINE },
	{ "Direct connection (TLS)", 0, C_FALCON },
	{ "Hebrew keyboard", "F10", C_HEBREW },
	{ "Protocol log", 0, C_LOG },
	{ 0, 0, 0 }
};
static const MBAR_ITEM m_help[] = {
	{ "EMail help", "Help", C_HELP },
	{ "Keys...", 0, C_KEYS },
	{ 0, 0, 0 }
};

static const MBAR_TITLE bar[] = {
	{ " EMail ", m_desk },
	{ " File ", m_file },
	{ " Edit ", m_edit },
	{ " Message ", m_msg },
	{ " Folder ", m_fold },
	{ " Options ", m_opts },
	{ " Help ", m_help },
	{ 0, 0 }
};

static MBAR mb;

static void enable(short cmd, int on)
{
	short i = mbar_object(&mb, cmd);
	if (i >= 0)
		menu_ienable(mb.tree, i, on ? 1 : 0);
}

void menu_update(void)
{
	int msg = cur_msg != 0, ed = w_editor.h > 0, ed_top = ed && win_topmost() == &w_editor;
	int some = msg || list_marked() > 1;	/* one message or a selection */
	enable(C_CUT, ed_top);
	enable(C_COPY, ed_top || msg || (win_topmost() == &w_folders && cur_finfo));
	enable(C_PASTE, ed_top);
	enable(C_REPLY, msg);
	enable(C_REPLYALL, msg);
	enable(C_FORWARD, msg);
	enable(C_MARKREAD, some);
	enable(C_UNREAD, some);
	enable(C_FLAG, some);
	enable(C_MOVE, some);
	enable(C_DELETE, some);
	enable(C_REFETCH, cur_finfo && !cur_finfo->local && (list_current() || list_marked() > 1));
	enable(C_GETALL, cur_finfo && !cur_finfo->local && !cur_finfo->noselect);
	enable(C_SELALL, cur_folder != 0);
	enable(C_SAVEATT, msg && cur_msg->nparts > 0);
	enable(C_NEXTWIN, ed && main_h > 0);
	enable(C_SEND, ed);
	enable(C_SAVEOUT, ed);
	enable(C_ATTACH, ed);
	enable(C_ABOOK, ed || naccts);	/* without a message: write to one */
	enable(C_NEWFOLDER, cur_acct && !cur_acct->pop);
	enable(C_DELFOLDER, cur_finfo && !cur_finfo->local && !cur_finfo->role);
	enable(C_RENFOLDER, cur_finfo && !cur_finfo->local && !cur_finfo->role && !cur_finfo->noselect);
	menu_icheck(mb.tree, mbar_object(&mb, C_OFFLINE), opt.offline);
	menu_icheck(mb.tree, mbar_object(&mb, C_FALCON), opt.falcon);
	menu_icheck(mb.tree, mbar_object(&mb, C_HEBREW), hebrew_kbd);
	menu_icheck(mb.tree, mbar_object(&mb, C_LOG), opt.log);
}

/* ---------------- status and helpers ---------------- */

void statusf(const char *fmt, ...)
{
	char t[120];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(t, sizeof(t), fmt, ap);
	va_end(ap);
	status(t);
}

void status(const char *msg)
{
	char tmp[120];
	snprintf(tmp, sizeof(tmp), " %s", msg);
	main_status(*msg ? tmp : "");
	if (*msg)
		crash_note(msg);
}

/* the bee; while it shows, Esc or Undo stops the network (busy.h) */
void busy(int on)
{
	busy_bee(on);
}

/* a password that can't be encrypted goes into EMAIL.INF as it is, and the
   user is told, once (the AERA guidelines, 7.2) */
static void warn_plain(const char *keyfile)
{
	alert(1, "[1][EMail can't write its key file, %s, so the passwords in EMAIL.INF are kept unencrypted. "
		 "Is the disk locked or full?][ OK ]", keyfile);
}

static void set_logging(void)
{
	conn_log_flush();
	if (opt.log) {
		const char *clock = tos_clock();
		path_join(conn_logfile, sizeof(conn_logfile), opt.workdir, "EMAIL.LOG");
		if (*clock)		/* a 200 Hz timer that isn't: the times are corrected */
			conn_log("EMail", " -- ", clock, (long)strlen(clock));
	} else {
		conn_logfile[0] = 0;
	}
}

/* Israeli SI-1452 layout by key position (scancode 0x10-0x35), giving
 * Atari character codes; 0 = key not remapped. From Claude ST. */
static const u8 hebrew_keys[0x36 - 0x10] = {
	/* 10 q..p */ '/', '\'', 0xD4, 0xD5, 0xC2, 0xCA, 0xC7, 0xD8, 0xDA, 0xD2,
	/* 1a [ ] ret ctrl */ 0, 0, 0, 0,
	/* 1e a..' */ 0xD6, 0xC5, 0xC4, 0xCC, 0xD1, 0xCB, 0xC9, 0xCD, 0xD9, 0xDB, ',',
	/* 29 ` lshift \ */ 0, 0, 0,
	/* 2c z../ */ 0xC8, 0xD0, 0xC3, 0xC6, 0xCF, 0xCE, 0xD3, 0xD7, 0xDC, '.'
};

unsigned char key_char(short kstate, short key)
{
	short scan = KEY_SCAN(key);
	unsigned char ascii = KEY_ASCII(key);
	if (hebrew_kbd && !(kstate & (K_LSHIFT | K_RSHIFT | K_ALT)) && !key_ctrl(kstate, key) &&
	    scan >= 0x10 && scan < 0x36 && hebrew_keys[scan - 0x10])
		ascii = hebrew_keys[scan - 0x10];
	return ascii;
}

void set_hebrew_kbd(short on)
{
	hebrew_kbd = on;
	menu_update();
}

/* folder lists can be rebuilt by the server: find the open one again */
void refind_current(const char *server)
{
	FOLDER *keep = cur_folder;
	if (!cur_acct)
		return;
	cur_finfo = server[0] ? folder_find(cur_acct, server) : 0;
	if (keep) {
		keep->fi = cur_finfo;
		if (!cur_finfo) {
			fold_close(keep);
			cur_folder = 0;
			reader_clear();
			list_load();
		}
	}
}

/* ---------------- commands ---------------- */

static int quiet_check;		/* the timed check: errors on the status line */

static int check_alerted;	/* cmd_check_all showed an alert */

/* new mail that came by itself, while EMail is behind another program
   or iconified, goes to BGEM.NG as a notification (the guidelines, 4.5);
   without BGEM.NG (TOS, MagiC) the status line says it, as before */
static ACCOUNT *news_acct;	/* the first account with new mail */
#ifdef DRAW_TRACE
static char *notify_test;
#endif

static int in_front(void)
{
	short top, d;
	if (main_h <= 0 || main_iconified)
		return 0;
	wind_get(0, WF_TOP, &top, &d, &d, &d);
	return top == main_h || (w_editor.h > 0 && top == w_editor.h);
}

/* Open: the main window up, with the Inbox of the account the news was
   for */
static void notified(short id, short button)
{
	FINFO *in;
	short i;
	(void)id;
	if (button != 1 || main_h <= 0)
		return;
	if (main_iconified) {
		short m[8];
		m[0] = WM_UNICONIFY;
		m[1] = ap_id;
		m[2] = 0;
		m[3] = main_h;
		wind_get(main_h, WF_UNICONIFY, &m[4], &m[5], &m[6], &m[7]);
		win_message(m);
	}
	wind_set(main_h, WF_TOP, 0, 0, 0, 0);
	for (i = 0; i < naccts && accts[i] != news_acct; i++)
		;
	if (i == naccts)
		i = 0;
	if (naccts && (in = folder_role(accts[i], FR_INBOX)) != 0)
		folders_select(accts[i], in);
	menu_update();
#ifdef DRAW_TRACE
	pf_debug("NOTIFY opened");
#endif
}

void cmd_check_all(void)
{
	short i;
	long total = 0;
	char server[160], errs[200] = "";
	str_copy(server, cur_finfo ? cur_finfo->server : "", sizeof(server));
	if (opt.offline) {
		check_alerted = 1;
		alert(1, "[1][EMail is working offline.|Switch it off in the Options|menu to check mail.][ OK ]");
		return;
	}
	busy(1);
	mail_new_other = 0;
	mail_new_folders = 0;
	for (i = 0; i < naccts; i++) {
		long n = 0;
		mail_err[0] = 0;
		if (!mail_check(accts[i], &n) && mail_err[0] && !errs[0])
			str_copy(errs, mail_err, sizeof(errs));
		if (n > 0 && !total)
			news_acct = accts[i];
		total += n;
	}
	busy(0);
	refind_current(server);
	folders_build();
	if (cur_folder && cur_finfo) {
		fold_done(cur_folder);	/* the check worked on it: saved once */
		list_refresh();
	}
	last_check = pf_ms();
	if (errs[0] && !quiet_check) {
		check_alerted = 1;
		alert(1, "[1][%s][ OK ]", errs);
	}
	if (errs[0]) {
		status(errs);
	} else {
		char m[120];
		snprintf(m, sizeof(m), total == 1 ? "1 new message" : "%s new messages", num(total));
		if (mail_new_folders == 1)
			snprintf(m + strlen(m), sizeof(m) - strlen(m), ", %s in %s", num(mail_new_other), mail_new_where);
		else if (mail_new_folders > 1)
			snprintf(m + strlen(m), sizeof(m) - strlen(m), ", %s in %d other folders",
				 num(mail_new_other), mail_new_folders);
		status(m);
		if (quiet_check && total > 0 && !in_front())
			notify_send("EMail", m, "Open|Later", 1);
	}
	menu_update();
}

static ACCOUNT *account_for_new(void)
{
	if (cur_acct)
		return cur_acct;
	return dlg_pick_account("Write from which account?");
}

/* ---------------- links (reader.c) ---------------- */

static short quit;

/* a link too long for a command line or VA_START: LINK.HTM in EMail's
   folder, a page that sends the browser on to it (a meta refresh) */
static const char *link_page(const char *url)
{
	static char path[220];
	SBUF b;
	const char *p;
	int ok;
	path_join(path, sizeof(path), opt.workdir, "LINK.HTM");
	sb_init(&b);
	sb_adds(&b, "<html><head><meta http-equiv=\"refresh\" content=\"0;url=");
	for (p = url; *p; p++) {
		if (*p == '&')
			sb_adds(&b, "&amp;");
		else if (*p == '"')
			sb_adds(&b, "&quot;");
		else
			sb_addc(&b, *p);
	}
	sb_adds(&b, "\"></head><body></body></html>\r\n");
	ok = b.s && pf_save(path, b.s, b.len) >= 0;
	sb_free(&b);
	return ok ? path : 0;
}

/* EWeb's program: the one EMail was told, else beside EMail, else
   asked for once (and kept in EMAIL.INF) */
static int find_browser(char *out, int size)
{
	/* EWeb's ST edition first on a 68000 */
	static const char *const names[2][2] = { { "EWEBST.PRG", "EWEB.PRG" }, { "EWEB.PRG", "EWEBST.PRG" } };
	int big = get_cookie(0x5F435055L) >= 20;	/* _CPU: a 68020 or better */
	char path[200], name[16];
	short button = 0, k;
	char *e;
	if (opt.browser[0] && pf_exists(opt.browser)) {
		str_copy(out, opt.browser, size);
		return 1;
	}
	for (k = 0; k < 2; k++) {
		path_join(out, size, opt.workdir, names[big][k]);
		if (pf_exists(out))
			return 1;
	}
	str_copy(path, opt.workdir[0] ? opt.workdir : "C:", sizeof(path) - 4);
	strcat(path, "\\*.PRG");
	strcpy(name, names[big][0]);
	if (!fsel_exinput(path, name, &button, "Where is EWeb (EWEB.PRG)?") || !button || !name[0])
		return 0;
	if ((e = strrchr(path, '\\')) != 0)
		e[1] = 0;
	str_copy(out, path, size);
	strncat(out, name, size - strlen(out) - 1);
	if (!pf_exists(out))
		return 0;
	str_copy(opt.browser, out, sizeof(opt.browser));
	store_save_settings();
	return 1;
}

/* EWeb started on url: beside EMail where the AES runs both (MagiC,
   MultiTOS, XaAES); on plain TOS in its place, once EMail has quit */
static void start_browser(const char *url)
{
	int multi = get_cookie(0x4D616758L) != 0 ||				/* MagX */
		    (get_cookie(0x4D694E54L) != 0 && aes_version() >= 0x0400);	/* MiNT */
	char prg[200], tail[128], dir[200], *e;
	const char *arg = url;
	short r, len;
	r = alert(1, multi ? "[2][No web browser is running.|Start EWeb on this link?][Open in EWeb|Copy link|Cancel]"
			   : "[2][No web browser is running.|EMail can close and start|EWeb on this link.]"
			     "[Open in EWeb|Copy link|Cancel]");
	if (r == 2) {
		main_status(clip_put(url, (long)strlen(url)) ? "The link is on the clipboard." :
							       "Couldn't write the clipboard.");
		return;
	}
	if (r != 1 || !find_browser(prg, sizeof(prg)))
		return;
	if (!multi && editor_dirty() &&
	    alert(2, "[2][You are still writing a message.|Quit anyway?][Quit|Cancel]") != 1)
		return;
	/* a command line holds 125 characters: a longer link goes by a page */
	if (strlen(url) > 120 && !(arg = link_page(url))) {
		alert(1, "[1][Can't write LINK.HTM|in EMail's folder.][ OK ]");
		return;
	}
	len = (short)strlen(arg);
	tail[0] = (char)len;
	memcpy(tail + 1, arg, len + 1);
	/* EWeb keeps its files in the folder it starts in: that one */
	str_copy(dir, prg, sizeof(dir));
	if ((e = strrchr(dir, '\\')) != 0)
		e[1] = 0;
	if (dir[0] && dir[1] == ':') {
		Dsetdrv((dir[0] & ~0x20) - 'A');
		Dsetpath(dir + 2);
	}
	if (!shel_write(1, 1, 0, prg, tail)) {
		alert(1, "[1][Couldn't start|%s.][ OK ]", prg);
		return;
	}
	if (multi) {
		main_status("EWeb is starting on the link.");
	} else {
		main_status("Quitting: EWeb starts on the link.");
		quit = 1;
	}
}

/* a link in a message: mailto: starts a new message; a web address goes
   to the browser running (EWeb, CAB, HighWire), else EWeb is offered */
void open_link(const char *url)
{
	const char *arg = url;
	if (!strncasecmp(url, "mailto:", 7)) {
		ACCOUNT *a;
		if (!naccts) {
			alert(1, "[1][Set up your mail account first:|Options > Accounts.][ OK ]");
			return;
		}
		if ((a = account_for_new()) != 0)
			editor_open(a, compose_mailto(a, url + 7), 0, 0);
		return;
	}
	/* VA_START carries 1 KB */
	if (strlen(url) > 1000 && !(arg = link_page(url)))
		arg = url;
	if (av_send("EWEB|EWEBST|CAB|HIGHWIRE", arg)) {
		main_status("The link is open in the web browser.");
		return;
	}
	start_browser(url);
}

/* VA_START (common/ui/av.c), from EContact's mail button, the desktop or
   another program: "mailto:..." starts a new message; file names are
   attached to the message being written, or to a new one; nothing brings
   the window to the front */
/* ---------------- EContact (the guidelines, 4.1) ----------------
 * People are shown by the names EContact has for them (people_ask.c asks
 * it in PEOPLE messages), and picked in its window */
const char *person_shown(const char *addr)
{
	const char *n = people_name(addr);
	return n && *n ? n : addr;
}

/* names that came while a dialog was up: drawn after it (the windows
   would be drawn over it) */
static int in_modal, names_wait;

static void names_came(void)
{
	if (in_modal) {
		names_wait = 1;
		return;
	}
	editor_names_came();
	reader_names_came();
	list_names_came();
}

static void picked(const PERSON *list, int n)
{
	editor_picked(list, n);
}

/* EContact's CONTACTS.TXT when it isn't loaded, if the user chose it
   (Settings > Contacts; the guidelines, 4.1). The people dialog on the
   file hands its messages here: kept for the main loop, as a dialog's */
static void file_message(short *m)
{
	if (m)
		ui_modal_message(m);
}

static void people_setup(void)
{
	people_file(opt.peoplefile && opt.peopledir[0] ? opt.peopledir : 0, file_message);
}

/* a modal loop's messages (a dialog's, the font list's): PEOPLE is read
   at once, as the guidelines ask, and the rest wait for the main loop */
static short kept[8][8];
static int nkept;
static char *kept_start;		/* a VA_START that came meanwhile */

void ui_modal_message(const short *msg)
{
	const char *s;
	in_modal = 1;
	switch (av_message(msg, &s)) {
	case AV_PEOPLE:
		people_message(s, msg[1]);
		break;
	case AV_START:
		if (!kept_start)
			kept_start = strdup(s);
		break;
	case AV_DONE:
		break;
	default:
		if (nkept < 8)
			memcpy(kept[nkept++], msg, sizeof(kept[0]));
	}
	people_poll();
	in_modal = 0;
}

/* the redraws the AES has queued, done now: a window closed before a
   long wait leaves the screen first. Other messages are kept for later */
void ui_redraws(void)
{
	short m[8];
	AEVENT e;
	int i;
	for (i = 0; i < 16; i++) {
		evnt_multi_(MU_MESAG | MU_TIMER, 0, 0, 0, 0, m, &e);
		if (!(e.which & MU_MESAG))
			break;
		if (m[0] == WM_REDRAW)
			win_message(m);
		else
			ui_modal_message(m);
	}
}

/* a message while the network works (busy_idle): a redraw now, as a
   multitasking AES (MiNT with XaAES or MyAES, MagiC) shows what is behind
   a window until it is drawn, and other programs go on moving their
   windows meanwhile; everything else waits for the main loop */
static void busy_msg(const short *msg)
{
	if (msg[0] == WM_REDRAW)
		win_message((short *)msg);
	else
		ui_modal_message(msg);
}

/* EMail's windows drawn again while a dialog of the AES's is open over
   them (MiNT, MagiC: dialog.h) */
static void dialog_redraw(const short *msg)
{
	win_message((short *)msg);
}

static void av_start(const char *s)
{
	char path[200];
	const char *p = s;
	ACCOUNT *a;
	main_open();
	if (!*s)
		return;
	if (!naccts) {
		alert(1, "[1][Set up your mail account first:|Options > Accounts.][ OK ]");
		return;
	}
	if (!strncasecmp(s, "mailto:", 7)) {
		a = account_for_new();
		if (a)
			editor_open(a, compose_mailto(a, s + 7), 0, 0);
		return;
	}
	while ((p = av_arg(p, path, sizeof(path))) != 0) {
		if (!pf_exists(path)) {
			alert(1, "[1][Can't attach %s:|it isn't there.][ OK ]", path);
			continue;
		}
		if (w_editor.h <= 0) {
			a = account_for_new();
			if (!a)
				return;
			editor_open(a, compose_new(a, ""), 0, 0);
			if (w_editor.h <= 0)
				return;
		}
		editor_attach_file(path);
	}
	if (w_editor.h > 0)
		win_top(&w_editor);
}

static void cmd_reply(int all, int forward)
{
	ACCOUNT *a = cur_acct;
	char *text, refs[1100];
	if (reader_missing) {
		alert(1, "[1][This message hasn't been|downloaded yet: check mail|to connect, then open it.][ OK ]");
		return;
	}
	if (!cur_msg || !a)
		return;
	if (forward) {
		char path[220];
		int have = 0;
		if (cur_msg->nparts) {
			path_join(path, sizeof(path), cur_folder->dir, "FORWARD.EML");
			have = msg_export(cur_folder, cur_uid, path) == 0;
		}
		text = compose_forward(a, cur_msg, have ? path : 0);
		editor_open(a, text, 0, 0);
		return;
	}
	text = compose_reply(a, cur_msg, all);
	snprintf(refs, sizeof(refs), "%s%s%s", cur_msg->references,
		 cur_msg->references[0] ? " " : "", cur_msg->message_id);
	abook_add(cur_msg->reply_to[0] ? cur_msg->reply_to : cur_msg->from);
	editor_open(a, text, cur_msg->message_id, refs);
}

/* Delete, Move and the marks work on the selected messages, or the
   current one. Their UIDs are taken first: the list changes as they go. */
static void bulk_status(const char *verb, const char *rest, long k, long n)
{
	if (n > 1)		/* "Moving 3 of 12 to Archive..." */
		statusf("%s %s of %s%s...", verb, num(k + 1), num(n), rest);
	else
		statusf("%s the message%s...", verb, rest);
}

static void cmd_delete(void)
{
	unsigned long *u;
	long n, k, done = 0;
	if (!cur_folder || !(u = list_targets(&n)))
		return;
	if (cur_finfo->role == FR_TRASH || n > 1) {
		short b = cur_finfo->role == FR_TRASH
			? (n > 1 ? alert(2, "[2][Delete these %s messages|for good?][Delete|Cancel]", num(n))
				 : alert(2, "[2][Delete this message|for good?][Delete|Cancel]"))
			: alert(2, "[2][Delete these %s messages?][Delete|Cancel]", num(n));
		if (b != 1) {
			free(u);
			return;
		}
	}
	busy(1);
	mail_err[0] = 0;
	for (k = 0; k < n; k++) {
		HDR *h = fold_get(cur_folder, u[k]);
		bulk_status("Deleting", "", k, n);
		if (!h)
			continue;
		if (!mail_delete(cur_folder, h))
			break;
		done++;
	}
	busy(0);
	free(u);
	status("");
	if (k < n)
		alert(1, "[1][%s][ OK ]", mail_err[0] ? mail_err : "Can't delete the message.");
	list_unmark();
	reader_clear();
	if (done)
		list_after_remove();
	menu_update();
}

/* the messages u[0..n) of the open folder into dest (Move to, and a drop
   on a folder); frees u */
void move_messages(unsigned long *u, long n, FINFO *dest)
{
	long k, done = 0;
	char title[60];
	busy(1);
	mail_err[0] = 0;
	snprintf(title, sizeof(title), " to %s", dest->disp);
	for (k = 0; k < n; k++) {
		HDR *h = fold_get(cur_folder, u[k]);
		bulk_status("Moving", title, k, n);
		if (!h)
			continue;
		if (!mail_move(cur_folder, h, dest))
			break;
		done++;
	}
	busy(0);
	free(u);
	status("");
	if (k < n)
		alert(1, "[1][%s][ OK ]", mail_err[0] ? mail_err : "Can't move the message.");
	list_unmark();
	reader_clear();
	if (done)
		list_after_remove();
	folders_build();
	menu_update();
}

static void cmd_move(void)
{
	unsigned long *u;
	long n;
	FINFO *dest;
	char title[60];
	if (!cur_folder || !(u = list_targets(&n)))
		return;
	if (n > 1)
		snprintf(title, sizeof(title), "Move %s messages to", num(n));
	else
		str_copy(title, "Move the message to", sizeof(title));
	dest = dlg_pick_folder(cur_acct, title);
	if (!dest) {
		free(u);
		return;
	}
	move_messages(u, n, dest);
}

/* read/unread, or the flag: for several messages the flag goes on all of
   them unless they all have it already, then off */
static void cmd_flag(unsigned short flag, int add)
{
	unsigned long *u;
	long n, k;
	int several;
	if (!cur_folder || !(u = list_targets(&n)))
		return;
	several = n > 1;
	if (flag == MF_FLAGGED) {
		add = 0;
		for (k = 0; k < n; k++) {
			HDR *h = fold_get(cur_folder, u[k]);
			if (h && !(h->flags & MF_FLAGGED))
				add = 1;
		}
	}
	mail_err[0] = 0;
	busy(1);
	for (k = 0; k < n; k++) {
		HDR *h = fold_get(cur_folder, u[k]);
		bulk_status(flag == MF_FLAGGED ? (add ? "Flagging" : "Unflagging") : "Marking",
			    flag == MF_FLAGGED ? "" : add ? " as read" : " as unread", k, n);
		if (h && !mail_flag(cur_folder, h, flag, add))
			break;
	}
	busy(0);
	status("");
	free(u);
	if (k < n)
		alert(1, "[1][%s][ OK ]", mail_err);
	list_refresh();
	if (several)
		list_titles();
	folders_build();
}

/* Message > Download again: the server's copy of the messages in place of
   the one here, which may have come wrong (cut off by a bad line) */
static void cmd_download_again(void)
{
	unsigned long *u;
	long n, k;
	HDR *h;
	if (!cur_folder || cur_finfo->local || !(u = list_targets(&n)))
		return;
	busy(1);
	mail_err[0] = 0;
	for (k = 0; k < n; k++) {
		char what[60];
		h = fold_get(cur_folder, u[k]);
		if (n > 1)
			snprintf(what, sizeof(what), "Downloading again: %s of %s", num(k + 1), num(n));
		else
			str_copy(what, "Downloading the message again", sizeof(what));
		if (h && !mail_download(cur_folder, h, 1, what))
			break;
	}
	busy(0);
	status("");
	if (k < n)
		alert(1, "[1][%s][ OK ]", mail_err[0] ? mail_err : "Can't download the message.");
	/* the one on the screen, as it is now */
	h = list_current();
	for (k = 0; h && k < n; k++)
		if (u[k] == h->uid) {
			reader_show(h);
			break;
		}
	free(u);
	menu_update();
}

/* Folder > Download missing messages: every message of the folder on
   the Atari, to read without the server, fetching only those it hasn't
   got; the folder mirrors all of them from now on, not the newest
   opt.page */
void cmd_download_all(ACCOUNT *a, FINFO *fi)
{
	FOLDER *f;
	long k, n = 0, done = 0, had = 0, i;
	int ok = 1, cur = fi == cur_finfo;
	char what[80], here[16];
	if (!a || !fi || fi->local || fi->noselect)
		return;
	/* the folder stays open through the job: one FOLDER, the screen's
	   when it is the one shown, saved once at the end */
	f = fold_open(a, fi);
	for (i = 0; f && i < f->n; i++)
		if (f->h[i].flags & MF_CACHED)
			had++;
	if (had)
		snprintf(here, sizeof(here), "%s %s", num(had), had == 1 ? "is" : "are");
	if (fi->total > 0)
		snprintf(what, sizeof(what), "|The server has %s message%s;|%s on the Atari %s.", num(fi->total),
			 fi->total == 1 ? "" : "s", had ? here : "none are", had ? "already" : "yet");
	else
		snprintf(what, sizeof(what), "|%s on the Atari %s.", had ? here : "None are", had ? "already" : "yet");
	if (alert(1, "[2][Download what isn't on the|Atari yet of %s?%s%s][Download|Cancel]", fi->disp, what,
		  opt.keepcache ? "" : "|(Kept only until EMail quits.)") != 1) {
		fold_close(f);
		return;
	}
	busy(1);
	mail_err[0] = 0;
	ok = f ? mail_mirror_all(a, fi) : 0;
	if (f) {
		for (i = 0; i < f->n; i++)
			if (!(f->h[i].flags & MF_CACHED))
				n++;
		for (i = 0, k = 0; ok && i < f->n; i++) {
			HDR *h = &f->h[i];
			char what[80];
			if (h->flags & MF_CACHED)
				continue;
			snprintf(what, sizeof(what), "%s: downloading %s of %s", fi->disp, num(k + 1), num(n));
			k++;
			ok = mail_download(f, h, 0, what);
			if (ok)
				done++;
		}
	}
	busy(0);
	status("");
	if (!f)
		alert(1, "[1][Not enough memory|to open %s.][ OK ]", fi->disp);
	else if (!ok && !strcmp(mail_err, "stopped"))
		alert(1, "[1][Stopped: %s of %s|messages downloaded.][ OK ]", num(done), num(n));
	else if (!ok)
		alert(1, "[1][%s][ OK ]", mail_err[0] ? mail_err : "Can't download the messages.");
	else if (!done)
		alert(1, "[1][All %s messages of %s|were on the Atari already:|nothing to download.][ OK ]", num(f->n), fi->disp);
	else
		alert(1, "[1][%s downloaded: all %s|messages of %s|are on the Atari.][ OK ]", num(done), num(f->n), fi->disp);
	fold_done(f);			/* the pack, then the index: once */
	fold_close(f);
	if (cur) {
		list_refresh();
		win_redraw(&w_list, 0);
	}
	folders_build();
	menu_update();
}

static void cmd_save_attachment(void)
{
	short i;
	if (!cur_msg || !cur_msg->nparts)
		return;
	if (cur_msg->nparts == 1) {
		reader_save_attachment(0);
		return;
	}
	for (i = 0; i < cur_msg->nparts; i++) {
		short b = alert(1, "[2][Save the attachment|%s?][Save|Skip|Stop]", cur_msg->parts[i].name);
		if (b == 3)
			break;
		if (b == 1)
			reader_save_attachment(i);
	}
}

static void cmd_accounts(void)
{
	ACCOUNT *a;
	short r;
	if (naccts) {
		short b = alert(1, "[2][Accounts][Edit|New|Cancel]");
		if (b == 3)
			return;
		a = b == 1 ? dlg_pick_account("Which account?") : acct_new();
	} else {
		a = acct_new();
	}
	if (!a) {
		if (naccts >= MAXACCT)
			alert(1, "[1][EMail has room for %d accounts.][ OK ]", MAXACCT);
		return;
	}
	r = (short)dlg_account(a);
	if (r < 0 || (r == 0 && !a->host[0])) {
		/* deleted, or a new account cancelled */
		if (cur_acct == a) {
			if (cur_folder)
				fold_close(cur_folder);
			cur_folder = 0;
			cur_finfo = 0;
			cur_acct = 0;
			reader_clear();
			list_load();
		}
		acct_delete(a);
	} else if (r > 0) {
		mail_disconnect(a);
		folders_load(a);
	}
	if (r)			/* Cancel writes nothing (3.5): not even the passwords again */
		store_save_settings();
	folders_build();
	menu_update();
	if (r > 0 && !opt.offline &&
	    alert(1, "[2][Check this account now?][Check|Later]") == 1) {
		long n;
		char server[160];
		str_copy(server, cur_finfo ? cur_finfo->server : "", sizeof(server));
		busy(1);
		mail_err[0] = 0;
		if (!mail_check(a, &n) && mail_err[0])
			alert(1, "[1][%s][ OK ]", mail_err);
		busy(0);
		if (cur_acct == a)
			refind_current(server);
		folders_build();
	}
}

void cmd_new_folder(ACCOUNT *a)
{
	char name[sizeof(((FINFO *)0)->disp)] = "";
	if (!a || a->pop)
		return;
	if (!dlg_ask("New folder", "Name:", name, sizeof(name) - 1))
		return;
	{
		char server[160];
		str_copy(server, cur_finfo ? cur_finfo->server : "", sizeof(server));
		busy(1);
		statusf("%s: creating the folder %s...", a->name, name);
		mail_err[0] = 0;
		if (!mail_folder_create(a, name))
			alert(1, "[1][%s][ OK ]", mail_err);
		busy(0);
		status("");
		refind_current(server);
	}
	folders_build();
}

void cmd_rename_folder(ACCOUNT *a, FINFO *fi)
{
	char name[sizeof(((FINFO *)0)->disp)], server[160], to[160];
	int was_cur;
	if (!a || !fi || fi->local || fi->role)
		return;
	str_copy(name, fi->disp, sizeof(name));
	if (!dlg_ask("Rename folder", "New name:", name, sizeof(name) - 1) || !strcmp(name, fi->disp))
		return;
	was_cur = fi == cur_finfo;
	str_copy(server, cur_finfo ? cur_finfo->server : "", sizeof(server));
	busy(1);
	statusf("%s: renaming %s to %s...", a->name, fi->disp, name);
	mail_err[0] = 0;
	to[0] = 0;
	if (!mail_folder_rename(a, fi, name, to, sizeof(to)) && mail_err[0])
		alert(1, "[1][%s][ OK ]", mail_err);
	busy(0);
	status("");
	/* the open folder is found again by its (perhaps new) name */
	if (cur_acct == a)
		refind_current(was_cur && to[0] ? to : server);
	list_titles();
	folders_build();
	menu_update();
}

void cmd_delete_folder(ACCOUNT *a, FINFO *fi)
{
	char server[160];
	if (!a || !fi || fi->local || fi->role)
		return;
	if (alert(2, "[2][Delete the folder|\"%s\"|and all its messages|on the server?][Delete|Cancel]",
		  fi->disp) != 1)
		return;
	str_copy(server, cur_finfo && cur_finfo != fi ? cur_finfo->server : "", sizeof(server));
	if (fi == cur_finfo) {
		if (cur_folder)
			fold_close(cur_folder);
		cur_folder = 0;
		cur_finfo = 0;
		reader_clear();
		list_load();
	}
	busy(1);
	statusf("%s: deleting the folder %s...", a->name, fi->disp);
	mail_err[0] = 0;
	if (!mail_folder_delete(a, fi))
		alert(1, "[1][%s][ OK ]", mail_err);
	busy(0);
	status("");
	if (cur_acct == a)
		refind_current(server);
	folders_build();
	menu_update();
}

/* edit one account (from its right-click menu) */
void cmd_edit_account(ACCOUNT *a)
{
	short r = (short)dlg_account(a);
	if (r < 0) {
		if (cur_acct == a) {
			if (cur_folder)
				fold_close(cur_folder);
			cur_folder = 0;
			cur_finfo = 0;
			cur_acct = 0;
			reader_clear();
			list_load();
		}
		acct_delete(a);
	} else if (r > 0) {
		mail_disconnect(a);
	}
	if (r)			/* Cancel writes nothing (3.5) */
		store_save_settings();
	folders_build();
	menu_update();
}

void cmd_refresh_folders(ACCOUNT *a)
{
	char server[160];
	str_copy(server, cur_finfo ? cur_finfo->server : "", sizeof(server));
	busy(1);
	mail_err[0] = 0;
	if (!mail_refresh_folders(a) && mail_err[0])
		alert(1, "[1][%s][ OK ]", mail_err);
	busy(0);
	if (cur_acct == a)
		refind_current(server);
	folders_build();
}

static unsigned long last_input;	/* pf_ms() of the last key, click or message */

/* ---------------- tooltips (as EFTP's and ECal's) ----------------
 * shown when the mouse rests on a button of the editor's bar, a name in
 * a message's header, or a name or subject the list or the folder pane
 * cuts short; gone when it moves, a key is pressed or a button clicked */
static short tip_on, tip_tried, hover_x = -1, hover_y = -1;
static unsigned long hover_at;
static GRECT tip_r;

static void tip_hide(void)
{
	if (!tip_on)
		return;
	tip_on = 0;
	/* whatever is under it, in any window, is drawn again */
	form_dial(FMD_FINISH, tip_r.x, tip_r.y, tip_r.w, tip_r.h);
}

static void tip_show(void)
{
	char text[120];
	short n;
	GRECT r, scr;
	tip_tried = 1;
	if (!editor_tip(hover_x, hover_y, text, sizeof(text)) &&
	    !(wind_find(hover_x, hover_y) == main_h &&
	      (reader_tip(hover_x, hover_y, text, sizeof(text)) || list_tip(hover_x, hover_y, text, sizeof(text)) ||
	       folders_tip(hover_x, hover_y, text, sizeof(text)))))
		return;
#ifdef DRAW_TRACE
	{
		char t[140];
		snprintf(t, sizeof(t), "TIP %s", text);
		pf_debug(t);
	}
#endif
	n = (short)strlen(text);
	if (n > (desk_w - 2 * MARGIN - 2) / cw)	/* as much as the screen's width holds */
		n = (short)((desk_w - 2 * MARGIN - 2) / cw);
	r.w = n * cw + 2 * MARGIN;
	r.h = ch + 6;
	r.x = hover_x + cw;
	r.y = hover_y + ch + 2;
	/* on the screen: to the left or above the mouse at the edges */
	if (r.x + r.w > desk_x + desk_w)
		r.x = hover_x - r.w - cw / 2;
	if (r.x < desk_x)
		r.x = desk_x;
	r.x &= ~7;		/* its text on a byte */
	if (r.y + r.h > desk_y + desk_h)
		r.y = hover_y - r.h - 2;
	if (r.y < desk_y)
		r.y = desk_y;
	tip_r = r;
	tip_r.w++;
	tip_r.h++;
	scr.x = scr.y = 0;
	scr.w = scr_w;
	scr.h = scr_h;
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, 0);
	clip_on(&scr);
	fill(&r, 0);
	frame(&r, 1);
	text_at(r.x + MARGIN, r.y + 3, text, n, n, 0);	/* a Hebrew subject right to left */
	graf_mouse(M_ON, 0);
	wind_update(END_UPDATE);
	tip_on = 1;
}

static void wait_release(void)
{
	short mx, my, mb, ks;
	for (;;) {
		graf_mkstate(&mx, &my, &mb, &ks);
		if (!(mb & 3))
			return;
		evnt_timer_(10);
	}
}

/* the "any button" event can't count clicks: see whether this press
   follows the last one closely enough to be a double click */
static short double_click(short mx, short my)
{
	static unsigned long last;
	static short lx, ly;
	unsigned long now = pf_ms();
	short n = (now - last < 400 && mx - lx < 4 && lx - mx < 4 && my - ly < 4 && ly - my < 4) ? 2 : 1;
	last = n == 2 ? 0 : now;
	lx = mx;
	ly = my;
	return n;
}

static void do_quit(void)
{
	if (editor_dirty() &&
	    alert(2, "[2][You are still writing a message.|Quit anyway?][Quit|Cancel]") != 1)
		return;
	quit = 1;
}

/* closing the main window ends EMail */
void main_closed(void);
void main_closed(void)
{
	do_quit();
}

static void command(short cmd);
void ui_command(short cmd)
{
	command(cmd);
}

/* the message shown, as text: its header lines, then what it says */
static void copy_message(void)
{
	MSG *m = cur_msg;
	char *t;
	long n;
	if (!m)
		return;
	n = 200 + (long)strlen(m->from ? m->from : "") + (long)strlen(m->to ? m->to : "") +
	    (long)strlen(m->cc ? m->cc : "") + (long)strlen(m->subject ? m->subject : "") + m->textlen;
	if (!(t = malloc(n))) {
		alert(1, "[1][Not enough memory to copy|this message.][ OK ]");
		return;
	}
	snprintf(t, n, "From: %s\nTo: %s\n%s%s%sDate: %s\nSubject: %s\n\n",
		 m->from ? m->from : "", m->to ? m->to : "",
		 m->cc && *m->cc ? "Cc: " : "", m->cc && *m->cc ? m->cc : "", m->cc && *m->cc ? "\n" : "",
		 m->date ? m->date : "", m->subject ? m->subject : "");
	if (m->text)
		memcpy(t + strlen(t), m->text, m->textlen + 1);
	main_status(clip_put(t, (long)strlen(t)) ? "Copied to the clipboard." : "Couldn't write the clipboard.");
	free(t);
}

/* ^C ^X ^V: in the message being written (the line the cursor is in when
   nothing is chosen); else ^C copies the message shown, or the folder's
   name when the folder list has the keyboard */
static void clip_command(short cmd)
{
	WIN *top = win_topmost();
	int what = cmd == C_COPY ? CLIP_COPY : cmd == C_CUT ? CLIP_CUT : CLIP_PASTE;
	if (top == &w_editor) {
		editor_clip(what);
	} else if (what == CLIP_COPY && top == &w_folders && cur_finfo) {
		if (clip_put(cur_finfo->disp, (long)strlen(cur_finfo->disp)))
			main_status("Copied to the clipboard.");
	} else if (what == CLIP_COPY) {
		copy_message();
	}
}

/* text dropped (drag and drop): into the message being written at the
   cursor, else a new message with it */
static void drop_text(const char *s)
{
	ACCOUNT *a;
	if (w_editor.h > 0) {
		win_top(&w_editor);
		editor_insert_text(s);
	} else if ((a = account_for_new()) != 0) {
		editor_open(a, compose_new(a, ""), 0, 0);
		editor_insert_body(s);
	}
}

static void command(short cmd)
{
	switch (cmd) {
	case C_ABOUT: dlg_about(); break;
	case C_HELP: help_key_pressed(); break;
	case C_KEYS: dlg_keys(); break;
	case C_CUT:
	case C_COPY:
	case C_PASTE: clip_command(cmd); break;
	case C_NEW: {
		ACCOUNT *a = account_for_new();
		if (a)
			editor_open(a, compose_new(a, ""), 0, 0);
		break;
	}
	case C_CHECK: cmd_check_all(); break;
	case C_SENDQ: {
		short i;
		busy(1);
		for (i = 0; i < naccts; i++) {
			int sent;
			mail_err[0] = 0;
			if (mail_send_outbox(accts[i], &sent) < 0 && mail_err[0]) {
				busy(0);
				alert(1, "[1][%s][ OK ]", mail_err);
				busy(1);
			}
		}
		busy(0);
		folders_build();
		break;
	}
	case C_SAVEATT: cmd_save_attachment(); break;
	case C_QUIT: do_quit(); break;
	case C_CLOSE:
		/* the editor; the main window closing ends EMail */
		if (win_topmost() == &w_editor) {
			if (w_editor.closed)
				w_editor.closed(&w_editor);
			else
				win_close(&w_editor);
		} else {
			main_closed();
		}
		break;
	case C_NEXTWIN: {
		/* EMail's other window on top, in turn: the main window and
		   the editor (the guidelines, 3.2); with one, nothing */
		short top, d;
		wind_get(0, WF_TOP, &top, &d, &d, &d);
		if (main_h > 0 && w_editor.h > 0)
			wind_set(top == main_h ? w_editor.h : main_h, WF_TOP, 0, 0, 0, 0);
		break;
	}
	case C_REPLY: cmd_reply(0, 0); break;
	case C_REPLYALL: cmd_reply(1, 0); break;
	case C_FORWARD: cmd_reply(0, 1); break;
	case C_UNREAD: cmd_flag(MF_SEEN, 0); break;
	case C_MARKREAD: cmd_flag(MF_SEEN, 1); break;
	case C_FLAG: cmd_flag(MF_FLAGGED, 1); break;
	case C_MOVE: cmd_move(); break;
	case C_DELETE: cmd_delete(); break;
	case C_REFETCH: cmd_download_again(); break;
	case C_GETALL: cmd_download_all(cur_acct, cur_finfo); break;
	case C_SELALL: list_select_all(); break;
	case C_SEND: editor_send(1); break;
	case C_SAVEOUT: editor_send(0); break;
	case C_ATTACH: editor_attach(); break;
	case C_ABOOK:
		if (w_editor.h > 0) {
			editor_insert_address();
		} else {			/* a new message, to the one chosen */
			ACCOUNT *a = account_for_new();
			if (a) {
				editor_open(a, compose_new(a, ""), 0, 0);
				editor_insert_address();
			}
		}
		break;
	case C_FOLDERS: main_open(); break;
	case C_REFRESH: {
		short i;
		char server[160];
		str_copy(server, cur_finfo ? cur_finfo->server : "", sizeof(server));
		busy(1);
		for (i = 0; i < naccts; i++) {
			mail_err[0] = 0;
			if (!mail_refresh_folders(accts[i]) && mail_err[0]) {
				busy(0);
				alert(1, "[1][%s][ OK ]", mail_err);
				busy(1);
			}
		}
		busy(0);
		refind_current(server);
		folders_build();
		break;
	}
	case C_NEWFOLDER: cmd_new_folder(cur_acct); break;
	case C_DELFOLDER: cmd_delete_folder(cur_acct, cur_finfo); break;
	case C_RENFOLDER: cmd_rename_folder(cur_acct, cur_finfo); break;
	case C_ACCOUNTS: cmd_accounts(); break;
	case C_SETTINGS:
		dlg_falcon = opt.falcon;
		if (dlg_settings()) {
			if (dlg_falcon != opt.falcon)
				falcon_mode_set(dlg_falcon);
			falcon_dsp_init();
			people_setup();
			set_logging();
			store_save_settings();
			if (w_editor.h <= 0)
				hebrew_kbd = opt.hebrew;
			theme_init();
			win_redraw_all();
			list_refresh();
			win_redraw(&w_reader, 0);
		}
		break;
	case C_FALCON:
		falcon_mode_set(!opt.falcon);
		list_titles();
		break;
	case C_OFFLINE:
		/* for this session (how EMail starts is Settings > Mail's):
		   offline, the connections close and the timed checks stop;
		   online, a check at once (the folder list, the Inbox, the
		   counts, the Outbox) and the timed checks from then on */
		opt.offline = !opt.offline;
		list_titles();
		menu_update();
		if (opt.offline)
			mail_disconnect_all();
		else
			cmd_check_all();
		break;
	case C_HEBREW: set_hebrew_kbd(!hebrew_kbd); break;
	case C_FONT: font_menu(); break;
	case C_LOG:
		opt.log = !opt.log;
		set_logging();
		store_save_settings();
		break;
	}
	menu_update();
}

/* ---------------- Direct connection (FALCON= in EMAIL.INF) ----------------
 * On: EMail talks TLS to the providers itself, with TLS.OVL (BearSSL on a
 * 68020 or better, RSA signatures on a Falcon's DSP; the AERA guidelines,
 * 7.4). Off: plain, through the Raspberry Pi gateway. Each account keeps
 * both sets of servers. */
short dlg_falcon;

int falcon_mode_set(int on)
{
	if (on) {
		char missing[200];
		int i;
		char err[160];
		if (get_cookie(0x5F435055L) < 20) {	/* _CPU */
			alert(1, "[3][A Direct connection needs a|68020 or better (Falcon, TT):|this Atari is too slow for|TLS. The Pi gateway does it.][ OK ]");
			return 0;
		}
		/* TLS.OVL, said at once rather than at the first connection */
		busy(1);
		if (!tls_ovl_load(err, sizeof(err))) {
			busy(0);
			alert(1, "[3][%s][ OK ]", err);
			return 0;
		}
		busy(0);
		if (!pf_exists(conn_cacert)) {
			alert(1, "[3][A Direct connection needs the|root certificates: put|CACERT.PEM next to EMAIL.PRG.|Get it from curl.se/docs/|caextract or EMail's GitHub page.][ OK ]");
			return 0;
		}
		missing[0] = 0;
		for (i = 0; i < naccts; i++) {
			ACCOUNT *a = accts[i];
			if (!a->dhost[0] && !acct_preset(a)) {
				long l = (long)strlen(missing);
				snprintf(missing + l, sizeof(missing) - l, "%s%s", l ? ", " : "", a->name);
			}
		}
		if (missing[0])
			alert(1, "[1][A Direct connection reaches the|providers itself. Enter the|servers for: %s|in Options > Accounts.][ OK ]", missing);
	}
	opt.falcon = (short)on;
	falcon_dsp_init();
	mail_disconnect_all();
	store_save_settings();
	menu_update();
	return 1;
}

/* ---------------- events ---------------- */

static int shortcut(short kstate, short key)
{
	short scan = KEY_SCAN(key);
	WIN *top = win_topmost();
	if (scan == 0x44) {		/* F10 */
		set_hebrew_kbd(!hebrew_kbd);
		return 1;
	}
	if (crash_test(scan))	/* test builds only: F1-F5 crash on purpose */
		return 1;
	if (scan == 0x62) {		/* Help */
		help_key_pressed();
		return 1;
	}
	if (key_ctrl(kstate, key)) {
		short cmd = C_NONE, shift = (kstate & (K_LSHIFT | K_RSHIFT)) != 0;
		switch (scan) {
		/* the AERA programs' keys (the guidelines, 3.2), with
		   Mail's on the Mac for the rest: Shift+^N get mail, Shift+^R
		   reply to all, Shift+^F forward, Shift+^D send; ^K and ^E
		   as before */
		case 0x31: cmd = shift ? C_CHECK : C_NEW; break;	/* N */
		case 0x25: cmd = C_CHECK; break;	/* K */
		case 0x10: cmd = C_QUIT; break;		/* Q */
		case 0x16: cmd = shift ? C_UNREAD : C_CLOSE; break;	/* U */
		case 0x11: cmd = C_NEXTWIN; break;	/* W */
		case 0x13: cmd = shift ? C_REPLYALL : C_REPLY; break;	/* R */
		case 0x12: cmd = C_REPLYALL; break;	/* E */
		case 0x21: cmd = shift ? C_FORWARD : C_NONE; break;	/* F: Find, one day */
		case 0x22: cmd = C_FLAG; break;		/* G */
		case 0x32: cmd = C_MOVE; break;		/* M */
		case 0x1e: cmd = C_SELALL; break;	/* A */
		case 0x2e: cmd = C_COPY; break;		/* C */
		case 0x2d: cmd = C_CUT; break;		/* X */
		case 0x2f: cmd = C_PASTE; break;	/* V */
		case 0x20: cmd = shift ? C_SEND : C_NONE; break;	/* D */
		case 0x1f: cmd = C_SAVEOUT; break;	/* S: into the Outbox */
		case 0x14: cmd = C_ATTACH; break;	/* T */
		case 0x30: cmd = C_ABOOK; break;	/* B */
		case 0x33: cmd = C_SETTINGS; break;	/* , */
		}
		if (cmd == C_SEND || cmd == C_SAVEOUT || cmd == C_ATTACH) {
			if (w_editor.h <= 0)
				return 1;
		} else if (cmd == C_ABOOK && w_editor.h <= 0 && !naccts) {
			return 1;		/* else a new message, as the menu's */
		} else if ((cmd == C_REPLY || cmd == C_REPLYALL || cmd == C_FORWARD) && !cur_msg) {
			return 1;
		} else if ((cmd == C_UNREAD || cmd == C_FLAG || cmd == C_MOVE) && !cur_msg && list_marked() < 2) {
			return 1;
		} else if (cmd == C_SELALL && (top == &w_editor || !cur_folder)) {
			return 0;		/* the editor may want ^A */
		}
		if (cmd != C_NONE) {
			command(cmd);
			return 1;
		}
	}
	/* Tab moves the keyboard to the next pane */
	if (scan == 0x0f && top && top->pane) {
		win_focus(top == &w_folders ? &w_list : top == &w_list ? &w_reader : &w_folders);
		return 1;
	}
	/* Delete removes the message unless the editor has the keyboard */
	if (scan == 0x53 && top != &w_editor && (cur_msg || list_marked() > 1)) {
		command(C_DELETE);
		return 1;
	}
	return 0;
}

static void place_windows(void)
{
	/* the editor where it was last time, if that still fits */
	w_editor.place.x = opt.ed_x;
	w_editor.place.y = opt.ed_y;
	w_editor.place.w = opt.ed_w;
	w_editor.place.h = opt.ed_h;
	if (opt.ed_w < 30 * cw || opt.ed_h < 8 * ch || opt.ed_x < 0 || opt.ed_y < desk_y ||
	    opt.ed_x + opt.ed_w > desk_x + desk_w + 4 || opt.ed_y + opt.ed_h > desk_y + desk_h + 4) {
		w_editor.place.x = desk_x + desk_w / 10;
		w_editor.place.y = desk_y + ch;
		w_editor.place.w = desk_w - desk_w / 5;
		w_editor.place.h = desk_h - 2 * ch;
	}
}

/* the folder EMAIL.PRG was started from */
static void work_dir(char *out, int size)
{
	char path[160];
	short drv = (short)Dgetdrv();
	path[0] = 0;
	Dgetpath(path, 0);
	snprintf(out, size, "%c:%s", 'A' + drv, path);
	if (out[strlen(out) - 1] == '\\')
		out[strlen(out) - 1] = 0;
}

/* quitting without keeping read messages: which folder's are going */
static void quit_clearing(const char *account, const char *folder)
{
	char m[120];
	snprintf(m, sizeof(m), "%s: removing the downloaded messages of %s...", account, folder);
	status(m);
}

/* EMail crashed and CRASH.LOG is written: say so and leave GEM tidy.
   Only the AES and VDI: the heap may be what went wrong */
static void crash_quit(const char *what)
{
	char a[200];
	graf_mouse(ARROW, 0);
	snprintf(a, sizeof(a), "[3][EMail has crashed|(%s).|What happened is in|CRASH.LOG, next to EMAIL.PRG.][ Quit ]", what);
	form_alert(1, a);
	if (w_editor.h > 0) {
		wind_close(w_editor.h);
		wind_delete(w_editor.h);
	}
	if (main_h > 0) {
		wind_close(main_h);
		wind_delete(main_h);
	}
	theme_exit();
	menu_bar(mb.tree, 0);
	v_clsvwk(vdi_h);
	appl_exit();
}

#ifdef DRAW_TRACE
/* the test build: when the start's steps ended ("START store 420", ms) */
static void start_mark(const char *what)
{
	char t[60];
	snprintf(t, sizeof(t), "START %s %lu", what, pf_ms());
	pf_debug(t);
}
#else
#define start_mark(what) ((void)0)
#endif

int main(void)
{
	short work_in_dummy, work_out[57], msg[8], d;
	char dir[200];
	long free_kb;
	int quiet;		/* nothing shown over the window as EMail started */
	AEVENT ev;

	{
		extern int mem_unaligned_ok;
		mem_unaligned_ok = get_cookie(0x5F435055L) >= 20;	/* _CPU */
	}
	{
		/* under 2 MB free: small receive buffers, so the editor and an
		   open message still fit beside a connection */
		long st = (long)Mxalloc(-1L, 0), tt = (long)Mxalloc(-1L, 1);
		if (st == -32) {		/* TOS 1.x: no Mxalloc, no TT RAM */
			st = (long)Malloc(-1L);
			tt = 0;
		}
		free_kb = ((st > 0 ? st : 0) + (tt > 0 ? tt : 0)) / 1024;
		if (free_kb < 2048)
			conn_bufsize = 4096;
	}
	ap_id = appl_init();
	start_mark("appl_init");
	notify_init("EMAIL   ");
	notify_clicked = notified;
	notify_ask();		/* BGEM.NG's answer comes to the main loop */
	if (ap_id < 0)
		return 1;
	vdi_h = graf_handle(&gl_wchar, &gl_hchar, &d, &d);
	cw = gl_wchar;
	ch = gl_hchar;
	(void)work_in_dummy;
	{
		/* the workstation before any of EMail's memory: NVDI needs some
		   for it (more with many fonts). 0 is no workstation: drawing
		   with it brings NVDI's "Invalid handle" alert, so say why and go */
		short aes_h = vdi_h, tries;
		char note[80];
		for (tries = 0; tries < 2; tries++) {
			vdi_h = v_opnvwk_(aes_h, work_out);
			snprintf(note, sizeof(note), "EMail: v_opnvwk -> %d (%ld KB free before)", vdi_h, free_kb);
			pf_debug(note);
			if (vdi_h > 0)
				break;
			evnt_timer_(200);
		}
		if (vdi_h <= 0) {
			char a[200];
			snprintf(a, sizeof(a), "[3][EMail can't open its screen:|the VDI has no memory for it.|%ld KB were free; the|accessories hold the rest.|Load fewer, or add memory.][ Quit ]",
				 free_kb);
			form_alert(1, a);
			appl_exit();
			return 1;
		}
	}
	scr_w = work_out[0] + 1;
	scr_h = work_out[1] + 1;
	vq_extnd(vdi_h, 1, work_out);
	planes = work_out[4];
	if (!screen_check("EMail", scr_w)) {	/* not in ST low */
		v_clsvwk(vdi_h);
		appl_exit();
		return 1;
	}
	vst_alignment(vdi_h, 0, 5);
	wind_get(0, WF_WORKXYWH, &desk_x, &desk_y, &desk_w, &desk_h);
	graf_mouse(BUSYBEE, 0);

	work_dir(dir, sizeof(dir));
	secret_warn = warn_plain;	/* before anything is saved */
	falcon_usable = get_cookie(0x5F435055L) >= 20;	/* _CPU: TLS.OVL needs a 68020 */
	store_init(dir);
	start_mark("store");
	path_join(tls_ovl_path, sizeof(tls_ovl_path), dir, "TLS.OVL");	/* beside CACERT.PEM */
	crash_install(dir, "EMail", mail_version);
	help_init();			/* memory the help viewer may read */
	av_init("EMAIL");		/* the desktop's AV server, if there is one */
	people_init(64, 0, names_came, picked);	/* names from EContact */
	people_setup();
	crash_cleanup = crash_quit;
	crash_report = conn_log_append;	/* the report in the protocol log too */
	if (store_plain_passwords && !store_lost_passwords)
		store_save_settings();		/* encrypt them right away */
	falcon_dsp_init();
	theme_init();
	font_apply();
	hebrew_kbd = opt.hebrew;
	set_logging();
	net_init();
	pf_idle = busy_idle;
	busy_message = busy_msg;
	dlg_title = "EMail";		/* a dialog's window title, where it has one */
	dlg_redraw_hook = dialog_redraw;
	mail_status = status;
	tls_note = status;
	tls_file_info = pf_info;	/* CACERT.PEM's date and size in one look */
	for (d = 0; d < naccts; d++)
		folders_load(accts[d]);
	start_mark("folders_load");

	folders_init();
	list_init();
	reader_init();
	editor_init();
	place_windows();
	if (mbar_build(&mb, bar, gl_wchar, gl_hchar, scr_w, scr_h)) {
		form_alert(1, "[3][EMail: not enough memory.][ Quit ]");
		v_clsvwk(vdi_h);
		appl_exit();
		return 1;
	}
	menu_bar(mb.tree, 1);
	menu_update();
	graf_mouse(ARROW, 0);

	quiet = !crash_previous() && free_kb >= 300 && naccts && !store_lost_passwords;
	start_mark("menu");
	main_open();
	start_mark("main_open");
	folders_build();
	list_load();
	reader_clear();
	start_mark("panes");
	/* the whole window now, its pane bars and dividers too, not at the
	   AES's first WM_REDRAW: that waits behind the welcome box or the
	   first check, and meanwhile a multitasking AES shows the desktop
	   through it */
	win_redraw_all();
	start_mark("drawn");

	crash_offer("EMail", "next to EMAIL.PRG");
	if (free_kb < 300)		/* say so now, not with a failed send or TLS later */
		alert(1, "[3][Only %ld KB of memory is free|for EMail: the accessories|hold the rest. Mail and secure|connections need more: load|fewer, or add memory.][ OK ]",
		      free_kb);

	if (!naccts) {
		alert(1, "[1][Welcome to EMail!||Let's set up your mail account.][ OK ]");
		cmd_accounts();
	} else {
		/* check mail at start, then open the first inbox; started
		   offline (Settings > Mail), the inbox as it is on disk, and
		   no connection until Options > Work offline goes online */
		FINFO *in;
		if (store_lost_passwords)
			alert(1, "[1][EMAIL.KEY is missing or new: type the passwords again in Options > Accounts.][ OK ]");
		if (!opt.offline)
			cmd_check_all();
		in = folder_role(accts[0], FR_INBOX);
		if (in)
			folders_select(accts[0], in);
	}
	start_mark("inbox");
#ifdef DRAW_TRACE
	/* the test build (xaaes_test.py): EMAIL_NOTIFY= in the AES's
	   environment is sent as new mail's notification would be, once
	   BGEM.NG has answered */
	if (!shel_envrn(&notify_test, "EMAIL_NOTIFY=") || (notify_test && !*notify_test))
		notify_test = 0;
#endif
	last_check = pf_ms();
	/* all of the window is drawn and nothing was shown over it: the
	   AES's redraw for its opening is the start's fourth of every pane */
	win_skip_open_redraw = quiet && !check_alerted;
	evnt_set_m1(1, 0, 0, 1, 1);
	{
		/* files or a mailto: given when EMail was started (dropped on
		   its icon, a desktop's "open with"), the TOS way or the ARGV
		   way of MiNT and MagiC: as a VA_START, in the main loop */
		const char *c = av_command();
		if (*c)
			kept_start = strdup(c);
	}

	while (!quit) {
		/* 0x101/3/0: wake on any button press, left or right */
		short which, hovering = !tip_tried && hover_x >= 0;
		conn_stop = 0;		/* a stop is for one operation */
		/* a key typed while busy comes first, as if typed now */
		if (names_wait) {		/* names that came during a dialog */
			names_wait = 0;
			names_came();
		}
		if (kept_start) {		/* what came during a dialog */
			char *v = kept_start;
			kept_start = 0;
			av_start(v);
			free(v);
		}
		if (nkept) {
			memcpy(msg, kept[0], sizeof(kept[0]));
			memmove(kept[0], kept[1], --nkept * sizeof(kept[0]));
			which = MU_MESAG;
		} else {
			which = busy_key(&ev) ? MU_KEYBD : evnt_multi_(MU_MESAG | MU_KEYBD | MU_BUTTON | MU_TIMER | MU_M1,
									 0x101, 3, 0,
									 list_show_waiting() ? 0 : hovering ? 100 : 1000,
									 msg, &ev);
		}
		/* a message a key moved to is shown once no key is waiting, and
		   before anything but another of those keys (Up, Down, Home) is
		   done: a command acts on the message shown */
		if (list_show_waiting() && !(which & (MU_KEYBD | MU_MESAG | MU_BUTTON)))
			list_show_later(0);
		else if (list_show_waiting() &&
			 (which & (MU_MESAG | MU_BUTTON) || (KEY_SCAN(ev.kreturn) != 0x48 &&
							   KEY_SCAN(ev.kreturn) != 0x50 && KEY_SCAN(ev.kreturn) != 0x47)))
			list_show_later(1);
		if (which & MU_M1) {
			win_hover(ev.mx, ev.my);
			hover_x = ev.mx;
			hover_y = ev.my;
		}
		if (which & (MU_M1 | MU_KEYBD | MU_BUTTON | MU_MESAG)) {
			hover_at = pf_ms();
			tip_tried = (which & (MU_KEYBD | MU_BUTTON)) != 0;
			tip_hide();
		} else if (hovering && pf_ms() - hover_at >= 700UL) {
			tip_show();	/* after the mouse rested 0.7 s */
		}
		if ((which & MU_MESAG) && notify_message(msg))
			which &= ~MU_MESAG;	/* BGEM.NG's: its answer, a button clicked */
		if (which & MU_MESAG) {
			const char *va;
			switch (msg[0]) {
			case MN_SELECTED:
				command(mbar_command(&mb, msg[4]));
				menu_tnormal(mb.tree, msg[3], 1);
				break;
			case AP_TERM:
				quit = 1;
				break;
			default: {
				DROP d;
				switch (dd_message(msg, DD_WANT_FILES | DD_WANT_TEXT, &d)) {
				case DD_FILES: av_start(d.data); break;	/* as VA_START's */
				case DD_TEXT: drop_text(d.data); break;
				case DD_NONE:
					switch (av_message(msg, &va)) {
					case AV_START: av_start(va); break;
					case AV_PEOPLE: people_message(va, msg[1]); break;
					case AV_DONE: break;
					default: win_message(msg);
					}
				}
				dd_done(&d);
			}
			}
		}
		if ((which & MU_KEYBD) && win_icon_on_top())
			which &= ~MU_KEYBD;	/* an icon takes no keys */
		if (which & MU_KEYBD) {
			WIN *top = win_topmost();
			if (!shortcut(ev.kstate, ev.kreturn) && top && top->key)
				top->key(top, ev.kstate, ev.kreturn);
		}
		if (which & MU_BUTTON) {
			short h = wind_find(ev.mx, ev.my), t, d;
			wind_get(0, WF_TOP, &t, &d, &d, &d);
			if (h > 0 && h != t)	/* a click in a window behind: bring it up too */
				wind_set(h, WF_TOP, 0, 0, 0, 0);
			if (win_mouse(ev.mx, ev.my, ev.mbutton, double_click(ev.mx, ev.my), ev.kstate))
				menu_update();
			wait_release();
		}
		if (which & (MU_KEYBD | MU_BUTTON | MU_MESAG))
			last_input = pf_ms();
		/* read and flag changes go to the server once the user pauses */
		if ((which & MU_TIMER) && pf_ms() - last_input > 3000 && mail_flags_pending(cur_folder)) {
			mail_push_flags(cur_folder);
			last_input = pf_ms();	/* failed: not again at once */
		}
		if ((which & MU_TIMER) && opt.check > 0 && !opt.offline && naccts &&
		    pf_ms() - last_check > (unsigned long)opt.check * 60000UL) {
			quiet_check = 1;
			cmd_check_all();
			quiet_check = 0;
		}
#ifdef DRAW_TRACE
		if (notify_test && notify_ok()) {
			news_acct = naccts ? accts[0] : 0;
			pf_debug(notify_send("EMail", notify_test, "Open|Later", 1) ? "NOTIFY sent" : "NOTIFY not sent");
			notify_test = 0;
		}
#endif
		people_poll();		/* the names asked for while drawing go out */
		win_skip_open_redraw = 0;	/* the first event only */
	}

	if (mail_flags_pending(cur_folder)) {
		status("Telling the server what you read...");
		mail_push_flags(cur_folder);
	}
	/* what is left to do, each step on the status line */
	busy(1);
	imap_logout_nowait = 1;		/* the server needn't answer */
	for (d = 0; d < naccts; d++)
		if (accts[d]->im) {
			char m[80];
			snprintf(m, sizeof(m), "%s: logging out...", accts[d]->name);
			status(m);
			mail_disconnect(accts[d]);
		}
	{
		/* where this Atari kept the user waiting longest (common's
		   aes_slowest; the guidelines' Speed): one line in the log */
		char t[200];
		const char *sl = aes_slowest();
		if (*sl) {
			snprintf(t, sizeof(t), "the longest waits this session: %s", sl);
			conn_log("EMail", " -- ", t, (long)strlen(t));
		}
	}
	conn_log_flush();
	if (cur_folder)
		fold_close(cur_folder);
	if (!opt.keepcache)
		cache_clear_all(quit_clearing, 0);
	status("Saving the folders and settings...");
	for (d = 0; d < naccts; d++)
		folders_save(accts[d]);
	store_save_settings();
	win_close(&w_editor);
	main_close();
	store_save_settings();
	theme_exit();			/* the palette back as it was */
	menu_bar(mb.tree, 0);
	av_exit();
	v_clsvwk(vdi_h);
	appl_exit();
	crash_remove();
	return 0;
}
