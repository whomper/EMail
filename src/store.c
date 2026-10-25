/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * store.c - settings, accounts, folder lists, header indexes and the
 * message cache (see store.h for the layout on disk).
 */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include "plat.h"
#include "store.h"
#include "secret.h"
#include "tz.h"
#include "charset.h"
#include "mime.h"
#include "util.h"
#include "conn.h"
#include "tls.h"

OPTIONS opt;
ACCOUNT *accts[MAXACCT];
short naccts;

static char inf_path[220], mail_dir[220];

/* ---------------- settings ---------------- */

static int tz_given;		/* EMAIL.INF had the old TZ= minutes */

static void defaults(void)
{
	opt.tz = 0;
	opt.tzname[0] = 0;
	tz_given = 0;
	opt.check = 0;
	opt.page = 100;
	opt.keepcache = 1;
	opt.log = 0;
	opt.hebrew = 0;
	opt.offline = 0;
	opt.dsp = 1;
	opt.colour = 1;
	opt.dialogs3d = 1;
	opt.peoplefile = 0;
	opt.peopledir[0] = 0;
	opt.browser[0] = 0;
	opt.wrap = 72;
	opt.font_id = 1;
	opt.font_pt = 0;	/* the system font at the screen's size */
}

/* "a,b,c,d" -> up to n shorts */
static void shorts(const char *v, short *out, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		char *e;
		out[i] = (short)strtol(v, &e, 10);
		if (*e != ',')
			break;
		v = e + 1;
	}
}

/* \n and \\ escapes keep multi-line values on one line */
static void unescape(char *d, const char *s, int size)
{
	int o = 0;
	while (*s && o < size - 1) {
		if (*s == '\\' && s[1] == 'n') {
			d[o++] = '\n';
			s += 2;
		} else if (*s == '\\' && s[1] == '\\') {
			d[o++] = '\\';
			s += 2;
		} else {
			d[o++] = *s++;
		}
	}
	d[o] = 0;
}

static void escape(SBUF *b, const char *s)
{
	for (; *s; s++) {
		if (*s == '\n')
			sb_adds(b, "\\n");
		else if (*s == '\\')
			sb_adds(b, "\\\\");
		else if (*s != '\r')
			sb_addc(b, *s);
	}
}

ACCOUNT *acct_new(void)
{
	ACCOUNT *a;
	short id, i;
	if (naccts >= MAXACCT)
		return 0;
	a = calloc(1, sizeof(ACCOUNT));
	if (!a)
		return 0;
	/* first free id */
	for (id = 1; id <= MAXACCT; id++) {
		for (i = 0; i < naccts; i++)
			if (accts[i]->id == id)
				break;
		if (i == naccts)
			break;
	}
	a->id = id;
	a->port = 143;
	a->smtpport = 587;
	a->leave = 1;
	strcpy(a->name, "Mail");
	accts[naccts++] = a;
	acct_dirs(a);
	return a;
}

const char *acct_host(ACCOUNT *a, int smtp)
{
	if (opt.falcon) {
		if (smtp && a->dsmtphost[0])
			return a->dsmtphost;
		return a->dhost;
	}
	if (smtp && a->smtphost[0])
		return a->smtphost;
	return a->host;
}

unsigned short acct_port(ACCOUNT *a, int smtp)
{
	if (opt.falcon)
		return smtp ? (a->dsmtpport ? a->dsmtpport : 465) : (a->dport ? a->dport : (a->pop ? 995 : 993));
	return smtp ? a->smtpport : a->port;
}

int acct_sec(ACCOUNT *a, int smtp)
{
	unsigned short p;
	short s;
	if (!opt.falcon)
		return SEC_PLAIN;
	s = smtp ? a->dsmtpsec : a->dsec;
	if (s)
		return s;
	p = acct_port(a, smtp);
	return p == 993 || p == 995 || p == 465 ? SEC_TLS : SEC_STARTTLS;
}

const char *acct_user(ACCOUNT *a, int smtp)
{
	const char *u = opt.falcon ? a->duser : a->user;
	if (smtp) {
		const char *su = opt.falcon ? a->dsmtpuser : a->smtpuser;
		if (!strcmp(su, "-"))
			return 0;
		if (su[0])
			return su;
	}
	return u;
}

const char *acct_pass(ACCOUNT *a, int smtp)
{
	if (smtp && (opt.falcon ? a->dsmtpuser : a->smtpuser)[0])
		return opt.falcon ? a->dsmtppass : a->smtppass;
	return opt.falcon ? a->dpass : a->pass;
}

void acct_fill_logins(ACCOUNT *a)
{
	if (!a->duser[0] && !a->dpass[0] && !a->dsmtpuser[0]) {
		str_copy(a->duser, a->user, sizeof(a->duser));
		str_copy(a->dpass, a->pass, sizeof(a->dpass));
		str_copy(a->dsmtpuser, a->smtpuser, sizeof(a->dsmtpuser));
		str_copy(a->dsmtppass, a->smtppass, sizeof(a->dsmtppass));
	} else if (!a->user[0] && !a->pass[0] && !a->smtpuser[0]) {
		str_copy(a->user, a->duser, sizeof(a->user));
		str_copy(a->pass, a->dpass, sizeof(a->pass));
		str_copy(a->smtpuser, a->dsmtpuser, sizeof(a->smtpuser));
		str_copy(a->smtppass, a->dsmtppass, sizeof(a->smtppass));
	}
}

/* the big providers' servers, for a Direct connection */
static const struct {
	const char *domains, *in, *out;
	unsigned short inport, outport;
} presets[] = {
	{ " gmail.com googlemail.com ", "imap.gmail.com", "smtp.gmail.com", 993, 465 },
	{ " icloud.com me.com mac.com ", "imap.mail.me.com", "smtp.mail.me.com", 993, 587 },
	{ " yahoo.com ymail.com ", "imap.mail.yahoo.com", "smtp.mail.yahoo.com", 993, 465 },
	{ " gmx.net gmx.de gmx.com ", "imap.gmx.net", "mail.gmx.net", 993, 465 },
	{ " web.de ", "imap.web.de", "smtp.web.de", 993, 587 },
	{ " aol.com ", "imap.aol.com", "smtp.aol.com", 993, 465 },
	{ " fastmail.com fastmail.fm ", "imap.fastmail.com", "smtp.fastmail.com", 993, 465 },
	{ " zoho.com ", "imap.zoho.com", "smtp.zoho.com", 993, 465 },
};

int acct_preset(ACCOUNT *a)
{
	const char *at = strrchr(a->email, '@');
	char key[80];
	unsigned i;
	if (!at)
		return 0;
	snprintf(key, sizeof(key), " %s ", at + 1);
	for (i = 0; key[i]; i++)
		key[i] = (char)tolower((unsigned char)key[i]);
	for (i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
		if (!strstr(presets[i].domains, key))
			continue;
		str_copy(a->dhost, presets[i].in, sizeof(a->dhost));
		a->dport = presets[i].inport;
		str_copy(a->dsmtphost, presets[i].out, sizeof(a->dsmtphost));
		a->dsmtpport = presets[i].outport;
		a->dsec = a->dsmtpsec = 0;
		return 1;
	}
	return 0;
}

void acct_dirs(ACCOUNT *a)
{
	char name[16];
	snprintf(name, sizeof(name), "ACCT%d", a->id);
	path_join(a->dir, sizeof(a->dir), mail_dir, name);
	pf_mkdir(a->dir);
}

void acct_delete(ACCOUNT *a)
{
	short i, j;
	for (i = 0; i < naccts; i++) {
		if (accts[i] != a)
			continue;
		for (j = i; j < naccts - 1; j++)
			accts[j] = accts[j + 1];
		naccts--;
		break;
	}
	if (a->im)
		imap_logout(a->im);
	free(a);
}

int store_lost_passwords, store_plain_passwords;

/* a password from EMAIL.INF: encrypted ({E}...) or, from an older EMail,
   as typed. One this EMAIL.KEY can't open is left empty */
static void unseal(char *field, const char *v, int size)
{
	if (!secret_decode(v, field, size)) {
		field[0] = 0;
		store_lost_passwords = 1;
	} else if (*v && strncmp(v, SECRET_TAG, strlen(SECRET_TAG))) {
		store_plain_passwords = 1;	/* written by an older EMail */
	}
}

static void set_acct(ACCOUNT *a, const char *k, const char *v)
{
	char tmp[256];
	unescape(tmp, v, sizeof(tmp));
	if (!strcmp(k, "name")) str_copy(a->name, tmp, sizeof(a->name));
	else if (!strcmp(k, "fullname")) str_copy(a->fullname, tmp, sizeof(a->fullname));
	else if (!strcmp(k, "email")) str_copy(a->email, tmp, sizeof(a->email));
	else if (!strcmp(k, "in")) a->pop = !strcasecmp(tmp, "pop3");
	else if (!strcmp(k, "host")) str_copy(a->host, tmp, sizeof(a->host));
	else if (!strcmp(k, "port")) a->port = (unsigned short)atoi(tmp);
	else if (!strcmp(k, "user")) str_copy(a->user, tmp, sizeof(a->user));
	else if (!strcmp(k, "pass")) unseal(a->pass, tmp, sizeof(a->pass));
	else if (!strcmp(k, "leave")) a->leave = (short)atoi(tmp);
	else if (!strcmp(k, "smtphost")) str_copy(a->smtphost, tmp, sizeof(a->smtphost));
	else if (!strcmp(k, "smtpport")) a->smtpport = (unsigned short)atoi(tmp);
	else if (!strcmp(k, "smtpuser")) str_copy(a->smtpuser, tmp, sizeof(a->smtpuser));
	else if (!strcmp(k, "smtppass")) unseal(a->smtppass, tmp, sizeof(a->smtppass));
	else if (!strcmp(k, "sent")) str_copy(a->sentname, tmp, sizeof(a->sentname));
	else if (!strcmp(k, "dhost")) str_copy(a->dhost, tmp, sizeof(a->dhost));
	else if (!strcmp(k, "dport")) a->dport = (unsigned short)atoi(tmp);
	else if (!strcmp(k, "dsmtphost")) str_copy(a->dsmtphost, tmp, sizeof(a->dsmtphost));
	else if (!strcmp(k, "dsmtpport")) a->dsmtpport = (unsigned short)atoi(tmp);
	else if (!strcmp(k, "dsec")) a->dsec = (short)atoi(tmp);
	else if (!strcmp(k, "dsmtpsec")) a->dsmtpsec = (short)atoi(tmp);
	else if (!strcmp(k, "duser")) str_copy(a->duser, tmp, sizeof(a->duser));
	else if (!strcmp(k, "dpass")) unseal(a->dpass, tmp, sizeof(a->dpass));
	else if (!strcmp(k, "dsmtpuser")) str_copy(a->dsmtpuser, tmp, sizeof(a->dsmtpuser));
	else if (!strcmp(k, "dsmtppass")) unseal(a->dsmtppass, tmp, sizeof(a->dsmtppass));
}

/* ---------------- the time zone ---------------- */


static const TZONE *zone(void)
{
	static char name[40];
	static const TZONE *z;
	if (!z || strcmp(name, opt.tzname)) {
		z = tz_find(opt.tzname);
		str_copy(name, opt.tzname, sizeof(name));
		if (!z)
			z = tz_utc();
	}
	return z;
}

int zone_offset_at(unsigned long utc)
{
	return tz_offset(zone(), (CTIME)(utc / 60));
}

long zone_utc_now(void)
{
	PFTIME t;
	pf_now(&t);
	return tz_to_utc(zone(), ct_make(t.year, t.mon, t.day, t.hour, t.min));
}

int zone_offset_now(void)
{
	return tz_offset(zone(), zone_utc_now());
}

/* the zone whose clock shows `minutes` east of UTC now, the usual one
   of the places sharing it first */
static const char *zone_for_offset(int minutes)
{
	static const char *const usual[] = {
		"Asia/Jerusalem", "Europe/London", "Europe/Berlin", "America/New_York",
		"America/Chicago", "America/Denver", "America/Los_Angeles", "Europe/Moscow",
		"Asia/Tokyo", "Australia/Sydney", 0
	};
	PFTIME t;
	int i;
	pf_now(&t);
	for (i = 0; usual[i]; i++) {
		const TZONE *z = tz_find(usual[i]);
		if (z && tz_offset(z, tz_to_utc(z, ct_make(t.year, t.mon, t.day, t.hour, t.min))) == minutes)
			return usual[i];
	}
	for (i = 0; tz_index(i); i++) {
		const TZONE *z = tz_index(i);
		if (tz_offset(z, tz_to_utc(z, ct_make(t.year, t.mon, t.day, t.hour, t.min))) == minutes)
			return z->name;
	}
	return 0;
}

/* a POSIX TZ ("IST-2IDT,M3.4.4/26,M10.5.0"): the zone its abbreviation
   names, else one with its offset */
static const char *zone_from_posix(const char *s)
{
	static const char *const abbr[][2] = {
		{ "IST", "Asia/Jerusalem" }, { "IDT", "Asia/Jerusalem" }, { "GMT", "Europe/London" },
		{ "BST", "Europe/London" }, { "CET", "Europe/Berlin" }, { "MET", "Europe/Berlin" },
		{ "EET", "Europe/Athens" }, { "MSK", "Europe/Moscow" }, { "EST", "America/New_York" },
		{ "CST", "America/Chicago" }, { "MST", "America/Denver" }, { "PST", "America/Los_Angeles" },
		{ "JST", "Asia/Tokyo" }, { "AEST", "Australia/Sydney" }, { "UTC", "UTC" }, { 0, 0 }
	};
	char name[8];
	int n = 0, i, sign = 1, h = 0, m = 0;
	while (n < 7 && ((s[n] >= 'A' && s[n] <= 'Z') || (s[n] >= 'a' && s[n] <= 'z'))) {
		name[n] = s[n];
		n++;
	}
	name[n] = 0;
	for (i = 0; abbr[i][0]; i++)
		if (!strcasecmp(name, abbr[i][0]))
			return abbr[i][1];
	s += n;
	if (*s == '-' || *s == '+')
		sign = *s++ == '-' ? 1 : -1;	/* POSIX counts west: "-2" is east */
	while (*s >= '0' && *s <= '9')
		h = h * 10 + *s++ - '0';
	if (*s == ':')
		for (s++; *s >= '0' && *s <= '9'; s++)
			m = m * 10 + *s - '0';
	return n ? zone_for_offset(sign * (h * 60 + m)) : 0;
}

/* the machine's time zone, if it says: TZ in the environment (MiNT,
   MagiC), MiNT's /etc/timezone, or the minutes of an older EMAIL.INF */
static void zone_detect(void)
{
	const char *tz = pf_getenv("TZ"), *found = 0;
	char *file;
	if (tz && *tz)
		found = tz_find(tz) ? tz : zone_from_posix(tz);
	if (!found && (file = pf_load("U:\\etc\\timezone", 0)) != 0) {
		char *e = file + strcspn(file, "\r\n \t");
		*e = 0;
		if (tz_find(file))
			str_copy(opt.tzname, file, sizeof(opt.tzname));
		free(file);
		if (opt.tzname[0])
			return;
	}
	if (!found && tz_given)
		found = zone_for_offset(opt.tz);
	str_copy(opt.tzname, found ? (tz_find(found) ? tz_find(found)->name : found) : "UTC",
		 sizeof(opt.tzname));
}

/* EMAIL.INF's FALCON= (a Direct connection) as it was read: where there
   is no TLS (a build without it, tls_available 0, or an Atari with a
   68000, falcon_usable 0) EMail runs through the gateway and writes it
   back as it found it, for the Falcon whose EMAIL.INF it was */
static short falcon_inf;
int falcon_usable = 1;

static void set_opt(const char *k, const char *v)
{
	short n = (short)atoi(v);
	if (!strcmp(k, "tz")) { opt.tz = n; tz_given = 1; }	/* minutes: before time zones */
	else if (!strcmp(k, "timezone")) str_copy(opt.tzname, v, sizeof(opt.tzname));
	else if (!strcmp(k, "check")) opt.check = n;
	else if (!strcmp(k, "signature")) unescape(opt.signature, v, sizeof(opt.signature));
	else if (!strcmp(k, "page")) opt.page = n < 20 ? 20 : n > 1000 ? 1000 : n;
	else if (!strcmp(k, "keepcache")) opt.keepcache = n;
	else if (!strcmp(k, "log")) opt.log = n;
	else if (!strcmp(k, "hebrew")) opt.hebrew = n;
	else if (!strcmp(k, "offline")) opt.offline = n;
	else if (!strcmp(k, "wrap")) opt.wrap = n < 40 ? 40 : n > 78 ? 78 : n;
	else if (!strcmp(k, "main")) shorts(v, &opt.main_x, 4);
	else if (!strcmp(k, "panes")) shorts(v, &opt.pane_w, 2);
	else if (!strcmp(k, "editor")) shorts(v, &opt.ed_x, 4);
	else if (!strcmp(k, "font")) shorts(v, &opt.font_id, 2);
	else if (!strcmp(k, "falcon")) {
		falcon_inf = n != 0;
		opt.falcon = tls_available && falcon_usable && falcon_inf;
	}
	else if (!strcmp(k, "dsp")) opt.dsp = n != 0;
	else if (!strcmp(k, "peoplefile")) opt.peoplefile = n != 0;
	else if (!strcmp(k, "peopledir")) str_copy(opt.peopledir, v, sizeof(opt.peopledir));
	else if (!strcmp(k, "browser")) str_copy(opt.browser, v, sizeof(opt.browser));
	else if (!strcmp(k, "colour") || !strcmp(k, "color")) opt.colour = n != 0;
	else if (!strcmp(k, "dialogs3d")) opt.dialogs3d = n != 0;
	else if (!strcmp(k, "hebfont")) opt.hebfont = n >= 0 && n <= 2 ? n : 0;
}

/* EMAIL.INF being read: what store_begin loaded, the line store_step is
   at, the account it is in */
static char *rd_buf, *rd_line;
static ACCOUNT *rd_cur;
static short rd_opts;
short store_inf_accounts;

int store_begin(const char *workdir)
{
	defaults();
	pf_size_open = pf_handle_size;	/* one look per file read */
	str_copy(opt.workdir, workdir, sizeof(opt.workdir));
	path_join(inf_path, sizeof(inf_path), workdir, "EMAIL.INF");
	{
		char kf[220];
		path_join(kf, sizeof(kf), workdir, "EMAIL.KEY");
		secret_init(kf);
		store_lost_passwords = store_plain_passwords = 0;
	}
	path_join(mail_dir, sizeof(mail_dir), workdir, "EMAIL");
	pf_mkdir(mail_dir);
	/* a Direct connection: the root certificates next to EMAIL.PRG, the random
	   seed with the mail */
	path_join(conn_cacert, sizeof(conn_cacert), workdir, "CACERT.PEM");
	path_join(tls_seed_path, sizeof(tls_seed_path), mail_dir, "SEED.DAT");
	path_join(tls_roots_cache, sizeof(tls_roots_cache), workdir, "ROOTS.DAT");

	free(rd_buf);
	rd_buf = rd_line = pf_load(inf_path, 0);
	rd_cur = 0;
	rd_opts = 0;
	store_inf_accounts = 0;
	return rd_buf != 0;
}

/* one line of EMAIL.INF */
static void inf_line(char *line)
{
	char *eq, *k;
	k = str_trim(line);
	if (*k == ';' || *k == '#' || !*k)
		return;
	if (*k == '[') {
		rd_opts = !strncasecmp(k, "[options]", 9);
		rd_cur = 0;
		if (!strncasecmp(k, "[account]", 9)) {
			store_inf_accounts++;
			rd_cur = acct_new();
		}
		return;
	}
	eq = strchr(k, '=');
	if (!eq)
		return;
	*eq = 0;
	{
		char *key = str_trim(k), *val = eq + 1;
		char *sc;
		long i;
		for (i = 0; key[i]; i++)	/* names in any case: PASS, pass */
			key[i] = (char)tolower((unsigned char)key[i]);
		/* "value ; comment" - but passwords may contain ';' */
		if (strcmp(key, "pass") && strcmp(key, "smtppass") && strcmp(key, "dpass") &&
		    strcmp(key, "dsmtppass") && strcmp(key, "signature") &&
		    (sc = strstr(val, " ;")))
			*sc = 0;
		val = str_trim(val);
		if (rd_cur)
			set_acct(rd_cur, key, val);
		else if (rd_opts)
			set_opt(key, val);
	}
}

#define STEP_LINES 8

int store_step(void)
{
	short n, i;
	if (!rd_buf)
		return 0;
	for (n = 0; n < STEP_LINES && rd_line && *rd_line; n++) {
		char *line = rd_line;
		rd_line = strchr(line, '\n');
		if (rd_line)
			*rd_line++ = 0;
		inf_line(line);
	}
	if (rd_line && *rd_line)
		return 1;
	free(rd_buf);
	rd_buf = rd_line = 0;
	if (!opt.tzname[0] || !tz_find(opt.tzname))
		zone_detect();		/* written with the next save */
	tls_tz_minutes = zone_offset_now();
	for (i = 0; i < naccts; i++)
		acct_fill_logins(accts[i]);
	return 0;
}

int store_init(const char *workdir)
{
	if (!store_begin(workdir))
		return 0;
	while (store_step())
		;
	return naccts;
}

/* EMAIL.INF explains itself: every setting is a ';' line saying what it
 * is, the setting in capitals, and an empty line. EMail skips the notes
 * when reading (and reads names in any case) and writes them anew on
 * every save. */
static void put_str(SBUF *b, const char *note, const char *key, const char *val)
{
	sb_adds(b, "; ");
	sb_adds(b, note);
	sb_adds(b, "\r\n");
	sb_adds(b, key);
	sb_adds(b, "=");
	escape(b, val);
	sb_adds(b, "\r\n\r\n");
}

/* a path, as it is: no escapes, so it reads as typed (C:\ECONTACT) */
static void put_path(SBUF *b, const char *note, const char *key, const char *val)
{
	sb_adds(b, "; ");
	sb_adds(b, note);
	sb_adds(b, "\r\n");
	sb_adds(b, key);
	sb_adds(b, "=");
	sb_adds(b, val);
	sb_adds(b, "\r\n\r\n");
}

static void put_fmt(SBUF *b, const char *note, const char *key, const char *fmt, ...)
{
	char val[80];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(val, sizeof(val), fmt, ap);
	va_end(ap);
	sb_adds(b, "; ");
	sb_adds(b, note);
	sb_adds(b, "\r\n");
	sb_adds(b, key);
	sb_adds(b, "=");
	sb_adds(b, val);
	sb_adds(b, "\r\n\r\n");
}

/* a password, encrypted with EMAIL.KEY */
static void put_secret(SBUF *b, const char *note, const char *key, const char *val)
{
	char sealed[300];
	secret_encode(val, sealed, sizeof(sealed));
	put_str(b, note, key, sealed);
}

int store_save_settings(void)
{
	SBUF b;
	short i;
	int r;
	tls_tz_minutes = zone_offset_now();
	sb_init(&b);
	sb_adds(&b, "; EMail settings, written by EMAIL.PRG. Lines starting with ; are notes.\r\n"
		    "; Most of these are set in Options > Settings; the Help key opens the guide.\r\n"
		    "; 1 means on, 0 means off.\r\n\r\n[OPTIONS]\r\n\r\n");
	put_str(&b, "your time zone, by its name (Asia/Jerusalem); summer time follows by itself",
		"TIMEZONE", opt.tzname);
	put_fmt(&b, "check for new mail every this many minutes; 0 = only when you ask",
		"CHECK", "%d", opt.check);
	put_fmt(&b, "messages loaded at a time in a folder, \"Load more\" gets the next ones (20-1000)",
		"PAGE", "%d", opt.page);
	put_fmt(&b, "1 = keep messages you have read on disk after quitting, 0 = only their headers",
		"KEEPCACHE", "%d", opt.keepcache);
	put_fmt(&b, "1 = write the conversation with the servers to EMAIL.LOG (passwords hidden)",
		"LOG", "%d", opt.log);
	put_fmt(&b, "1 = start with the Hebrew keyboard (F10 switches)", "HEBREW", "%d", opt.hebrew);
	put_fmt(&b, "1 = work offline: don't connect, keep new messages in the Outbox",
		"OFFLINE", "%d", opt.offline);
	put_fmt(&b, "the editor wraps lines at this column (40-78)", "WRAP", "%d", opt.wrap);
	put_str(&b, "added below new messages from every account; \\n starts a new line (up to 4)",
		"SIGNATURE", opt.signature);
	put_fmt(&b, "main window x,y,width,height in pixels; 0,0,0,0 = let EMail place it",
		"MAIN", "%d,%d,%d,%d", opt.main_x, opt.main_y, opt.main_w, opt.main_h);
	put_fmt(&b, "dividers: folder pane width, message list height, in pixels",
		"PANES", "%d,%d", opt.pane_w, opt.pane_h);
	put_fmt(&b, "editor window x,y,width,height in pixels; 0,0,0,0 = let EMail place it",
		"EDITOR", "%d,%d,%d,%d", opt.ed_x, opt.ed_y, opt.ed_w, opt.ed_h);
	put_fmt(&b, "text font: GDOS font id (1 = system font), size (system font: 0, 8 small, 16 large)",
		"FONT", "%d,%d", opt.font_id, opt.font_pt);
	put_fmt(&b, "the font's Hebrew letters: 0 = Atari places, 1 = ISO-8859-8, 2 = DOS 862 (Options > Font)",
		"HEBFONT", "%d", opt.hebfont);
	put_fmt(&b, "1 = Direct connection: TLS on the Atari (TLS.OVL, a 68020 or better), straight to the providers; 0 = through the Pi gateway",
		"FALCON", "%d", tls_available && falcon_usable ? opt.falcon : falcon_inf);
	put_fmt(&b, "Direct connection: 1 = the DSP checks the servers' signatures, 0 = the 68030 does",
		"DSP", "%d", opt.dsp);
	put_fmt(&b, "1 = colours (selection, unread counts, flags, quotes, links) with 16 or more; with 4 links in blue",
		"COLOUR", "%d", opt.colour);
	put_fmt(&b, "1 = dialogs in the 3D look where the AES draws them (TOS 4, MagiC, XaAES, EmuTOS in colour); 0 = flat",
		"DIALOGS3D", "%d", opt.dialogs3d);
	put_fmt(&b, "1 = when EContact isn't loaded, names come from its contacts file (CONTACTS.TXT)",
		"PEOPLEFILE", "%d", opt.peoplefile);
	put_path(&b, "EContact's folder, where its contacts file is (C:\\ECONTACT)", "PEOPLEDIR", opt.peopledir);
	put_path(&b, "EWeb, started on a link when no web browser runs (C:\\EWEB\\EWEB.PRG; empty: asked once)",
		 "BROWSER", opt.browser);
	for (i = 0; i < naccts; i++) {
		ACCOUNT *a = accts[i];
		sb_adds(&b, "; one [ACCOUNT] part per account, up to 8; Options > Accounts edits them\r\n"
			    "[ACCOUNT]\r\n\r\n");
		put_str(&b, "the account's name in the folder list", "NAME", a->name);
		put_str(&b, "your name, as people you write to see it", "FULLNAME", a->fullname);
		put_str(&b, "your e-mail address", "EMAIL", a->email);
		put_str(&b, "incoming mail: imap (folders stay on the server) or pop3 (mail comes to the Atari)",
			"IN", a->pop ? "pop3" : "imap");
		put_str(&b, "incoming server through the Pi gateway: the Pi's IP address", "HOST", a->host);
		put_fmt(&b, "its port: 143 (IMAP) or 110 (POP3) on the gateway", "PORT", "%u", a->port);
		put_str(&b, "login for the incoming server (Pi gateway)", "USER", a->user);
		put_secret(&b, "password, encrypted with EMAIL.KEY (type it in Options > Accounts)", "PASS", a->pass);
		put_fmt(&b, "POP3 only: 1 = leave mail on the server after downloading it",
			"LEAVE", "%d", a->leave);
		put_str(&b, "outgoing (SMTP) server through the Pi gateway: the Pi's IP address",
			"SMTPHOST", a->smtphost);
		put_fmt(&b, "its port: 587 on the gateway", "SMTPPORT", "%u", a->smtpport);
		put_str(&b, "SMTP login: empty = same as incoming, - = the server needs no login",
			"SMTPUSER", a->smtpuser);
		put_secret(&b, "SMTP password, if the SMTP login differs (encrypted)", "SMTPPASS", a->smtppass);
		put_str(&b, "IMAP folder for copies of sent mail; empty = the server's Sent folder",
			"SENT", a->sentname);
		put_str(&b, "Direct connection: the provider's incoming server", "DHOST", a->dhost);
		put_fmt(&b, "its port: 993 (IMAP) and 995 (POP3) are TLS, others STARTTLS", "DPORT", "%u", a->dport);
		put_fmt(&b, "0 = TLS or STARTTLS by the port, 1 = always TLS, 2 = always STARTTLS",
			"DSEC", "%d", a->dsec);
		put_str(&b, "Direct connection: the provider's outgoing (SMTP) server", "DSMTPHOST", a->dsmtphost);
		put_fmt(&b, "its port: 465 is TLS, 587 STARTTLS", "DSMTPPORT", "%u", a->dsmtpport);
		put_fmt(&b, "0 = TLS or STARTTLS by the port, 1 = always TLS, 2 = always STARTTLS",
			"DSMTPSEC", "%d", a->dsmtpsec);
		put_str(&b, "Direct connection: login for the provider's incoming server", "DUSER", a->duser);
		put_secret(&b, "Direct connection: its password, encrypted (iCloud, Gmail: an app password)", "DPASS", a->dpass);
		put_str(&b, "Direct connection: SMTP login; empty = same as incoming, - = none", "DSMTPUSER", a->dsmtpuser);
		put_secret(&b, "Direct connection: SMTP password, if the SMTP login differs (encrypted)", "DSMTPPASS", a->dsmtppass);
	}
	r = pf_save(inf_path, b.s, b.len);
	sb_free(&b);
	return r;
}

/* ---------------- folder lists ---------------- */

static void folder_names(FINFO *f, char delim)
{
	const char *last = f->server, *p;
	char *d;
	f->depth = 0;
	if (delim) {
		for (p = f->server; *p; p++)
			if (*p == delim && p[1]) {
				f->depth++;
				last = p + 1;
			}
	}
	if (f->role == FR_INBOX && !f->local) {
		strcpy(f->disp, "Inbox");
		return;
	}
	d = mutf7_to_atari(last);
	str_copy(f->disp, d ? d : last, sizeof(f->disp));
	free(d);
}

void folder_set_server(FINFO *f, const char *server, char delim)
{
	str_copy(f->server, server, sizeof(f->server));
	folder_names(f, delim);
}

FINFO *folder_add(ACCOUNT *a, const char *server, char delim, short role, short noselect, short local)
{
	FINFO *f = folder_find(a, server);
	if (!f) {
		if (a->nfolders >= MAXFOLDER)
			return 0;
		f = &a->folders[a->nfolders++];
		memset(f, 0, sizeof(*f));
		str_copy(f->server, server, sizeof(f->server));
		snprintf(f->dir, sizeof(f->dir), "F%07lX", (str_hash(server) + (local ? 1 : 0)) & 0xFFFFFFFL);
	}
	f->role = role;
	f->noselect = noselect;
	f->local = local;
	folder_names(f, delim);
	/* its directory is made when the folder is first opened: GEMDOS
	   keeps a little of its scarce internal memory for every directory
	   it has been in, and with many folders, touching them all at each
	   start fills it (TOS then halts, FOLDRxxx territory) */
	return f;
}

FINFO *folder_find(ACCOUNT *a, const char *server)
{
	short i;
	for (i = 0; i < a->nfolders; i++)
		if (!strcmp(a->folders[i].server, server))
			return &a->folders[i];
	return 0;
}

FINFO *folder_role(ACCOUNT *a, short role)
{
	short i;
	for (i = 0; i < a->nfolders; i++)
		if (a->folders[i].role == role)
			return &a->folders[i];
	/* well-known names when the server has no SPECIAL-USE */
	if (role == FR_SENT || role == FR_TRASH || role == FR_DRAFTS || role == FR_JUNK) {
		static const char *names[][4] = {
			{ "Sent", "Sent Items", "Sent Messages", "INBOX.Sent" },
			{ "Trash", "Deleted Items", "Deleted Messages", "INBOX.Trash" },
			{ "Drafts", "INBOX.Drafts", "Draft", "Drafts" },
			{ "Junk", "Spam", "INBOX.Junk", "INBOX.Spam" },
		};
		short r = role == FR_SENT ? 0 : role == FR_TRASH ? 1 : role == FR_DRAFTS ? 2 : 3, k;
		if (role == FR_SENT && a->sentname[0])
			return folder_find(a, a->sentname);
		for (k = 0; k < 4; k++)
			for (i = 0; i < a->nfolders; i++)
				if (!a->folders[i].local && !strcasecmp(a->folders[i].server, names[r][k]))
					return &a->folders[i];
	}
	return 0;
}

void folder_path(ACCOUNT *a, FINFO *f, char *out, int size)
{
	path_join(out, size, a->dir, f->dir);
}

void folders_touch(ACCOUNT *a)
{
	a->fdirty = 1;
}

int folders_flush(ACCOUNT *a)
{
	return a->fdirty ? folders_save(a) : 0;
}

int folders_save(ACCOUNT *a)
{
	SBUF b;
	char path[220];
	short i;
	int r;
	sb_init(&b);
	for (i = 0; i < a->nfolders; i++) {
		FINFO *f = &a->folders[i];
		/* the cache directory too: a renamed folder keeps its own */
		sb_printf(&b, "%d\t%d\t%d\t%ld\t%ld\t%s\t%s\r\n", f->role, f->noselect, f->local,
			  f->total, f->unread, f->server, f->dir);
	}
	path_join(path, sizeof(path), a->dir, "FOLDERS.LST");
	r = pf_save(path, b.s ? b.s : "", b.len);
	a->fdirty = 0;
	sb_free(&b);
	return r;
}

/* the local folders every account has */
static void local_folders(ACCOUNT *a)
{
	folder_add(a, "Outbox", 0, FR_OUTBOX, 0, 1);
	if (a->pop) {
		folder_add(a, "Inbox", 0, FR_INBOX, 0, 1);
		folder_add(a, "Sent", 0, FR_SENT, 0, 1);
		folder_add(a, "Trash", 0, FR_TRASH, 0, 1);
	}
}

int folders_load(ACCOUNT *a)
{
	char path[220], *buf, *line, *next;
	a->nfolders = 0;
	path_join(path, sizeof(path), a->dir, "FOLDERS.LST");
	buf = pf_load(path, 0);
	if (buf) {
		for (line = buf; line && *line; line = next) {
			char *f[7];
			short n = 0;
			next = strchr(line, '\n');
			if (next)
				*next++ = 0;
			if (*line && line[strlen(line) - 1] == '\r')
				line[strlen(line) - 1] = 0;
			f[n++] = line;
			while (n < 7) {
				char *t = strchr(f[n - 1], '\t');
				if (!t)
					break;
				*t = 0;
				f[n++] = t + 1;
			}
			if (n >= 6 && *f[5]) {
				FINFO *fi = folder_add(a, f[5], '/', (short)atoi(f[0]), (short)atoi(f[1]), (short)atoi(f[2]));
				if (fi) {
					fi->total = atol(f[3]);
					fi->unread = atol(f[4]);
					/* older lists have no directory: it follows from the name */
					if (n == 7 && f[6][0] == 'F' && strlen(f[6]) < sizeof(fi->dir))
						str_copy(fi->dir, f[6], sizeof(fi->dir));
				}
			}
		}
		free(buf);
	}
	local_folders(a);
	return a->nfolders;
}

/* ---------------- header index ---------------- */

/* A folder open in more than one place (the screen, and a check or a
 * move working on it) is one FOLDER: what one changes the other sees,
 * and the index and the pack are written by one owner (guidelines 6.5) */
#define NOPEN 16
static FOLDER *open_folders[NOPEN];

static char *dupclean(const char *s)
{
	char *d = strdup(s ? s : ""), *p;
	if (!d)
		return 0;
	for (p = d; *p; p++)
		if (*p == '\t' || *p == '\r' || *p == '\n')
			*p = ' ';
	return d;
}

void hdr_set(HDR *h, const char *from, const char *subject, const char *to)
{
	free(h->from);
	free(h->subject);
	free(h->to);
	h->from = dupclean(from);
	h->subject = dupclean(subject);
	h->to = dupclean(to);
}

static long find(FOLDER *f, unsigned long uid, int *found)
{
	long lo = 0, hi = f->n - 1;
	while (lo <= hi) {
		long mid = (lo + hi) / 2;
		if (f->h[mid].uid == uid) {
			*found = 1;
			return mid;
		}
		if (f->h[mid].uid < uid)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	*found = 0;
	return lo;
}

HDR *fold_get(FOLDER *f, unsigned long uid)
{
	int found;
	long i = find(f, uid, &found);
	return found ? &f->h[i] : 0;
}

HDR *fold_add(FOLDER *f, unsigned long uid)
{
	int found;
	long i = find(f, uid, &found);
	if (found)
		return &f->h[i];
	if (f->n == f->cap) {
		long cap = f->cap ? f->cap * 2 : 64;
		HDR *n = realloc(f->h, cap * sizeof(HDR));
		if (!n)
			return 0;
		f->h = n;
		f->cap = cap;
	}
	memmove(f->h + i + 1, f->h + i, (f->n - i) * sizeof(HDR));
	f->n++;
	memset(&f->h[i], 0, sizeof(HDR));
	f->h[i].uid = uid;
	f->h[i].off = -1;
	f->dirty = 1;
	f->gen++;
	return &f->h[i];
}

void msg_path(FOLDER *f, unsigned long uid, char *out, int size)
{
	char name[16];
	snprintf(name, sizeof(name), "%08lX.EML", uid);
	path_join(out, size, f->dir, name);
}

static void pack_path(FOLDER *f, char *out, int size)
{
	path_join(out, size, f->dir, "MESSAGES.DAT");
}

/* a body no longer wanted: its record becomes a gap, gone at the next
   compaction */
void body_drop(FOLDER *f, HDR *h)
{
	if (h->off >= 0)
		f->gaps += PACK_HEAD + h->plen;
	h->off = -1;
	h->plen = 0;
	h->flags &= ~MF_CACHED;
}

void fold_remove(FOLDER *f, unsigned long uid)
{
	int found;
	long i = find(f, uid, &found);
	if (!found)
		return;
	body_drop(f, &f->h[i]);
	free(f->h[i].from);
	free(f->h[i].subject);
	free(f->h[i].to);
	memmove(f->h + i, f->h + i + 1, (f->n - i - 1) * sizeof(HDR));
	f->n--;
	f->dirty = 1;
	f->gen++;
}

void fold_clear(FOLDER *f)
{
	while (f->n)
		fold_remove(f, f->h[f->n - 1].uid);
}

void fold_count(FOLDER *f)
{
	long i, u = 0;
	/* IMAP folders show the server's counts (STATUS), kept up to date by
	   the operations in mail.c; only a part of them is on disk */
	if (f->fi && !f->fi->local)
		return;
	for (i = 0; i < f->n; i++)
		if (!(f->h[i].flags & (MF_SEEN | MF_DELETED)))
			u++;
	if (f->fi) {
		f->fi->total = f->n;
		f->fi->unread = u;
	}
}

/* Index files before version 3 hold headers an early EMail decoded
 * wrongly. Read them again from the messages on disk; IMAP headers
 * without one are dropped, and the next sync fetches them again. */
static void upgrade_headers(FOLDER *f)
{
	long i;
	for (i = f->n - 1; i >= 0; i--) {
		HDR *h = &f->h[i];
		char path[220], *raw;
		long len;
		msg_path(f, h->uid, path, sizeof(path));
		raw = pf_load(path, &len);
		if (raw) {
			unsigned short flags = h->flags;
			long size = h->size;
			hdr_from_raw(h, raw, len);
			h->flags |= flags;
			h->size = size;
			free(raw);
		} else if (f->fi && !f->fi->local) {
			fold_remove(f, h->uid);
		}
	}
	f->dirty = 1;
}

/* where the pack ends, as far as the index knows: the records it points
   to. A record appended after the index was last saved is found at the
   next append (pack_flush asks the file) */
static long pack_end_known(FOLDER *f)
{
	long i, end = 0;
	for (i = 0; i < f->n; i++)
		if (f->h[i].off >= 0 && f->h[i].off + PACK_HEAD + f->h[i].plen > end)
			end = f->h[i].off + PACK_HEAD + f->h[i].plen;
	return end;
}

FOLDER *fold_open(ACCOUNT *a, FINFO *fi)
{
	FOLDER *f;
	char path[220], dir[200], *buf, *line, *next;
	short version;
	long i;
	int made;
	folder_path(a, fi, dir, sizeof(dir));
	for (i = 0; i < NOPEN; i++)
		if (open_folders[i] && open_folders[i]->acct == a && !strcmp(open_folders[i]->dir, dir)) {
			f = open_folders[i];
			f->fi = fi;	/* the folder list may have been read again */
			f->refs++;
			return f;
		}
	f = calloc(1, sizeof(FOLDER));
	if (!f)
		return 0;
	f->acct = a;
	f->fi = fi;
	f->refs = 1;
	str_copy(f->dir, dir, sizeof(f->dir));
	for (i = 0; i < NOPEN; i++)
		if (!open_folders[i]) {
			open_folders[i] = f;
			break;
		}
	/* a folder just made has no index to look for */
	made = !fi->opened && pf_mkdir(f->dir) == 1;
	fi->opened = 1;
	f->uidnext = 1;
	path_join(path, sizeof(path), f->dir, "INDEX.DAT");
	buf = made ? 0 : pf_load(path, 0);
	if (!buf)
		return f;
	version = 0;
	for (line = buf; line && *line; line = next) {
		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		if (!strncmp(line, "MAILIDX ", 8)) {
			char *e;
			version = (short)strtoul(line + 8, &e, 10);
			f->uidvalidity = strtoul(e, &e, 10);
			f->uidnext = strtoul(e, &e, 10);
			f->window = strtol(e, &e, 10);
			f->exists = strtol(e, &e, 10);
			f->gaps = strtol(e, &e, 10);
			continue;
		}
		{
			char *fl[9];
			short n = 1;
			HDR *h;
			fl[0] = line;
			while (n < 9 && (fl[n] = strchr(fl[n - 1], '\t'))) {
				*fl[n] = 0;
				fl[n]++;
				n++;
			}
			if (n < 7)
				continue;
			if (fl[n - 1][0] && fl[n - 1][strlen(fl[n - 1]) - 1] == '\r')
				fl[n - 1][strlen(fl[n - 1]) - 1] = 0;
			h = fold_add(f, strtoul(fl[0], 0, 10));
			if (!h)
				break;
			h->flags = (unsigned short)strtoul(fl[1], 0, 16);
			h->size = atol(fl[2]);
			h->date = strtoul(fl[3], 0, 10);
			if (n == 9) {
				h->off = atol(fl[7]);
				h->plen = atol(fl[8]);
			}
			hdr_set(h, fl[4], fl[5], fl[6]);
		}
	}
	free(buf);
	f->dirty = 0;
	if (version < 3)
		upgrade_headers(f);
	f->pend_base = pack_end_known(f);
	fold_count(f);
	return f;
}

/* ---------------- the message pack ----------------
 * A folder's bodies are records in one file, MESSAGES.DAT: "EM", the UID
 * and the length (4 bytes each, high byte first), then the message. The
 * index has each one's place. Bodies from the server gather in memory
 * (pf_bulk) and are appended in one go when the folder is saved or the
 * buffer is full; a folder only on the Atari has its message appended
 * at once. The index is saved only after the append, so it never points
 * past what is on disk; a record is checked before it is used. */

static void put4(unsigned char *p, unsigned long v)
{
	p[0] = (unsigned char)(v >> 24);
	p[1] = (unsigned char)(v >> 16);
	p[2] = (unsigned char)(v >> 8);
	p[3] = (unsigned char)v;
}

static unsigned long get4(const unsigned char *p)
{
	return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

/* what waits in memory goes to the end of the pack: one open. The
   records' places are moved if the pack is not where the index thought
   it ended. 0, or < 0 and the waiting bodies are dropped */
int pack_flush(FOLDER *f)
{
	char path[220];
	long end, i, w;
	int h, ok;
	if (!f->pend_len)
		return 0;
	pack_path(f, path, sizeof(path));
	h = pf_open(path, PF_APPEND);
	end = h >= 0 ? pf_handle_size(h) : -1;
	if (end >= 0 && pf_seek(h, end) == end)
		w = pf_write(h, f->pend, f->pend_len);
	else
		w = -1;
	if (h >= 0)
		pf_close(h);
	ok = w == f->pend_len;
	for (i = 0; i < f->n; i++) {
		HDR *m = &f->h[i];
		if (m->off < f->pend_base)
			continue;
		if (ok) {
			m->off += end - f->pend_base;
		} else {
			m->off = -1;	/* not on disk: fetched again when wanted */
			m->plen = 0;
			m->flags &= ~MF_CACHED;
		}
	}
	f->pend_base = ok ? end + w : (end >= 0 ? end : f->pend_base);
	f->pend_len = 0;
	f->dirty = 1;
	return ok ? 0 : -1;
}

static void pend_free(FOLDER *f)
{
	pf_bulk_free(f->pend);
	f->pend = 0;
	f->pend_cap = 0;
}

int msg_save(FOLDER *f, unsigned long uid, const char *data, long len)
{
	HDR *h = fold_get(f, uid);
	unsigned char head[PACK_HEAD];
	if (!h)
		return -1;
	body_drop(f, h);
	head[0] = 'E';
	head[1] = 'M';
	put4(head + 2, uid);
	put4(head + 6, (unsigned long)len);
	if (!f->pend && !f->pend_len) {
		char note[48];
		f->pend = pf_bulk(&f->pend_cap);
		if (!f->pend)
			f->pend_cap = 0;
		/* for the rigs (the emulator's console, no disk): a job's appends
		   are its bodies' bytes over this, rounded up */
		snprintf(note, sizeof(note), "EMail: pack buffer %ld bytes", f->pend_cap);
		pf_debug(note);
	}
	if (f->pend_len + PACK_HEAD + len > f->pend_cap && pack_flush(f) < 0)
		return -1;
	if (PACK_HEAD + len > f->pend_cap) {
		/* bigger than the buffer: a buffer of its own, at once */
		char *one = malloc(PACK_HEAD + len), *keep = f->pend;
		long keepcap = f->pend_cap;
		int r;
		if (!one)
			return -1;
		memcpy(one, head, PACK_HEAD);
		memcpy(one + PACK_HEAD, data, len);
		f->pend = one;
		f->pend_cap = f->pend_len = PACK_HEAD + len;
		h->off = f->pend_base;
		h->plen = len;
		r = pack_flush(f);
		free(one);
		f->pend = keep;
		f->pend_cap = keepcap;
		if (r < 0)
			return -1;
	} else {
		memcpy(f->pend + f->pend_len, head, PACK_HEAD);
		memcpy(f->pend + f->pend_len + PACK_HEAD, data, len);
		h->off = f->pend_base + f->pend_len;
		h->plen = len;
		f->pend_len += PACK_HEAD + len;
	}
	h->flags |= MF_CACHED;
	f->dirty = 1;
	/* mail only on the Atari is the user's own: on disk at once (a POP
	   download, still on the server until it quits, flushes itself) */
	if (f->fi && f->fi->local && !f->bulk && pack_flush(f) < 0)
		return -1;
	return 0;
}

/* Records follow one another to the end of the pack: a record is whole
   when its end is the end of the file or the start of another ("EM").
   That tells one cut short by a reset, with more appended after it */
static int record_whole(int h, long end, long size, const char *in, long inlen)
{
	char two[2];
	if (end == size)
		return 1;
	if (end + 2 > size)
		return 0;
	if (in && inlen >= 2)
		return in[0] == 'E' && in[1] == 'M';
	return pf_seek(h, end) == end && pf_read(h, two, 2) == 2 && two[0] == 'E' && two[1] == 'M';
}

/* the pack read through, a buffer at a time, to find each record's place
   again (after a reset in the middle of a compaction, or damage): a
   record is one whose header names a UID and length the index has and
   that is whole; of two copies, the later is the newer. What doesn't
   match is stepped over, a byte at a time */
static void pack_scan(FOLDER *f)
{
	char path[220], *buf;
	long pos = 0, got, cap, i, size;
	int h, more = 1;
	pack_path(f, path, sizeof(path));
	h = pf_open(path, PF_READ);
	size = h >= 0 ? pf_handle_size(h) : -1;
	buf = size >= 0 ? pf_bulk(&cap) : 0;
	if (!buf || cap < 4 * PACK_HEAD) {
		if (h >= 0)
			pf_close(h);
		pf_bulk_free(buf);
		return;
	}
	for (i = 0; i < f->n; i++)
		if (f->h[i].off >= 0 && f->h[i].off < f->pend_base) {
			f->h[i].off = -2;	/* unplaced */
			f->h[i].flags |= MF_FLAGSCAN;
		}
	while (more && pf_seek(h, pos) == pos && (got = pf_read(h, buf, cap)) >= PACK_HEAD) {
		long k;
		more = got == cap;
		for (k = 0; k + PACK_HEAD <= got; k++) {
			unsigned char *p = (unsigned char *)buf + k;
			HDR *m;
			long len, end;
			if (p[0] != 'E' || p[1] != 'M' || !(m = fold_get(f, get4(p + 2))) || !(m->flags & MF_FLAGSCAN))
				continue;
			len = (long)get4(p + 6);
			end = pos + k + PACK_HEAD + len;
			if (len != m->plen)
				continue;
			if (record_whole(h, end, size, end - pos + 2 <= got ? buf + (end - pos) : 0, 2))
				m->off = pos + k;	/* a later copy wins */
			pf_seek(h, pos + got);	/* record_whole may have moved it */
		}
		pos += more ? got - PACK_HEAD + 1 : got;
	}
	pf_close(h);
	pf_bulk_free(buf);
	for (i = 0; i < f->n; i++) {
		if (f->h[i].off == -2) {
			f->h[i].off = -1;
			f->h[i].plen = 0;
			f->h[i].flags &= ~MF_CACHED;
		}
		f->h[i].flags &= ~MF_FLAGSCAN;
	}
	f->dirty = 1;
}

/* one record, checked: NULL if it isn't what the index says */
static char *pack_read(FOLDER *f, HDR *m, long *len)
{
	char path[220], *buf;
	long want = PACK_HEAD + m->plen, got;
	int h;
	if (m->off >= f->pend_base && m->off + want <= f->pend_base + f->pend_len) {
		buf = malloc(m->plen + 1);
		if (!buf)
			return 0;
		memcpy(buf, f->pend + (m->off - f->pend_base) + PACK_HEAD, m->plen);
	} else {
		pack_path(f, path, sizeof(path));
		h = pf_open(path, PF_READ);
		if (h < 0)
			return 0;
		/* two bytes more: the next record's "EM", or nothing at the end */
		buf = malloc(want + 3);
		got = buf && pf_seek(h, m->off) == m->off ? pf_read(h, buf, want + 2) : -1;
		pf_close(h);
		if (got < want || (got == want + 1) || (got == want + 2 && (buf[want] != 'E' || buf[want + 1] != 'M')) ||
		    buf[0] != 'E' || buf[1] != 'M' || get4((unsigned char *)buf + 2) != m->uid ||
		    (long)get4((unsigned char *)buf + 6) != m->plen) {
			free(buf);
			return 0;
		}
		memmove(buf, buf + PACK_HEAD, m->plen);
	}
	buf[m->plen] = 0;
	if (len)
		*len = m->plen;
	return buf;
}

char *msg_load(FOLDER *f, unsigned long uid, long *len)
{
	HDR *m = fold_get(f, uid);
	char *buf;
	if (len)
		*len = 0;
	if (!m || m->off < 0)
		return 0;
	buf = pack_read(f, m, len);
	if (!buf && m->off < f->pend_base) {
		/* not where the index says: look the pack through once */
		pack_scan(f);
		if ((m = fold_get(f, uid)) != 0 && m->off >= 0)
			buf = pack_read(f, m, len);
		else
			m = 0;
	}
	if (!buf && m)
		body_drop(f, m);	/* from the server again, when wanted */
	return buf;
}

/* a body written out on its own, for a forward's attachment */
int msg_export(FOLDER *f, unsigned long uid, const char *path)
{
	long len;
	char *raw = msg_load(f, uid, &len);
	int r;
	if (!raw)
		return -1;
	r = pf_save(path, raw, len);
	free(raw);
	return r;
}

static int cmp_off(const void *x, const void *y)
{
	const HDR *a = *(HDR * const *)x, *b = *(HDR * const *)y;
	return a->off < b->off ? -1 : a->off > b->off;
}

/* The pack without its gaps: a safe save (MESSAGES.DA$, then renamed),
 * read and written a buffer at a time, when more than half of it is
 * gaps. At the end of a job, nothing waiting in memory. */
static void pack_compact(FOLDER *f)
{
	char path[220], tmp[220];
	HDR **live;
	char *buf;
	long bufcap, nlive = 0, i, inpos = -1, inlen = 0, outlen = 0, outpos = 0, half;
	int in, out, ok = 1;
	long end = pack_end_known(f);
	if (f->pend_len || f->gaps <= 0 || f->gaps * 2 < end)
		return;
	pack_path(f, path, sizeof(path));
	for (i = 0; i < f->n; i++)
		if (f->h[i].off >= 0)
			nlive++;
	if (!nlive) {
		pf_remove(path);
		f->gaps = 0;
		f->pend_base = 0;
		f->dirty = 1;
		return;
	}
	live = malloc(nlive * sizeof(HDR *));
	buf = pf_bulk(&bufcap);
	if (!live || !buf || bufcap < 2 * PACK_HEAD) {
		free(live);
		pf_bulk_free(buf);
		return;
	}
	for (i = 0, nlive = 0; i < f->n; i++)
		if (f->h[i].off >= 0)
			live[nlive++] = &f->h[i];
	qsort(live, nlive, sizeof(HDR *), cmp_off);
	half = bufcap / 2;
	str_copy(tmp, path, sizeof(tmp));
	tmp[strlen(tmp) - 1] = '$';
	in = pf_open(path, PF_READ);
	out = in >= 0 ? pf_open(tmp, PF_WRITE) : -1;
	if (out < 0)
		ok = 0;
	for (i = 0; ok && i < nlive; i++) {
		HDR *m = live[i];
		long want = PACK_HEAD + m->plen;
		char *rec;
		if (m->off >= inpos && m->off + want <= inpos + inlen) {
			rec = buf + (m->off - inpos);
		} else if (want <= half) {
			inpos = m->off;
			inlen = pf_seek(in, inpos) == inpos ? pf_read(in, buf, half) : -1;
			if (inlen < want) {
				ok = 0;
				break;
			}
			rec = buf;
		} else {
			/* bigger than half the buffer: on its own */
			rec = malloc(want);
			if (!rec || pf_seek(in, m->off) != m->off || pf_read(in, rec, want) != want) {
				free(rec);
				ok = 0;
				break;
			}
			if (outlen && pf_write(out, buf + half, outlen) != outlen)
				ok = 0;
			outpos += outlen;
			outlen = 0;
			if (ok && pf_write(out, rec, want) != want)
				ok = 0;
			free(rec);
			m->plen = -1 - m->plen;	/* placed below, at outpos - want */
			m->off = outpos;
			outpos += want;
			continue;
		}
		if (outlen + want > half) {
			if (pf_write(out, buf + half, outlen) != outlen)
				ok = 0;
			outpos += outlen;
			outlen = 0;
		}
		memcpy(buf + half + outlen, rec, want);
		m->plen = -1 - m->plen;
		m->off = outpos + outlen;
		outlen += want;
	}
	if (ok && outlen && pf_write(out, buf + half, outlen) != outlen)
		ok = 0;
	outpos += outlen;
	if (in >= 0)
		pf_close(in);
	if (out >= 0)
		pf_close(out);
	pf_bulk_free(buf);
	free(live);
	if (ok) {
		pf_remove(path);
		ok = pf_rename(tmp, path) == 0;
	}
	if (!ok) {
		pf_remove(tmp);
		/* the old pack stays, and so do the places: the new ones were
		   kept apart (plen < 0) until now */
		for (i = 0; i < f->n; i++)
			if (f->h[i].plen < 0)
				f->h[i].plen = -1 - f->h[i].plen;
		pack_scan(f);
		return;
	}
	for (i = 0; i < f->n; i++)
		if (f->h[i].plen < 0)
			f->h[i].plen = -1 - f->h[i].plen;
	f->gaps = 0;
	f->pend_base = outpos;
	f->dirty = 1;
}

int fold_save(FOLDER *f)
{
	SBUF b;
	char path[220];
	long i;
	int r;
	pack_flush(f);			/* the pack first: the index never points past it */
	pend_free(f);			/* the memory is the job's, not the folder's */
	sb_init(&b);
	sb_printf(&b, "MAILIDX 4 %lu %lu %ld %ld %ld\n", f->uidvalidity, f->uidnext, f->window, f->exists, f->gaps);
	for (i = 0; i < f->n; i++) {
		HDR *h = &f->h[i];
		sb_printf(&b, "%lu\t%x\t%ld\t%lu\t", h->uid, h->flags, h->size, h->date);
		sb_adds(&b, h->from ? h->from : "");
		sb_addc(&b, '\t');
		sb_adds(&b, h->subject ? h->subject : "");
		sb_addc(&b, '\t');
		sb_adds(&b, h->to ? h->to : "");
		sb_printf(&b, "\t%ld\t%ld\n", h->off, h->plen);
	}
	path_join(path, sizeof(path), f->dir, "INDEX.DAT");
	r = pf_save(path, b.s, b.len);
	sb_free(&b);
	f->dirty = 0;
	fold_count(f);
	return r;
}

int fold_done(FOLDER *f)
{
	return f && (f->dirty || f->pend_len) ? fold_save(f) : 0;
}

void fold_close(FOLDER *f)
{
	long i;
	if (!f)
		return;
	if (--f->refs > 0) {
		/* still open elsewhere (the screen): a local folder's change is
		   the user's own and goes to disk now; an IMAP folder's is saved
		   by the job's owner at its end (fold_done), once */
		if (f->fi && f->fi->local)
			fold_done(f);
		return;
	}
	pack_compact(f);
	if (f->dirty || f->pend_len)
		fold_save(f);
	for (i = 0; i < NOPEN; i++)
		if (open_folders[i] == f)
			open_folders[i] = 0;
	pend_free(f);
	for (i = 0; i < f->n; i++) {
		free(f->h[i].from);
		free(f->h[i].subject);
		free(f->h[i].to);
	}
	free(f->h);
	free(f);
}

void hdr_from_raw(HDR *h, const char *raw, long len)
{
	long hl = mime_header_len(raw, len);
	char *from = mime_header(raw, hl, "From"), *subj = mime_header(raw, hl, "Subject");
	char *to = mime_header(raw, hl, "To"), *date = mime_header(raw, hl, "Date");
	char *ct = mime_header(raw, hl, "Content-Type");
	char *df = hdr_decode(from ? from : ""), *ds = hdr_decode(subj ? subj : ""),
	     *dt = hdr_decode(to ? to : "");
	hdr_set(h, df, ds, dt);
	h->date = mime_date(date);
	if (ct && str_istr(ct, "multipart/mixed"))
		h->flags |= MF_ATTACH;
	free(from);
	free(subj);
	free(to);
	free(date);
	free(ct);
	free(df);
	free(ds);
	free(dt);
}

void cache_clear_all(void (*each)(const char *account, const char *folder), int all)
{
	short i, j;
	for (i = 0; i < naccts; i++) {
		ACCOUNT *a = accts[i];
		for (j = 0; j < a->nfolders; j++) {
			FINFO *fi = &a->folders[j];
			FOLDER *f;
			char path[220];
			long k;
			if (fi->local || fi->noselect || (!all && !fi->opened))
				continue;
			if (each)
				each(a->name, fi->disp);
			f = fold_open(a, fi);
			if (!f)
				continue;
			/* the bodies are all in the pack: it goes in one */
			f->pend_len = 0;
			for (k = 0; k < f->n; k++)
				if (f->h[k].flags & MF_CACHED || f->h[k].off >= 0) {
					f->h[k].flags &= ~MF_CACHED;
					f->h[k].off = -1;
					f->h[k].plen = 0;
					f->dirty = 1;
				}
			pack_path(f, path, sizeof(path));
			if (f->dirty || f->gaps)
				pf_remove(path);
			f->gaps = 0;
			f->pend_base = 0;
			fold_close(f);
		}
	}
}

/* ---------------- address book ---------------- */

char *abook_load(long *len)
{
	char path[220];
	path_join(path, sizeof(path), opt.workdir, "ADDRESS.TXT");
	return pf_load(path, len);
}

/* the addresses not in the book yet, in one go: the book is read once
   and written once, however many there are */
int abook_add_list(char addr[][160], int n)
{
	char path[220], email[96], *buf;
	SBUF add;
	int h, i, added = 0;
	buf = abook_load(0);
	sb_init(&add);
	for (i = 0; i < n; i++) {
		addr_split(addr[i], 0, 0, email, sizeof(email));
		if (!*email || !strchr(email, '@') || (buf && str_istr(buf, email)) ||
		    (add.s && str_istr(add.s, email)))
			continue;
		sb_adds(&add, addr[i]);
		sb_adds(&add, "\r\n");
		added++;
	}
	free(buf);
	if (added) {
		path_join(path, sizeof(path), opt.workdir, "ADDRESS.TXT");
		h = pf_open(path, PF_APPEND);
		if (h < 0) {
			added = -1;
		} else {
			pf_write(h, add.s, add.len);
			pf_close(h);
		}
	}
	sb_free(&add);
	return added;
}

int abook_add(const char *addr)
{
	char one[1][160];
	str_copy(one[0], addr, sizeof(one[0]));
	return abook_add_list(one, 1);
}

/* ---------------- dates ---------------- */

void date_str(unsigned long t, char *out, int size, int with_year)
{
	long z, era, doe, yoe, y, doy, mp, d, m;
	unsigned long secs;
	if (!t) {
		str_copy(out, "", size);
		return;
	}
	t += (long)zone_offset_at(t) * 60;
	secs = t % 86400;
	z = t / 86400 + 719468;
	era = z / 146097;
	doe = z - era * 146097;
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	y = yoe + era * 400;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	d = doy - (153 * mp + 2) / 5 + 1;
	m = mp < 10 ? mp + 3 : mp - 9;
	if (m <= 2)
		y++;
	if (with_year)
		snprintf(out, size, "%02ld.%02ld.%02ld %02lu:%02lu", d, m, y % 100, secs / 3600, (secs / 60) % 60);
	else
		snprintf(out, size, "%02ld.%02ld %02lu:%02lu", d, m, secs / 3600, (secs / 60) % 60);
}
