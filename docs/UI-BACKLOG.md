# EMail's interface backlog (Erez's decisions of 9 Nov 2026)

The family's interface rules are in AERA-Atari's `standards/ui` (main a261597: DECISIONS-2026-11-09.md, MENUS.md,
CONTROLS.md, WINDOWS.md, DEVIATIONS.md). This list is where EMail still differs from them, as the hub sent it
on 9 Nov 2026.

EMail is in maintenance mode, so this is a backlog, not work to start: an item goes into a release EMail makes
anyway (a fault fix, or one Erez asked for) without growing that release's risk. The S items can go in freely;
the H and M items only when that release is otherwise untouched. Every GEM release still passes the speed rig.
When an item is done, send the hub one line with its commit, and mark it done here.

S = small (under an hour), H = half a day, M = more. [hub] = waits for shared code in `common/`; the hub says
when it is there.

| # | Item | Rows | Size | State |
|---|---|---|---|---|
| 1 | Mark as unread leaves ⇧^U for **^J** (Erez, 9 Nov 2026): a key typed while EMail is busy can lose its Shift, and ^U closes the window. The bar reads "Mark as unread  ^J", the list's right-click the same | D1, MD13 | S | approved |
| 2 | Pop-up words and keys are the bar's, in sentence case: Mark as unread shows the bar's key (after 1), Reply to all shows ⇧^R (`ui/list.c`) | D14, MD19 | S | |
| 3 | Translate in its own section of Options, with a line above and below (`ui/main.c` m_opts) | D5, MD11 | S | |
| 4 | `Write a log` in the menu and in Settings > Connection | D8, MD3 | S | |
| 5 | `Direct connection` without `(TLS)` in Options | D9, MD6 | S | |
| 6 | Out-of-range numbers in Settings refused by an alert, not clamped (Erez approved the words, 9 Nov 2026): `[1][Messages to load at a time:\|type a number from 20 to 1000.][ OK ]` and `[1][Wrap lines at column:\|type a number from 40 to 78.][ OK ]`; Settings stays on the page with the field ready, nothing saved until in range | D30, C21 | S | approved |
| 7 | POP3 "leave mail on the server": the toggle button becomes a bitmap tick (`d_check` to `d_tick`); the pick list's rows nested as EFTP's `d_nest` | D23, C5, C10 | S each | |
| 8 | A black title band in every dialog (the hub does the Font and Languages dialogs) | D22, C4 | S to H | |
| 9 | Pop-up labels drop their own padding once `popup()` adds the margin | D15, MD18 | S | [hub] |
| 10 | "Translate messages into" pop-up opens over its button with the current choice ticked | D27, C8 | S | [hub] |
| 11 | Insert opens the context menu (list, reader, folders, editor); where the box opens is still open | D17, MD20 | H | |
| 12 | The editor's toolbar buttons act on the release over the button | D32, W1 | H | |
| 13 | Clipboard words: `Copied to the clipboard.` everywhere (the link copy still says "The link is on the clipboard.") | D50, W17 | S | |
| 14 | Shift+Up/Down pages `rows - 1` | W5 | S | [hub: key_shift] |
| 15 | Drag threshold 4 pixels as EFTP's (`ui/list.c`); click timing through `click.h` `press_track` and `press_double` | W21, W24 | S; H | |
| 16 | 16-colour selection (0,0,667) from the family's `theme.c` | D42, W16 | S | [hub] |
| 17 | INF keys `WINDOW=x,y,w,h`, `PANES=`, `FONT=name,points`: the old key read once, the new one wins | D43, D44, W18 | H | |
| 18 | ⇧^S hides the folder pane. New work; its menu place is still open: ask the hub first | D45, W20 | H | |
| 19 | Keyboard reach in dialogs (D25, C6) H; EMail's own `form_loop` onto `dlg_do` (C11) H; the password-copy flag (C12); window arrows to the pane under the pointer (D37): all [hub]. `-DDLG_CLICK` (C16) S | D25, C6, C11, C12, C16, D37 | | |

Still open, not to start: Find's words and place (D4, MD10).
Nothing to do: Accounts... is in Options already (D13); `Open|Later` fits (W25).
