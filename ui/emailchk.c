/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * emailchk.c - EMAILCHK.ACC, "EMail check": a desk accessory that looks
 * for new mail in the Inbox of EMail's accounts while EMail isn't
 * running, as often as EMail's own timed check (CHECK= in EMAIL.INF),
 * and asks whether to open EMail when some has come.
 *
 * It reads EMail's files and never writes them: EMAIL.INF and EMAIL.KEY
 * (the accounts, their passwords), each Inbox's INDEX.DAT (what EMail
 * last saw) or UIDL.DAT (POP3), src/newmail.c. Its own settings are
 * X:\EMAILCHK.INF in the boot drive's root (the guidelines, 6.1):
 * FOLDER=, EMail's folder (asked for the first time the accessory is
 * opened), START=, MEMKB= and HEAPKB=.
 *
 * An accessory's rules (the guidelines, 2.2): nothing read before the
 * first event call but its INF; the first check a minute after boot
 * (START=), then every CHECK= minutes, only while the mouse and keys rest
 * (bg_may_work: a Direct connection's handshake holds the machine for a
 * second or more), never while EMAIL.PRG runs (twin.c: EMail answers
 * TWIN_ASK). Opened from the Desk menu: when it last looked, Check now,
 * About. Started as a program, it checks once and says what it found.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui.h"
#include "tos.h"
#include "plat.h"
#include "alert.h"
#include "dialog.h"
#include "aboutbox.h"
#include "accmem.h"
#include "bgwork.h"
#include "twin.h"
#include "busy.h"
#include "av.h"
#include "newmail.h"
#include "tls.h"
#include "tlsovl.h"
#include "icon.h"
#include "icon16.h"
#include "icon256.h"
#include "../src/version.h"

short vdi_h, cw, ch, scr_w, scr_h, desk_x, desk_y, desk_w, desk_h, planes, gl_wchar, gl_hchar, ap_id;
extern short _is_acc;		/* crt0.S */

static short is_acc, menu_id = -1;
static char inf[16] = "C:\\EMAILCHK.INF";
static char folder[200];	/* EMail's, with EMAIL.INF; "" not known */
static char start_v[16] = "60", heapkb_v[12], memkb_v[12];
static unsigned long inf_stamp;	/* EMAIL.INF's date and size when read */
static long inf_size = -2;
static NEWMAIL nm[MAXACCT];
static unsigned long last_check;	/* pf_ms(), 0 never */
static long last_new;			/* what it found then */
static char last_err[120];
static short checked;			/* checked since the machine started */
static short prg_ran;			/* EMAIL.PRG was seen running */
static short reread = 1;		/* EMAIL.INF to be looked at before the next check */

#define FIRST_HEAP	(64L * 1024)
#define DIRECT_ROOM	(232L * 1024)

/* the stack crt0 gives us lies just above _is_acc: painted at the start,
   the paint left tells how deep it has been (the console line, for the
   rigs: STACK_SIZE in the Makefile is set from it) */
#define PAINT 0x5A5A5A5AUL
static unsigned long *stack_low, stack_top;

static void stack_paint(void)
{
	unsigned long here, *p;
	stack_top = (unsigned long)&here;	/* (main's frame above it: a few bytes) */
	stack_low = (unsigned long *)(((unsigned long)&_is_acc + 2 + 3) & ~3UL);
	for (p = stack_low; (unsigned long)p < (unsigned long)&here - 1024; p++)
		*p = PAINT;
}

static long stack_used(void)
{
	unsigned long *p = stack_low;
	while (*p == PAINT)
		p++;
	return (long)(stack_top - (unsigned long)p);
}

/* ---------------- the few drawing calls the shared About box and
   dialogs take (gemui.h): the system font, no Hebrew mapping ---------------- */

short text_pen = 1;

void clip_on(GRECT *r)
{
	short pxy[4];
	pxy[0] = r->x;
	pxy[1] = r->y;
	pxy[2] = r->x + r->w - 1;
	pxy[3] = r->y + r->h - 1;
	vs_clip(vdi_h, 1, pxy);
}

void clip_off(void)
{
	short pxy[4] = { 0, 0, 0, 0 };
	pxy[2] = scr_w - 1;
	pxy[3] = scr_h - 1;
	vs_clip(vdi_h, 0, pxy);
}

void fill(GRECT *r, short color)
{
	short pxy[4];
	if (r->w <= 0 || r->h <= 0)
		return;
	pxy[0] = r->x;
	pxy[1] = r->y;
	pxy[2] = r->x + r->w - 1;
	pxy[3] = r->y + r->h - 1;
	vsf_interior(vdi_h, 1);
	vsf_color(vdi_h, color);
	vsf_perimeter(vdi_h, 0);
	vswr_mode(vdi_h, 1);
	vr_recfl(vdi_h, pxy);
}

void line_col(short x1, short y1, short x2, short y2, short color)
{
	short p[4];
	p[0] = x1;
	p[1] = y1;
	p[2] = x2;
	p[3] = y2;
	vswr_mode(vdi_h, 1);
	vsl_color(vdi_h, color);
	v_pline(vdi_h, 2, p);
}

void hline(short x1, short x2, short y)
{
	line_col(x1, y, x2, y, 1);
}

void frame(GRECT *r, short color)
{
	short p[10];
	p[0] = p[6] = p[8] = r->x;
	p[1] = p[3] = p[9] = r->y;
	p[2] = p[4] = r->x + r->w - 1;
	p[5] = p[7] = r->y + r->h - 1;
	vswr_mode(vdi_h, 1);
	vsl_color(vdi_h, color);
	v_pline(vdi_h, 5, p);
}

void text_at(short x, short y, const char *s, long n, short cols, short flags)
{
	if (n > cols)
		n = cols;
	if (n > 120)
		n = 120;
	if (n <= 0)
		return;
	vswr_mode(vdi_h, 2);
	vst_color(vdi_h, (flags & TX_INVERSE) ? 0 : text_pen);
	vst_effects(vdi_h, (flags & TX_BOLD) ? 1 : (flags & TX_LIGHT) ? 2 : 0);
	v_gtext_n(vdi_h, x, y, s, (short)n);
	vst_effects(vdi_h, 0);
}

/* the system font is the only one here */
void font_sys_push(void)
{
	vst_font(vdi_h, 1);
	vst_height(vdi_h, gl_hchar >= 16 ? 13 : 6, &cw, &ch);
	cw = gl_wchar;
	ch = gl_hchar;
}

void font_sys_pop(void)
{
}

/* the About box's icon goes to the screen from ST RAM: taken before the
   first event call (an accessory's memory, 2.2) */
static unsigned short st_fallback[256];
static unsigned short *st_buf = st_fallback;

void *st_ram(long n)
{
	return n <= (long)sizeof(st_fallback) ? st_buf : 0;
}

/* ---------------- EMAILCHK.INF ---------------- */

static const char inf_notes[] =
	"; EMail check (EMAILCHK.ACC): looks for new mail in the Inbox of\r\n"
	"; EMail's accounts while EMail isn't running, as often as EMail's\r\n"
	"; own timed check (CHECK= in EMAIL.INF; 0 there: never on its own).\r\n"
	"\r\n";

static const struct {
	const char *key, *note;
	char *val;
	short size;
} keys[] = {
	{ "FOLDER", "; EMail's folder, where EMAIL.PRG and EMAIL.INF are", folder, sizeof(folder) },
	{ "START", "; the first check: seconds after the machine started, \"now\" or\r\n"
		   "; \"opened\" (once opened from the Desk menu)", start_v, sizeof(start_v) },
	{ "MEMKB", "; the memory it took the last time it measured, KB (it measures\r\n"
		   "; itself; delete the line to start again)", memkb_v, sizeof(memkb_v) },
	{ "HEAPKB", "; memory to take at boot, KB, in place of what it measured", heapkb_v, sizeof(heapkb_v) },
};
#define NKEYS (short)(sizeof(keys) / sizeof(keys[0]))

static void inf_read(void)
{
	char *buf = pf_load(inf, 0), *line, *next;
	if (!buf)
		return;
	for (line = buf; line && *line; line = next) {
		char *eq, *k;
		short i;
		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		k = str_trim(line);
		if (*k == ';' || !(eq = strchr(k, '=')))
			continue;
		*eq = 0;
		for (i = 0; i < NKEYS; i++)
			if (!strcasecmp(str_trim(k), keys[i].key))
				str_copy(keys[i].val, str_trim(eq + 1), keys[i].size);
	}
	free(buf);
}

/* written whole: each setting a paragraph, its note, its line and an
   empty line; one with no value is left out. The notes are written anew;
   a setting it doesn't know is kept, a paragraph of its own at the end */
static void inf_write(void)
{
	SBUF b;
	char *old = pf_load(inf, 0), *line, *next;
	short i;
	sb_init(&b);
	sb_adds(&b, inf_notes);
	for (i = 0; i < NKEYS; i++) {
		if (!keys[i].val[0])
			continue;
		sb_adds(&b, keys[i].note);
		sb_adds(&b, "\r\n");
		sb_adds(&b, keys[i].key);
		sb_adds(&b, "=");
		sb_adds(&b, keys[i].val);
		sb_adds(&b, "\r\n\r\n");
	}
	for (line = old; line && *line; line = next) {
		char *k, *eq;
		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		k = str_trim(line);
		if (*k == ';' || !(eq = strchr(k, '=')))
			continue;
		for (i = 0; i < NKEYS && strncasecmp(k, keys[i].key, strlen(keys[i].key)); i++)
			;
		if (i < NKEYS && k + strlen(keys[i].key) == eq)
			continue;
		sb_adds(&b, k);
		sb_adds(&b, "\r\n\r\n");
	}
	free(old);
	if (b.s)
		pf_save(inf, b.s, b.len);
	sb_free(&b);
}

static short mem_told;		/* it ran out of memory this boot: MEMKB= written for the next */

/* the memory to take at the next boot (MEMKB=): the most it used, with
   room for a connection (accmem.c) */
static void mem_note(void)
{
	long kb;
	if (!is_acc || !acc_heap_bytes || mem_told)
		return;
	kb = acc_mem_kb(32L * 1024, 16L * 1024);
	if (!acc_mem_moved(kb, atol(memkb_v)))
		return;
	snprintf(memkb_v, sizeof(memkb_v), "%ld", kb);
	inf_write();
}

/* its memory ran out (accounts EMAIL.INF has that didn't fit, or a check
   that failed for it): said, and four times as much at the next start,
   which then measures it down (the guidelines, 2.1) */
static void mem_short(void)
{
	if (!is_acc || mem_told)
		return;
	mem_told = 1;
	snprintf(memkb_v, sizeof(memkb_v), "%ld", acc_mem_full_kb());
	inf_write();
	alert(1, "[3][EMail check ran out of memory.|It takes more when the|Atari starts again.][ OK ]");
}

/* the [account] lines in EMAIL.INF */
static short inf_accounts(const char *path)
{
	char *buf = pf_load(path, 0), *p;
	short n = 0;
	if (!buf)
		return 0;
	for (p = buf; (p = strstr(p, "\n[")) != 0; p++)
		n += !strncasecmp(p + 1, "[account]", 9);
	n += !strncasecmp(buf, "[account]", 9);
	free(buf);
	return n;
}

/* ---------------- EMail's settings ---------------- */

static void prg_path(char *out, int size)
{
	path_join(out, size, folder, "EMAIL.PRG");
}

/* EMAIL.INF read again when it changed: EMail's accounts and its timer */
static int settings(void)
{
	char path[220];
	long size;
	unsigned long stamp;
	if (!folder[0])
		return 0;
	path_join(path, sizeof(path), folder, "EMAIL.INF");
	stamp = pf_info(path, &size);
	if (!stamp)
		return 0;
	if (stamp != inf_stamp || size != inf_size) {
		inf_stamp = stamp;
		inf_size = size;
		newmail_load(folder);
		if (is_acc && naccts < MAXACCT && naccts < inf_accounts(path))
			mem_short();	/* an account that didn't fit */
		memset(nm, 0, sizeof(nm));
		dlg_3d_on = opt.dialogs3d;	/* EMail's 3D setting for its alerts too (5.6) */
		path_join(tls_ovl_path, sizeof(tls_ovl_path), folder, "TLS.OVL");
		falcon_dsp_init();
		/* EMail's protocol log on (LOG=1): the accessory's own beside
		   its INF, EMAILCHK.LOG, written at the end of each check */
		if (opt.log) {
			str_copy(conn_logfile, inf, sizeof(conn_logfile));
			strcpy(conn_logfile + strlen(conn_logfile) - 3, "LOG");
		} else {
			conn_logfile[0] = 0;
		}
	}
	return 1;
}

/* the folder, from EMAIL.PRG chosen in the file selector */
static int choose_folder(void)
{
	char path[200], name[20] = "EMAIL.PRG", *e;
	short button = 0;
	if (alert(1, "[2][EMail check needs to know|where EMail is.|Choose EMAIL.PRG.][Choose|Cancel]") != 1)
		return 0;
	snprintf(path, sizeof(path), "%c:\\*.PRG", 'A' + boot_drive());
	if (!fsel_exinput(path, name, &button, "Where is EMail (EMAIL.PRG)?") || !button || !name[0])
		return 0;
	if ((e = strrchr(path, '\\')) != 0)
		*e = 0;
	{
		char inf_path[220];
		path_join(inf_path, sizeof(inf_path), path, "EMAIL.INF");
		if (!pf_exists(inf_path)) {
			alert(1, "[1][There's no EMAIL.INF in|%s.|Start EMail there once to|set up your accounts.][ OK ]",
			      path);
			return 0;
		}
	}
	str_copy(folder, path, sizeof(folder));
	inf_stamp = 0;
	inf_write();
	return 1;
}

/* ---------------- starting EMail ---------------- */

/* On plain TOS an accessory can't start a program while the desktop is
 * in front: TOS's and EmuTOS's desktops leave a shel_write from an
 * accessory waiting, and have no way to be asked to leave (tried on
 * TOS 4.04 and EmuTOS 1.4, 24 Oct 2026). So EMail is offered only where
 * something can start it: an AES that runs programs side by side
 * (MagiC, MiNT), or a desktop that speaks the AV protocol (Thing,
 * TeraDesk, Gemini, jinnee), which takes AV_STARTPROG. Elsewhere the
 * box only tells (Erez, 24 Oct 2026). */
#define AV_STARTPROG	0x4722

static int multitasking(void)
{
	return get_cookie(0x4D616758L) != 0 ||				/* MagX */
	       (get_cookie(0x4D694E54L) != 0 && aes_version() >= 0x0400);	/* MiNT */
}

/* the AV desktop's id, -1 for none */
static short av_desktop(void)
{
	static const char *const names[] = { "THING   ", "TERADESK", "GEMINI  ", "JINNEE  " };
	short i, id;
	for (i = 0; i < (short)(sizeof(names) / sizeof(names[0])); i++)
		if ((id = appl_find(names[i])) >= 0 && id != ap_id)
			return id;
	return -1;
}

static int can_start(void)
{
	return multitasking() || av_desktop() >= 0;
}

/* EMAIL.PRG, in its folder (EMail keeps its files where it starts) */
static void start_email(void)
{
	/* the AV desktop reads them after we have gone back to our event
	   loop: kept here (an accessory's memory stays) */
	static char prg[220], tail[2];
	short desk = multitasking() ? -1 : av_desktop();
	prg_path(prg, sizeof(prg));
	if (!pf_exists(prg)) {
		alert(1, "[1][EMAIL.PRG isn't in|%s.][ OK ]", folder);
		return;
	}
	if (folder[0] && folder[1] == ':') {
		Dsetdrv((folder[0] & ~0x20) - 'A');
		Dsetpath(folder[2] ? folder + 2 : "\\");
	}
	if (desk >= 0) {
		short m[8];
		m[0] = AV_STARTPROG;
		m[1] = ap_id;
		m[2] = 0;
		m[3] = (short)((unsigned long)prg >> 16);
		m[4] = (short)(unsigned long)prg;
		m[5] = (short)((unsigned long)tail >> 16);
		m[6] = (short)(unsigned long)tail;
		m[7] = 0;
		appl_write(desk, 16, m);
	} else if (!shel_write(1, 1, 1, prg, tail)) {
		alert(1, "[1][Couldn't start EMail.][ OK ]");
		return;
	}
	pf_debug("EMAILCHK starts EMail");
}

/* ---------------- the check ---------------- */

static int may_check(void)
{
	return folder[0] && opt.check > 0 && !opt.offline && naccts > 0 &&
	       (!checked || pf_ms() - last_check >= (unsigned long)opt.check * 60000UL);
}

/* "3 new messages in the Inbox." or "3 new messages: Work 2, Home 1." */
static void found_text(char *out, int size, long total)
{
	short i, with = 0;
	for (i = 0; i < naccts; i++)
		with += nm[i].count > 0;
	snprintf(out, size, "%ld new message%s%s", total, total == 1 ? "" : "s",
		 with > 1 ? ":" : naccts > 1 ? " in" : " in the Inbox.");
	for (i = 0; i < naccts; i++) {
		size_t l = strlen(out);
		if (nm[i].count <= 0)
			continue;
		if (with > 1)
			snprintf(out + l, size - l, " %.16s %ld%s", accts[i]->name, nm[i].count,
				 --with ? "," : ".");
		else if (naccts > 1)
			snprintf(out + l, size - l, " %.20s's Inbox.", accts[i]->name);
	}
}

/* a Direct connection's room in the accessory's memory: TLS.OVL loaded,
   or room for it. Short of it in TT RAM, the next boot takes more
   (MEMKB=); in ST RAM only HEAPKB= does (acc_memory) */
static int direct_room(void)
{
	long kb;
	if (!is_acc || !opt.falcon || !falcon_usable || tls_ovl_loaded() || !acc_heap_bytes ||
	    acc_heap_bytes - heap_use >= DIRECT_ROOM)
		return 1;
	kb = (heap_use + DIRECT_ROOM + 32L * 1024) / 1024;
	if (acc_heap_tt && kb > atol(memkb_v)) {
		snprintf(memkb_v, sizeof(memkb_v), "%ld", kb);
		inf_write();
	}
	return 0;
}

/* every account's Inbox; asked: the user wants the answer whatever it is */
static void check(int asked)
{
	char err[160], text[200];
	long total = 0;
	short i, fresh = 0, ok = 0, naccts_skip = 0;
	if (!settings())
		return;
	if (asked)
		busy_bee(1);
	conn_log_hold(1);		/* the log written at the end, or when its 4 KB fill */
	last_err[0] = 0;
	if (!direct_room()) {
		/* a Direct connection with no room for TLS.OVL: said from the
		   Desk menu, not checked */
		str_copy(last_err, "no memory for a Direct connection here: HEAPKB= in EMAILCHK.INF",
			 sizeof(last_err));
		naccts_skip = 1;
	}
	for (i = 0; i < naccts && !naccts_skip; i++) {
		conn_stop = 0;
		if (newmail_count(accts[i], &nm[i], err, sizeof(err))) {
			ok++;
			total += nm[i].count;
			fresh |= newmail_fresh(&nm[i]);
		} else if (!last_err[0]) {
			snprintf(last_err, sizeof(last_err), "%.24s: %s", accts[i]->name, err);
		}
	}
	if (asked)
		busy_bee(0);
	conn_log_hold(0);
	checked = 1;
	last_check = pf_ms();
	last_new = total;
	if (strstr(last_err, "memory") && !naccts_skip)
		mem_short();
	else if (!naccts_skip)
		mem_note();		/* (skipped: the room it wrote for next time stays) */
	{
		/* for the rigs: the emulator's console */
		char note[200];
		snprintf(note, sizeof(note), "EMAILCHK checked %d of %d account%s: %ld new%s%s%s", ok, naccts,
			 naccts == 1 ? "" : "s", total, fresh ? ", fresh" : "", last_err[0] ? "; " : "", last_err);
		pf_debug(note);
		snprintf(note, sizeof(note), "EMAILCHK stack: %ld bytes used at most, heap %ld KB of %ld",
			 stack_used(), heap_peak / 1024, acc_heap_bytes / 1024);
		pf_debug(note);
	}
	if (!total && !asked)
		return;
	if (!total) {
		if (last_err[0])
			alert(1, "[3][%s][ OK ]", last_err);
		else
			alert(1, "[1][No new mail.][ OK ]");
		return;
	}
	if (!fresh && !asked)
		return;
	for (i = 0; i < naccts; i++)
		newmail_told(&nm[i]);
	found_text(text, sizeof(text), total);
	if (!can_start())
		alert(1, "[1][%s|Start EMail to read %s.][ OK ]", text, total == 1 ? "it" : "them");
	else if (alert(1, "[1][%s|Open EMail?][Open|Later]", text) == 1)
		start_email();
}

/* ---------------- the Desk menu ---------------- */

static void about(void)
{
	static char line1[80], line2[60];
	ABOUT a = { "EMail check", mail_version, "New mail for EMail while it isn't running", { line1, line2, 0 },
		    { 0, 0, 0 }, icon_bits, icon16_px, icon16_rgb, icon256_px, icon256_rgb };
	char avail[40];
	snprintf(line1, sizeof(line1), "%s  \xF9  %s", strcmp(net_stack(), "none") ? net_stack() : "no network",
		 !opt.falcon ? "Gateway" : tls_ovl_loaded() ? "Direct (TLS.OVL)" : "Direct connection");
	pf_memory(line2, sizeof(line2), avail, sizeof(avail));
	about_show(&a);
}

static void opened(void)
{
	char when[160];
	short r;
	pf_debug("EMAILCHK opened");
	bg_opened();
	if (!folder[0] && !choose_folder())
		return;
	if (!settings()) {
		alert(1, "[1][EMAIL.INF isn't in|%s.][ OK ]", folder);
		folder[0] = 0;
		return;
	}
	if (!naccts) {
		alert(1, "[1][EMail has no accounts yet:|set one up in EMail first.][ OK ]");
		return;
	}
	if (opt.offline)
		snprintf(when, sizeof(when), "EMail is set to work offline.");
	else if (opt.check <= 0)
		snprintf(when, sizeof(when), "EMail's timed check is off.");
	else if (!checked)
		snprintf(when, sizeof(when), "It checks every %d minute%s.", opt.check, opt.check == 1 ? "" : "s");
	else {
		unsigned long ago = (pf_ms() - last_check) / 60000UL;
		char found[80], minutes[24];
		if (last_new)
			snprintf(found, sizeof(found), "%ld new message%s", last_new, last_new == 1 ? "" : "s");
		else	/* what went wrong, as the check said it */
			snprintf(found, sizeof(found), "%.70s", last_err[0] ? last_err : "nothing new");
		if (ago)
			snprintf(minutes, sizeof(minutes), "%lu minute%s", ago, ago == 1 ? "" : "s");
		snprintf(when, sizeof(when), "Checked %s ago: %s.", ago ? minutes : "under a minute", found);
	}
	r = alert(1, "[1][EMail check|%s][Check now|About|OK]", when);
	if (r == 1)
		check(1);
	else if (r == 2)
		about();
}

/* ---------------- start ---------------- */


static int vwk_open(void)
{
	short work_out[57], d;
	vdi_h = graf_handle(&gl_wchar, &gl_hchar, &d, &d);
	vdi_h = v_opnvwk_(vdi_h, work_out);
	if (vdi_h <= 0)
		return 0;
	scr_w = work_out[0] + 1;
	scr_h = work_out[1] + 1;
	vq_extnd(vdi_h, 1, work_out);
	planes = work_out[4];
	cw = gl_wchar;
	ch = gl_hchar;
	vst_alignment(vdi_h, 0, 5);
	return 1;
}

/* memory, once, before the first event call (accmem.c): 64 KB, a
   check through the gateway (it measured 80 KB with its margin, MEMKB=).
   A Direct connection needs about 230 KB more (TLS.OVL, ROOTS.DAT,
   BearSSL's buffers: MEMKB=313): taken at the first boot only in TT RAM,
   where there is plenty. In ST RAM it would be taken from the program in
   front (on a 4 MB Falcon with NVDI and the family's accessories, EMail
   then couldn't open its screen: the hub's acc_all.py, 24 Oct 2026), so
   there only HEAPKB= gives it */
static void acc_memory(void)
{
	acc_heap_take(FIRST_HEAP, FIRST_HEAP + DIRECT_ROOM, atol(heapkb_v), atol(memkb_v));
	{
		void *p = Mxalloc(sizeof(st_fallback), 0);	/* ST RAM, for the icon */
		if ((long)p > 0)
			st_buf = p;
	}
}

int main(void)
{
	short msg[8];
	AEVENT ev;
	is_acc = _is_acc;
	stack_paint();
	ap_id = appl_init();
	if (ap_id < 0)
		return 1;
	if (is_acc)
		menu_id = menu_register(ap_id, "  EMail check");
	inf[0] = (char)('A' + boot_drive());
	inf_read();
	vwk_open();
	wind_get(0, WF_WORKXYWH, &desk_x, &desk_y, &desk_w, &desk_h);
	falcon_usable = get_cookie(0x5F435055L) >= 20;	/* _CPU: TLS.OVL needs a 68020 */
	conn_bufsize = 4096;		/* commands and flags: small replies */
	conn_log_hold_bytes = 0;	/* the log's 4 KB, not 16 from the heap */
	twin_init("EMAIL");
	av_init("EMAILCHK");
	net_init();
	pf_idle = busy_idle;
	tls_file_info = pf_info;
	if (!is_acc) {
		/* as a program: the same as from the Desk menu, once */
		opened();
		v_clsvwk(vdi_h);
		appl_exit();
		return 0;
	}
	acc_memory();
	bg_init(bg_parse(start_v));

	for (;;) {
		short which, pending, running;
		pending = (short)(folder[0] && (reread || may_check()));
		which = evnt_multi_(MU_MESAG | MU_TIMER, 0, 0, 0, bg_timer(pending, 1000), msg, &ev);
		bg_tick();
		if (which & MU_MESAG) {
			const char *va;
			switch (msg[0]) {
			case AC_OPEN:
				if (msg[4] == menu_id)
					opened();
				break;
			case AC_CLOSE:
				twin_reset();		/* another program in front */
				break;
			default:
				if (av_message(msg, &va) == AV_START)
					opened();	/* VA_START: as from the Desk menu */
				break;
			}
		}
		/* EMail checks while it runs; once it has ended, its own check
		   is the last, and the time counts from then */
		running = twin_prg() >= 0;
		if (running)
			prg_ran = 1;
		else if (prg_ran) {
			prg_ran = 0;
			last_check = pf_ms();
			checked = 1;
			reread = 1;		/* its settings may have changed */
		}
		if (!running && folder[0] && bg_may_work()) {
			if (reread) {
				reread = 0;
				settings();
			}
			if (may_check())
				check(0);
		}
	}
}
