/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * ui.h - EMail's GEM interface: four windows like the GFA Troll
 * (folders, message list, message, editor), a menu bar and dialogs.
 */
#ifndef UI_H
#define UI_H

#include "tos.h"
#include "gem.h"
#include "../src/store.h"
#include "../src/mime.h"

typedef struct { short x, y, w, h; } GRECT;

typedef struct WIN WIN;
struct WIN {
	short pane;			/* 1: a pane of the main window */
	GRECT box;			/* pane: its whole area, header and scroll bar included */
	short h;			/* AES handle, -1 when closed */
	short kind;
	char title[100];
	char info[120];
	GRECT work;
	GRECT place;			/* outer rectangle to open at */
	long top;			/* first visible line */
	long total;			/* lines of content */
	short line_h;			/* pixels per line */
	short head_h;			/* fixed pixels above the scrolled lines */
	void (*draw)(WIN *w, GRECT *clip);
	void (*click)(WIN *w, short mx, short my, short clicks, short kstate);
	int  (*key)(WIN *w, short kstate, short key);	/* 1 = handled */
	void (*rclick)(WIN *w, short mx, short my);	/* right button: context menu */
	void (*closed)(WIN *w);
	void (*resized)(WIN *w);
	void (*scrolled)(WIN *w);
	short iconified;		/* a window of its own shown as EMail's icon (WM_ICONIFY) */
};

/* screen and font */
extern short vdi_h, cw, ch, scr_w, scr_h, desk_x, desk_y, desk_w, desk_h, planes;
extern short gl_wchar, gl_hchar;	/* the AES's system font cell (menus, dialogs) */
extern short ap_id;

/* EContact (main.c, common/ui/people_ask.c): names for addresses, and
   people picked in its window */
#include "people_ask.h"
void ui_modal_message(const short *msg);	/* a modal loop's MU_MESAG */
void editor_names_came(void);
void editor_picked(const PERSON *list, int n);
void reader_names_came(void);
int  reader_tip(short mx, short my, char *out, int size);	/* a name's address: 1 */
void list_names_came(void);
/* the name to show for an address: EContact's, or the address itself */
const char *person_shown(const char *addr);

/* windows (win.c) */
extern WIN w_folders, w_list, w_reader, w_editor;
void win_open(WIN *w);
void win_close(WIN *w);
void win_top(WIN *w);
int  win_is_top(WIN *w);
WIN *win_find(short handle);
void win_title(WIN *w, const char *title);
void win_info(WIN *w, const char *info);
void win_redraw(WIN *w, GRECT *area);		/* area NULL = everything */
void win_redraw_lines(WIN *w, long first, long n);
void win_sliders(WIN *w);
void win_scroll_to(WIN *w, long top);
void win_ensure_visible(WIN *w, long line);
long win_rows(WIN *w);				/* whole lines that fit */
void win_message(short *msg);			/* WM_* handling */
WIN *win_topmost(void);
int win_icon_on_top(void);		/* the top window is iconified */
int  win_mouse(short mx, short my, short button, short clicks, short kstate);
void win_hover(short mx, short my);
void win_focus(WIN *w);
void win_relayout(void);			/* after a font change */
void win_redraw_all(void);
void win_redraw_area(GRECT *area);	/* now, under a dialog that closed */
extern short main_h;
extern short main_iconified;		/* shown as the icon */
void icon_window(short h, GRECT *area);	/* an iconified window: the icon, and unread mail */
void main_open(void);
void main_close(void);
void main_status(const char *s);

/* drawing (draw.c) */
#define TX_BOLD     1
#define TX_INVERSE  2
#define TX_RIGHT    4	/* align right-to-left text to the right edge */
#define TX_LIGHT    8
#define TX_LTR     16	/* left-to-right paragraph even when it starts in Hebrew */
#define TX_DIM     TX_LIGHT	/* what can't be used: the shared popup's disabled items */
void fill(GRECT *r, short color);
extern short text_pen;		/* the colour of text (theme.h pens); set, draw, set back to 1 */
/* text in a colour: text_at with text_pen set for the one call */
#define text_pen_at(p, x, y, s, n, cols, fl) \
	do { text_pen = (p); text_at(x, y, s, n, cols, fl); text_pen = 1; } while (0)
void clip_on(GRECT *r);
void clip_off(void);
void *st_ram(long n);		/* a bitmap's room in ST RAM, for the blitter (n <= 512) */
/* draw up to cols characters of Atari text at x,y (logical order in) */
void text_at(short x, short y, const char *s, long n, short cols, short flags);
void hline(short x1, short x2, short y);
void frame(GRECT *r, short color);
void line_col(short x1, short y1, short x2, short y2, short color);
/* word wrap: start of each line of text into breaks[]; returns count */
long wrap_text(const char *s, long n, short width, long *breaks, long max);
short line_rtl(const char *s, long n);
/* move Hebrew letters to where the font has them (HEB_*), in place */
void heb_font_map(char *s, long n, short where);

/* views */
void folders_build(void);
void folders_select(ACCOUNT *a, FINFO *fi);
extern ACCOUNT *cur_acct;
extern FINFO *cur_finfo;
extern FOLDER *cur_folder;

void list_load(void);			/* (re)read cur_folder into the list */
void list_refresh(void);
void list_titles(void);
HDR *list_current(void);
/* several messages selected */
long list_marked(void);				/* how many (0: just the current one) */
unsigned long *list_targets(long *n);	/* their UIDs, malloc'ed; or the current one */
void list_unmark(void);
void list_select_all(void);
void reader_selection(long n);			/* the reader says how many are selected */
void list_select_uid(unsigned long uid);
void list_after_remove(void);

void reader_show(HDR *h);		/* load and show a message */
void reader_clear(void);
extern MSG *cur_msg;
extern char *cur_raw;
extern long cur_rawlen;
extern unsigned long cur_uid;
void reader_save_attachment(short i);

void editor_open(ACCOUNT *a, char *text, const char *in_reply_to, const char *references);
int  editor_dirty(void);
void editor_send(int now);
void editor_attach(void);
void editor_attach_file(const char *path);
void editor_bar_redraw(void);			/* its buttons */
int  editor_tip(short mx, short my, char *out, int size);	/* a button's tooltip: 1 */
void editor_clip(int what);			/* CLIP_COPY, CLIP_CUT, CLIP_PASTE */
void editor_insert_text(const char *s);		/* at the cursor */
void editor_insert_body(const char *s);		/* at the start of the message's text */
void editor_insert_address(void);
char *dlg_pick_address(void);

/* dialogs */
int  dlg_account(ACCOUNT *a);		/* 1 = OK, -1 = delete it */
int  dlg_settings(void);
int  dlg_ask(const char *title, const char *label, char *buf, short len);
FINFO *dlg_pick_folder(ACCOUNT *a, const char *title);
ACCOUNT *dlg_pick_account(const char *title);
void dlg_about(void);

/* the dialog builder (dialogs.c): sizes in characters */
void   d_begin(short w, short h);
short  d_add(short type, short flags, short state, long spec, short x, short y, short w, short h);
short  d_text(short x, short y, const char *s);
short  d_button(short x, short y, short w, const char *s, short flags);
short  d_check(short x, short y, const char *s, short on);
void   d_end(void);
OBJECT *d_tree(void);
#include "alert.h"		/* alert(): common/ui */
void dlg_keys(void);			/* Help: the keys */

/* menu commands (main.c) */
enum {
	C_NONE, C_ABOUT, C_HELP, C_KEYS, C_REFETCH, C_GETALL,
	C_NEW, C_CHECK, C_SENDQ, C_SAVEATT, C_CLOSE, C_QUIT,
	C_CUT, C_COPY, C_PASTE, C_SELALL,
	C_REPLY, C_REPLYALL, C_FORWARD, C_UNREAD, C_MARKREAD, C_FLAG, C_MOVE, C_DELETE,
	C_SEND, C_SAVEOUT, C_ATTACH, C_ABOOK,
	C_FOLDERS, C_REFRESH, C_NEWFOLDER, C_RENFOLDER, C_DELFOLDER,
	C_ACCOUNTS, C_SETTINGS, C_FONT, C_OFFLINE, C_FALCON, C_HEBREW, C_LOG
};
void ui_command(short cmd);
void cmd_new_folder(ACCOUNT *a);
void cmd_rename_folder(ACCOUNT *a, FINFO *fi);
void cmd_delete_folder(ACCOUNT *a, FINFO *fi);
void cmd_download_all(ACCOUNT *a, FINFO *fi);
void move_messages(unsigned long *u, long n, FINFO *dest);	/* frees u */
int  folder_takes(ACCOUNT *a, FINFO *f);	/* the open folder's messages can go there */
FINFO *folders_drop_target(short mx, short my, GRECT *row);
void cmd_edit_account(ACCOUNT *a);
void cmd_refresh_folders(ACCOUNT *a);
int  falcon_mode_set(int on);		/* 1 if it is now as asked */
extern short dlg_falcon;		/* the Settings dialog's Falcon mode box */
void falcon_dsp_init(void);		/* RSA on the DSP in Falcon mode (dsp.c) */
const char *falcon_dsp_state(void);	/* "DSP" / "68030" for the About box */

/* right-click menus (common/ui/popup.c): the chosen item, or -1 */
#include "popup.h"

/* font.c */
void font_apply(void);		/* opt.font_id/pt -> cw, ch */
void font_sys_push(void);	/* the system font, for a popup or the About box */
void font_sys_pop(void);	/* back to the window's font */
void font_reselect(void);	/* the text font again, after drawing in another */
/* how the text font gets its Hebrew letters */
#define HM_MAP     0		/* where opt.hebfont says (system and bitmap fonts) */
#define HM_UNICODE 1		/* a vector font's own, found by Unicode (NVDI) */
#define HM_SYSTEM  2		/* a vector font without them: the system font's */
extern short heb_mode;
extern unsigned short heb_idx[27];
void font_menu(void);

/* main.c */
void status(const char *msg);
/* the same, made with printf: what EMail is doing ("Moving to Archive...") */
void statusf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void busy(int on);
void cmd_check_all(void);
void refind_current(const char *server);	/* the open folder after a new folder list */
extern int reader_missing;	/* the reader shows a note: the message isn't here */
void set_hebrew_kbd(short on);
extern short hebrew_kbd;
unsigned char key_char(short kstate, short key);	/* honours the Hebrew layout */
void menu_update(void);

#define K_ALT 0x08
#define KEY_SCAN(k)  (((unsigned short)(k)) >> 8)
#define KEY_ASCII(k) ((unsigned char)((k) & 0xff))

#endif
