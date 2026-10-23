/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * unit.c - checks of the mail core that need no server: charsets
 * (Hebrew included), MIME decoding, message building, bidi, dates.
 * Built for the development machine by `make test`.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "charset.h"
#include "mime.h"
#include "compose.h"
#include "store.h"
#include "bidi.h"
#include "util.h"
#include "plat.h"
#include "tls.h"
#include "secret.h"

static int fails, checks;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* Atari bytes for the Hebrew word shalom (shin lamed vav final-mem) */
static const char shalom_atari[] = "\xD6\xCD\xC7\xDA";
static const char shalom_utf8[] = "\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D";

static void t_charset(void)
{
	char *s;
	long n;

	s = cs_to_atari(shalom_utf8, strlen(shalom_utf8), CS_UTF8, &n);
	CHECK(n == 4 && !memcmp(s, shalom_atari, 4), "utf8 hebrew -> atari got %ld bytes", n);
	free(s);

	/* ISO-8859-8 and windows-1255 shalom: F9 EC E5 ED */
	s = cs_to_atari("\xF9\xEC\xE5\xED", 4, CS_ISO8859_8, &n);
	CHECK(n == 4 && !memcmp(s, shalom_atari, 4), "iso-8859-8 hebrew");
	free(s);
	s = cs_to_atari("\xF9\xEC\xE5\xED", 4, CS_CP1255, &n);
	CHECK(n == 4 && !memcmp(s, shalom_atari, 4), "cp1255 hebrew");
	free(s);

	/* niqqud and direction marks disappear */
	s = cs_to_atari("\xD7\xA9\xD6\xB8\xE2\x80\x8F", 7, CS_UTF8, &n);
	CHECK(n == 1 && (unsigned char)s[0] == 0xD6, "niqqud dropped, got %ld", n);
	free(s);

	/* Latin-1 accents map to the ST font; curly quotes become ASCII */
	/* a UTF-8 letter split between two encoded words */
	{
		char *d = hdr_decode("=?UTF-8?B?1w==?= =?UTF-8?B?qQ==?=");
		CHECK(d && !strcmp(d, "\xD6"), "utf-8 letter split across words");
		free(d);
	}
	/* real Latin-1 capitals stay Latin-1 */
	s = cs_to_atari("\xC7" "a a \xE9t\xE9 \xC9\xC9", 11, CS_LATIN1, &n);
	CHECK(n == 11 && (unsigned char)s[0] == 0x80 && (unsigned char)s[10] == 0x90, "latin-1 kept");
	free(s);
	s = cs_to_atari("caf\xC3\xA9 \xE2\x80\x9Cok\xE2\x80\x9D", 14, CS_UTF8, &n);
	CHECK(!strcmp(s, "caf\x82 \"ok\""), "accents/quotes: %s", s);
	free(s);

	s = atari_to_utf8(shalom_atari, 4, &n);
	CHECK(n == 8 && !memcmp(s, shalom_utf8, 8), "atari -> utf8 hebrew");
	free(s);

	s = hdr_decode("=?UTF-8?B?16nXnNeV150=?= =?UTF-8?Q?_world?=");
	CHECK(!strcmp(s, "\xD6\xCD\xC7\xDA world"), "rfc2047 decode: [%s]", s);
	free(s);

	s = hdr_decode("=?windows-1255?Q?=F9=EC=E5=ED?= from Tel Aviv");
	CHECK(!strcmp(s, "\xD6\xCD\xC7\xDA from Tel Aviv"), "rfc2047 cp1255: [%s]", s);
	free(s);

	s = hdr_encode("Shalom \xD6\xCD\xC7\xDA");
	CHECK(!strncmp(s, "=?UTF-8?B?", 10), "rfc2047 encode: %s", s);
	{
		char *back = hdr_decode(s);
		CHECK(!strcmp(back, "Shalom \xD6\xCD\xC7\xDA"), "encode round trip [%s]", back);
		free(back);
	}
	free(s);

	s = mutf7_to_atari("&BdIF3AXTBdk-");	/* gimel lamed dalet yod */
	CHECK(!strcmp(s, "\xC4\xCD\xC5\xCB"), "mutf7 decode [%s]", s);
	{
		char *e = atari_to_mutf7(s);
		CHECK(!strcmp(e, "&BdIF3AXTBdk-"), "mutf7 encode [%s]", e);
		free(e);
	}
	free(s);
	s = mutf7_to_atari("Tom &- Jerry");
	CHECK(!strcmp(s, "Tom & Jerry"), "mutf7 ampersand");
	free(s);

	s = qp_decode("a=3Db=\r\nc", 10, 0, &n);
	CHECK(!strcmp(s, "a=bc"), "qp decode [%s]", s);
	free(s);
	s = base64_decode("aGVsbG8=", 8, &n);
	CHECK(n == 5 && !strcmp(s, "hello"), "base64");
	free(s);
}

static const char *multipart =
	"From: =?UTF-8?B?15PXoNeU?= <dana@example.com>\r\n"
	"To: me@example.com\r\n"
	"Subject: =?UTF-8?B?16nXnNeV150=?=\r\n"
	"Date: Thu, 08 Oct 2026 18:30:00 +0300\r\n"
	"Message-ID: <abc@example.com>\r\n"
	"MIME-Version: 1.0\r\n"
	"Content-Type: multipart/mixed; boundary=\"XX\"\r\n"
	"\r\n"
	"preamble\r\n"
	"--XX\r\n"
	"Content-Type: multipart/alternative; boundary=YY\r\n"
	"\r\n"
	"--YY\r\n"
	"Content-Type: text/plain; charset=utf-8\r\n"
	"Content-Transfer-Encoding: base64\r\n"
	"\r\n"
	"16nXnNeV150gMTk5MA==\r\n"
	"--YY\r\n"
	"Content-Type: text/html; charset=utf-8\r\n"
	"\r\n"
	"<p>HTML version</p>\r\n"
	"--YY--\r\n"
	"--XX\r\n"
	"Content-Type: image/png; name=\"pic.png\"\r\n"
	"Content-Disposition: attachment; filename*=utf-8''%D7%AA.png\r\n"
	"Content-Transfer-Encoding: base64\r\n"
	"\r\n"
	"iVBORw0KGgo=\r\n"
	"--XX--\r\n";

static void t_mime(void)
{
	MSG *m = mime_parse(multipart, strlen(multipart));
	long n;
	char *d;
	CHECK(m != 0, "parse");
	CHECK(!strcmp(m->subject, shalom_atari), "subject [%s]", m->subject);
	CHECK(!strcmp(m->text, "\xD6\xCD\xC7\xDA 1990"), "plain part chosen: [%s]", m->text);
	CHECK(m->nparts == 1, "one attachment, got %d", m->nparts);
	CHECK(m->nparts && !strcmp(m->parts[0].name, "\xD7.png"), "rfc2231 filename [%s]",
	      m->nparts ? m->parts[0].name : "");
	d = mime_part_data(multipart, &m->parts[0], &n);
	CHECK(n == 8 && !memcmp(d, "\x89PNG\r\n\x1a\n", 8), "attachment data %ld", n);
	free(d);
	CHECK(mime_date("Thu, 08 Oct 2026 18:30:00 +0300") == 1791473400UL,
	      "date %lu", mime_date("Thu, 08 Oct 2026 18:30:00 +0300"));
	mime_free(m);

	{
		const char *h = "<html><head><style>p{}</style></head><body><p>Hello&nbsp;<b>you</b></p>"
				"<ul><li>one</li><li>&#1513;</li></ul>&lt;3</body></html>";
		char *t = html_to_text(h, strlen(h), &n);
		CHECK(!strcmp(t, "Hello you\n\n  - one\n  - \xD6\n<3"), "html: [%s]", t);
		free(t);
	}
	{
		char name[64], email[64];
		addr_split("\"Cohen, Dana\" <dana@example.com>", name, 64, email, 64);
		CHECK(!strcmp(name, "Cohen, Dana") && !strcmp(email, "dana@example.com"), "addr_split %s|%s", name, email);
		addr_split("avi@example.org (Avi)", name, 64, email, 64);
		CHECK(!strcmp(name, "Avi") && !strcmp(email, "avi@example.org"), "addr_split comment %s|%s", name, email);
	}
}

/* the address link_next finds first in s, or "" */
static const char *first_link(const char *s)
{
	static char out[200];
	long at, n;
	out[0] = 0;
	if (link_next(s, (long)strlen(s), 0, &at, &n) && n < (long)sizeof(out)) {
		memcpy(out, s + at, n);
		out[n] = 0;
	}
	return out;
}

static void t_links(void)
{
	static const struct { const char *text, *link; } cases[] = {
		{ "See https://atari.org/news.", "https://atari.org/news" },
		{ "(at http://example.com/a_(b))!", "http://example.com/a_(b)" },
		{ "go to www.frogfind.com, then", "www.frogfind.com" },
		{ "<http://example.com/x?a=1&b=2>", "http://example.com/x?a=1&b=2" },
		{ "\"HTTP://EXAMPLE.COM\"", "HTTP://EXAMPLE.COM" },
		{ "write to mailto:dana@example.com?subject=Hi;", "mailto:dana@example.com?subject=Hi" },
		{ "**https://bold.example/**", "https://bold.example/" },
		{ "not xhttp://a.b or a.www.b or http:// or www.", "" },
		{ "\xD6 http://example.com/\xD6", "http://example.com/" },
		{ "\xD7\xD6\xC3-www.atari.org \xD6", "www.atari.org" },
	};
	short k;
	char *u;
	for (k = 0; k < (short)(sizeof(cases) / sizeof(cases[0])); k++)
		CHECK(!strcmp(first_link(cases[k].text), cases[k].link), "link in [%s]: [%s]", cases[k].text,
		      first_link(cases[k].text));
	{
		const char *t = "one http://a.example two www.b.example";
		long at, n, from = 0, found = 0;
		while (link_next(t, (long)strlen(t), from, &at, &n)) {
			found++;
			from = at + n;
		}
		CHECK(found == 2, "two links, got %ld", found);
	}
	u = link_url("www.atari.org", 13);
	CHECK(!strcmp(u, "http://www.atari.org"), "www gets http:// [%s]", u);
	free(u);
	CHECK(link_wanted("https://x") && link_wanted("mailto:a@b") && !link_wanted("javascript:x()") &&
	      !link_wanted("#top") && !link_wanted("http://"), "link_wanted");
	{
		const char *h = "<p>Read <a class=x href=\"https://example.com/a?x=1&amp;y=2\">the <b>news</b></a>"
				" or <A HREF='mailto:dana@example.com'> Dana </A>, not <a href=\"#top\">top</a>"
				" or <a href=\"javascript:go()\">this</a>.</p><pre>a\r\nb</pre><a href=\"http://x.example\">x";
		MLINK *lk = 0;
		short nl = 0;
		long n;
		char *t = html_to_text_links(h, strlen(h), &n, &lk, &nl);
		CHECK(!strcmp(t, "Read the news or Dana , not top or this.\n\na\nbx"), "html links text [%s]", t);
		CHECK(nl == 3, "three links kept, got %d", nl);
		if (nl == 3) {
			CHECK(!strncmp(t + lk[0].at, "the news", lk[0].n) && lk[0].n == 8 &&
			      !strcmp(lk[0].url, "https://example.com/a?x=1&y=2"), "first link [%.*s] %s",
			      (int)lk[0].n, t + lk[0].at, lk[0].url);
			CHECK(!strncmp(t + lk[1].at, "Dana", lk[1].n) && lk[1].n == 4 &&
			      !strcmp(lk[1].url, "mailto:dana@example.com"), "second link [%.*s] %s",
			      (int)lk[1].n, t + lk[1].at, lk[1].url);
			CHECK(lk[2].n == 1 && t[lk[2].at] == 'x', "an unclosed <a> ends with the text");
		}
		for (n = 0; n < nl; n++)
			free(lk[n].url);
		free(lk);
		free(t);
	}
	{
		const char *raw = "Subject: links\r\nContent-Type: multipart/mixed; boundary=b\r\n\r\n"
				  "--b\r\nContent-Type: text/html\r\n\r\n<p>Hi <a href=\"http://one.example\">one</a></p>\r\n"
				  "--b\r\nContent-Type: text/html\r\n\r\n<p>And <a href=\"http://two.example\">two</a></p>\r\n"
				  "--b--\r\n";
		MSG *m = mime_parse(raw, strlen(raw));
		CHECK(m && m->nlinks == 2, "links of two parts: %d", m ? m->nlinks : -1);
		if (m && m->nlinks == 2) {
			CHECK(!strncmp(m->text + m->links[0].at, "one", 3) && !strncmp(m->text + m->links[1].at, "two", 3),
			      "links where their parts' text is [%s]", m->text);
			CHECK(!strcmp(m->links[1].url, "http://two.example"), "second part's link %s", m->links[1].url);
		}
		mime_free(m);
	}
}

static void t_compose(void)
{
	ACCOUNT a;
	char err[200], rc[10][96];
	char *raw, *smtp, text[400];
	long n, sl;
	int k;
	MSG *m;
	memset(&a, 0, sizeof(a));
	strcpy(a.email, "me@example.com");
	strcpy(a.fullname, "Yaary \xC2");
	snprintf(text, sizeof(text),
		 "To: \"Cohen, Dana\" <dana@example.com>, avi@example.org\n"
		 "Cc: \n"
		 "Bcc: secret@example.net\n"
		 "Subject: %s 1990\n"
		 "\n"
		 "%s, this is EMail.\n.leading dot\n", shalom_atari, shalom_atari);
	raw = compose_build(&a, text, strlen(text), "<abc@example.com>", "<abc@example.com>", err, sizeof(err), &n);
	CHECK(raw != 0, "build: %s", err);
	if (!raw)
		return;
	k = compose_rcpts(raw, n, rc, 10);
	CHECK(k == 3 && !strcmp(rc[0], "dana@example.com") && !strcmp(rc[2], "secret@example.net"),
	      "rcpts %d", k);
	smtp = compose_for_smtp(raw, n, &sl);
	CHECK(!strstr(smtp, "secret@"), "bcc stripped");
	CHECK(strstr(smtp, "Content-Transfer-Encoding: quoted-printable") != 0, "qp body");
	m = mime_parse(smtp, sl);
	CHECK(!strcmp(m->subject, "\xD6\xCD\xC7\xDA 1990"), "subject round trip [%s]", m->subject);
	CHECK(strstr(m->text, "\xD6\xCD\xC7\xDA, this is EMail.") != 0, "body round trip [%s]", m->text);
	CHECK(strstr(m->from, "Yaary \xC2") != 0, "from name round trip [%s]", m->from);
	CHECK(strstr(m->to, "Cohen, Dana") != 0, "quoted name [%s]", m->to);
	mime_free(m);
	free(raw);
	free(smtp);

	strcpy(opt.signature, "Yaary\nSent from my Falcon");
	raw = compose_new(&a, "dana@example.com");
	CHECK(!strcmp(raw, "To: dana@example.com\nCc: \nSubject: \n\n\n\n-- \nYaary\nSent from my Falcon"),
	      "new message with signature [%s]", raw);
	free(raw);

	/* mailto: links, as EContact sends them with VA_START */
	opt.signature[0] = 0;
	raw = compose_mailto(&a, "Maya Carter <maya@pixelpress.example>");
	CHECK(!strcmp(raw, "To: Maya Carter <maya@pixelpress.example>\nCc: \nSubject: \n\n"),
	      "mailto: a name and address [%s]", raw);
	free(raw);
	raw = compose_mailto(&a, "a@b.example,c@d.example?subject=Hello%20there&cc=e@f.example&body=Line%201%0D%0ALine%202");
	CHECK(!strcmp(raw, "To: a@b.example,c@d.example\nCc: e@f.example\nSubject: Hello there\n\nLine 1\nLine 2"),
	      "mailto: subject, cc, body, escapes [%s]", raw);
	free(raw);
	raw = compose_mailto(&a, "?to=x@y.example&TO=z@y.example&Subject=a%0Ab&subject=second&odd&=v&body=%zz%4");
	CHECK(!strcmp(raw, "To: x@y.example, z@y.example\nCc: \nSubject: a b\n\n%zz%4"),
	      "mailto: to= twice, one subject, a line break kept out of it, bad escapes kept [%s]", raw);
	free(raw);
	raw = compose_mailto(&a, "");
	CHECK(!strcmp(raw, "To: \nCc: \nSubject: \n\n"), "mailto: nothing [%s]", raw);
	free(raw);
	strcpy(opt.signature, "Yaary");
	raw = compose_mailto(&a, "q@r.example?body=Hi");
	CHECK(!strcmp(raw, "To: q@r.example\nCc: \nSubject: \n\nHi\n\n-- \nYaary"), "mailto: body, then the signature [%s]", raw);
	free(raw);

	/* a reply and a forward: the signature goes with what is typed, above the quote */
	{
		MSG o;
		memset(&o, 0, sizeof(o));
		o.from = "dana@example.com";
		o.date = "Tue, 20 Oct 2026 10:00:00 +0300";
		o.subject = "Lunch";
		o.to = "me@example.com";
		o.cc = o.reply_to = "";
		o.text = "See you at one.";
		raw = compose_reply(&a, &o, 0);
		CHECK(!strcmp(raw, "To: dana@example.com\nCc: \nSubject: Re: Lunch\n\n\n\nYaary\n\n"
			      "On Tue, 20 Oct 2026 10:00:00 +0300,\ndana@example.com wrote:\n> See you at one.\n"),
		      "reply: the signature above the quote [%s]", raw);
		free(raw);
		raw = compose_forward(&a, &o, 0);
		CHECK(!strcmp(raw, "To: \nCc: \nSubject: Fwd: Lunch\n\n\n\nYaary\n\n---------- Forwarded message ----------\n"
			      "From: dana@example.com\nDate: Tue, 20 Oct 2026 10:00:00 +0300\nSubject: Lunch\nTo: me@example.com\n\n"
			      "See you at one."),
		      "forward: the signature above the forwarded text [%s]", raw);
		free(raw);
		opt.signature[0] = 0;
		raw = compose_reply(&a, &o, 0);
		CHECK(!strncmp(raw, "To: dana@example.com\nCc: \nSubject: Re: Lunch\n\n\n\nOn Tue", 50),
		      "reply without a signature, as before [%s]", raw);
		free(raw);
	}
}

static void t_bidi(void)
{
	char out[64];
	/* "shalom 1990" in a Hebrew paragraph: number stays LTR, word reversed, at the right */
	const char *s = "\xD6\xCD\xC7\xDA 1990";
	bidi_visual(s, (short)strlen(s), bidi_is_rtl(s, (short)strlen(s)), out);
	CHECK(!memcmp(out, "1990 \xDA\xC7\xCD\xD6", 9), "bidi visual [%.9s]", out);
}

/* the root certificates shipped with EMail load as BearSSL trust anchors */
static void t_tls(void)
{
	char err[200];
	int n;
	snprintf(tls_roots_cache, sizeof(tls_roots_cache), "/tmp/mail-unit-roots.%d", (int)getpid());
	remove(tls_roots_cache);
	n = tls_load_anchors("common/certs/CACERT.PEM", err, sizeof(err));
	CHECK(n == 121, "common/certs/CACERT.PEM: %d roots (%s)", n, n ? "" : err);
	CHECK(tls_load_anchors("common/certs/CACERT.PEM", err, sizeof(err)) == n, "anchors loaded once");
	CHECK(pf_size(tls_roots_cache) > 1000, "decoded roots cached");
	tls_free_anchors();
	n = tls_load_anchors("common/certs/CACERT.PEM", err, sizeof(err));
	CHECK(n == 121, "roots read back from the cache: %d", n);
	remove(tls_roots_cache);
}

static void t_num(void)
{
	CHECK(!strcmp(num(0), "0"), "0");
	CHECK(!strcmp(num(999), "999"), "999");
	CHECK(!strcmp(num(1000), "1,000"), "1,000: %s", num(1000));
	CHECK(!strcmp(num(1234567), "1,234,567"), "1,234,567: %s", num(1234567));
	CHECK(!strcmp(num(-45210), "-45,210"), "-45,210: %s", num(-45210));
}

static void forget_accounts(void)
{
	while (naccts)
		free(accts[--naccts]);
}

/* time zones by name, like ECal: summer time by the date, and the
   machine's zone found from a POSIX TZ */
static void t_zone(void)
{
	char dir[64], path[100];
	snprintf(dir, sizeof(dir), "/tmp/mail-unit-tz.%d", (int)getpid());
	mkdir(dir, 0755);
	snprintf(path, sizeof(path), "%s/EMAIL.INF", dir);
	pf_save(path, "[OPTIONS]\r\nTIMEZONE=Asia/Jerusalem\r\n", 34);
	forget_accounts();
	store_init(dir);
	CHECK(!strcmp(opt.tzname, "Asia/Jerusalem"), "TIMEZONE read: %s", opt.tzname);
	CHECK(zone_offset_at(1783000000UL) == 180, "Jerusalem in July 2026: +3");	/* 2026-07-02 */
	CHECK(zone_offset_at(1767225600UL) == 120, "Jerusalem in January: +2");	/* 2026-01-01 */
	pf_save(path, "[OPTIONS]\r\n", 11);
	setenv("TZ", "IST-2IDT,M3.4.4/26,M10.5.0", 1);
	forget_accounts();
	store_init(dir);
	CHECK(!strcmp(opt.tzname, "Asia/Jerusalem"), "TZ=IST-2IDT found: %s", opt.tzname);
	setenv("TZ", "Europe/Paris", 1);
	forget_accounts();
	store_init(dir);
	CHECK(!strcmp(opt.tzname, "Europe/Paris"), "TZ=Europe/Paris found: %s", opt.tzname);
	unsetenv("TZ");
}

/* EMAIL.INF: names in capitals, a note, the setting and an empty line;
   read back the same, a password with " ;" and the two-line signature too */
static void t_inf(void)
{
	char dir[64], path[100], *inf, *p;
	ACCOUNT *a;
	long len;
	int blank_after = 1;
	snprintf(dir, sizeof(dir), "/tmp/mail-unit-inf.%d", (int)getpid());
	mkdir(dir, 0755);
	snprintf(path, sizeof(path), "%s/EMAIL.INF", dir);
	{
		static const char old[] = "[options]\r\ntz=180\r\nsignature=Dana\\nTel Aviv\r\n"
			"peoplefile=1\r\npeopledir=C:\\ECONTACT\r\n[account]\r\nname=Home\r\n"
			"email=a@me.com\r\nuser=a\r\npass=x ;y\r\n";
		pf_save(path, old, (long)strlen(old));
	}
	unsetenv("TZ");			/* no machine zone: the old minutes decide */
	CHECK(store_init(dir) == 1, "old lower-case EMAIL.INF read");
	CHECK(opt.tzname[0] && zone_offset_now() == 180, "TZ=180 became a zone: %s", opt.tzname);
	CHECK(!strcmp(accts[0]->pass, "x ;y"), "password with ' ;' kept: %s", accts[0]->pass);
	CHECK(store_save_settings() == 0, "settings saved");
	inf = pf_load(path, &len);
	CHECK(inf && strstr(inf, "\r\n[OPTIONS]\r\n") && strstr(inf, "\r\n[ACCOUNT]\r\n"), "sections in capitals");
	CHECK(inf && strstr(inf, "; your time zone") && strstr(inf, "\r\nTIMEZONE=") && !strstr(inf, "\r\nTZ="),
	      "note, TIMEZONE=, empty line");
	CHECK(inf && strstr(inf, "\r\nPASS={E}") && !strstr(inf, "x ;y"), "PASS written encrypted");
	CHECK(inf && strstr(inf, "\r\nPEOPLEFILE=1\r\n") && strstr(inf, "\r\nPEOPLEDIR=C:\\ECONTACT\r\n"),
	      "PEOPLEFILE=, and PEOPLEDIR= as it is (one backslash)");
	CHECK(inf && strstr(inf, "\r\nSIGNATURE=Dana\\nTel Aviv\r\n") &&
	      strstr(inf, "\r\nSIGNATURE=") < strstr(inf, "\r\n[ACCOUNT]\r\n"), "SIGNATURE written, in [OPTIONS]");
	/* every setting line: a note above, an empty line below */
	for (p = inf; p && (p = strstr(p, "\r\n")); p += 2) {
		const char *l = p + 2;
		if (*l >= 'A' && *l <= 'Z' && strchr(l, '=') && strchr(l, '=') < strstr(l, "\r\n"))
			if (strncmp(strstr(l, "\r\n"), "\r\n\r\n", 4))
				blank_after = 0;
	}
	CHECK(blank_after, "an empty line after every setting");
	forget_accounts();		/* store_init adds to what is loaded */
	store_init(dir);
	a = accts[0];
	CHECK(!strcmp(a->pass, "x ;y") && !strcmp(opt.signature, "Dana\nTel Aviv") && zone_offset_now() == 180,
	      "read back from the new EMAIL.INF");
	CHECK(opt.peoplefile == 1 && !strcmp(opt.peopledir, "C:\\ECONTACT"), "EContact's file read back: %d %s",
	      opt.peoplefile, opt.peopledir);
	CHECK(!store_lost_passwords, "EMAIL.KEY opens them");
	/* another key: the password is gone, and EMail says so */
	{
		char kf[120];
		snprintf(kf, sizeof(kf), "%s/EMAIL.KEY", dir);
		unlink(kf);
		forget_accounts();
		store_init(dir);
		CHECK(store_lost_passwords && !accts[0]->pass[0], "a new EMAIL.KEY can't open the old passwords");
		unlink(kf);
	}
	{
		char enc[300], dec[100];
		secret_init("/tmp/mail-unit-key");
		secret_encode("app-pass-1234", enc, sizeof(enc));
		CHECK(!strncmp(enc, "{E}", 3) && secret_decode(enc, dec, sizeof(dec)) && !strcmp(dec, "app-pass-1234"),
		      "encrypt and decrypt: %s", enc);
		secret_encode("app-pass-1234", dec, sizeof(dec));
		CHECK(strcmp(enc, dec), "a new nonce each time");
		CHECK(secret_decode("plain", dec, sizeof(dec)) && !strcmp(dec, "plain"), "plain value read as it is");
		unlink("/tmp/mail-unit-key");
	}
	forget_accounts();
	free(inf);
	unlink(path);
	rmdir(dir);
}

int main(void)
{
	t_num();
	t_zone();
	t_inf();
	t_charset();
	t_mime();
	t_links();
	t_compose();
	t_bidi();
	t_tls();
	printf("%d checks, %d failed\n", checks, fails);
	return fails != 0;
}
