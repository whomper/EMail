# EMail user guide

EMail is an e-mail program for the Atari ST, STE, Mega ST/STE, TT and
Falcon. It reads mail from IMAP or POP3 servers, sends it over SMTP,
keeps a copy of your IMAP folders on disk so you can read offline, and
reads and writes Hebrew.

## What you need

- An Atari with TOS 1.04 or newer (TOS 4 on the Falcon), EmuTOS, MagiC or
  MiNT, and a screen of at least 640×200 (ST medium); 640×400 (ST high)
  or the Falcon's 640×480 show three windows side by side
- 1 MB of memory or more: EMail itself takes about 424 KB, the ST
  edition (below) about 303 KB, and more memory holds bigger folders. With EContact's accessory loaded, 2 MB or
  more: on a 1 MB ST the two don't fit together, so there use EContact as
  a program (`ECONTACT.PRG`), between EMail's runs
- A TCP/IP stack:
  - **STinG** on plain TOS (or STiK), or
  - **MiNTnet** under FreeMiNT (with or without GlueSTiK), or MagiC-Net

  With STinG, set the Delay in `STING.CPX` to suit the machine: 10 on a
  Falcon or TT, 25 to 50 on an ST, as STinG's manual says. STinG looks at
  the network card that often, so it sets the speed of every STinG
  program, EMail's checks and downloads among them.
- For providers that need encryption (nearly all), one of:
  - a Raspberry Pi or other Linux computer on your network as a gateway
    (any Atari), see [The Raspberry Pi gateway](#the-raspberry-pi-gateway), or
  - **Falcon mode**: EMail does the encryption itself (a Falcon, or a TT,
    with 4 MB of memory or more), see [Falcon mode](#falcon-mode)

## Installing

The files are in the latest release on EMail's GitHub page
(github.com/whomper/EMail/releases), each on its own and all together in
`EMAIL-x.y.zip`. Copy `EMAIL.PRG` to any folder on any drive: nothing
has to be in a particular place.

On an ST, take `EMAILST.PRG` instead, **the ST edition**: the same
program without Falcon mode (an ST is too slow for the encryption, and
reaches secure servers through the Pi gateway), about 120 KB smaller in
memory, which leaves a 1 MB ST more room for your mail. It reads the same
`EMAIL.INF` and folders, so you can move between the two; it leaves the
Falcon mode setting as it found it for `EMAIL.PRG`. Use one of them at a
time. EMail keeps everything in the folder it
is started from, which the desktop makes its own folder, so a folder of
its own keeps it all together. Here it is `C:\EMAIL\`:

```
C:\EMAIL\EMAIL.PRG      the program
C:\EMAIL\EMAIL.INF      settings and accounts (each setting has a note above it)
C:\EMAIL\EMAIL.KEY      the key the passwords in EMAIL.INF are encrypted with
C:\EMAIL\ADDRESS.TXT   the addresses you wrote to (Ctrl+B without EContact)
C:\EMAIL\EMAIL.LOG      protocol log, when switched on
C:\EMAIL\EMAIL.FNT      which fonts are fixed-width (see Options > Font)
C:\EMAIL\CRASH.LOG      what happened, if EMail ever crashes
C:\EMAIL\CACERT.PEM    root certificates, for Falcon mode only
C:\EMAIL\ROOTS.DAT     the same, decoded ahead of time (Falcon mode only)
C:\EMAIL\EMAIL.HYP     this guide, for EHypView or ST-Guide (the Help key opens it)
C:\EMAIL\EMAIL.REF     its page names, for ST-Guide's reference list (optional)
C:\EMAIL\EMAIL\         your mail: one folder per account, one per mailbox,
                        each with its messages in one file (MESSAGES.DAT)
C:\EMAIL\EMAIL\SEED.DAT  randomness kept for Falcon mode's encryption
```

Start it. The first time, EMail asks for your account.

This guide is also in `EMAIL.HYP`, a hypertext: with EHypView, ST-Guide
or HypView running (as an accessory, or as a program under MultiTOS or
MagiC), the Help key or **Help > EMail help** opens it, from next to
`EMAIL.PRG` or from the viewer's own help folder. Without a viewer, Help
says how to get one and offers the list of keys instead; **Help > Keys**
shows that list at any time.

## Updating

Copy the new release's files over the old ones: `EMAIL.PRG`, `EMAIL.HYP`
and `EMAIL.REF`, and for Falcon mode `CACERT.PEM` and `ROOTS.DAT`.
`EMAIL.INF`, `EMAIL.KEY` and your mail stay as they are. Keep EMAIL.KEY
with EMAIL.INF: without it EMail asks for the passwords again. Coming
from a version before 1.37, type your signature once more, in Options >
Settings > Signature. On the Pi, `./update-pi.sh` in the AERA-Gateway
folder updates the gateway.

## The Raspberry Pi gateway

Most providers want an encrypted connection, which an ST is too slow
to make. A Raspberry Pi (or another Linux computer) on your home network
makes it for EMail: EMail talks plainly to the Pi, and the Pi talks
TLS to your provider. Its software is AERA-Gateway
(github.com/whomper/AERA-Gateway). On the Pi, from a fresh install:

```
sudo apt install -y git                  (Raspberry Pi OS Lite only)
git clone https://github.com/whomper/AERA-Gateway
cd AERA-Gateway
./update-pi.sh
sudo gateway/aera-gateway install mail IMAP_SERVER SMTP_SERVER[:587] [POP3_SERVER] --allow ATARI_IP
```

Give your provider's servers, the POP3 one only if you use POP3, and
your Atari's IP address, for example:

```
sudo gateway/aera-gateway install mail imap.gmail.com smtp.gmail.com pop.gmail.com --allow 192.168.1.20
```

iCloud is `imap.mail.me.com smtp.mail.me.com:587`; AERA-Gateway's
README has the others. `--allow` lets only your Atari in; without it,
any computer on your home network may use the gateway. The installer
prints the Pi's address and the ports to give EMail.

- To update the gateway: `cd ~/AERA-Gateway && ./update-pi.sh`
- To check it: `gateway/aera-gateway status`

In EMail, in **Options > Accounts**, the server is the Pi's address
(PI_IP), with port 143 for IMAP (110 for POP3) and 587 for sending
(SMTP); the user and password are your provider's (see below).

## Setting up an account

Options > Accounts > New, or the dialog EMail shows at first start:

| Field          | What to type                                                     |
|----------------|------------------------------------------------------------------|
| Account name   | a name for the folder list, e.g. "Home"                          |
| Your name      | shown to people you write to (Hebrew is fine)                    |
| E-mail         | your address                                                     |
| Incoming mail  | IMAP (folders stay on the server) or POP3 (mail comes to the Atari) |
| Server, Port   | through the gateway: the Pi's IP and 143 (IMAP) or 110 (POP3), as for a server without TLS |
| User, Password | your login (Gmail, iCloud, Yahoo, Fastmail: an app password)     |
| POP3: leave mail on the server | keep a copy on the server after downloading    |
| Outgoing       | the Pi's IP and 587 through the gateway; leave User empty to use the incoming login, or type `-` for a server that needs none |
| Passwords      | shown as `*`; click the eye beside the field to see what you typed |

EMail holds up to eight accounts. Edit or delete one with Options >
Accounts > Edit. Passwords are kept in `EMAIL.INF` encrypted with a random
key EMail makes the first time, in `EMAIL.KEY`: someone who reads or copies
EMAIL.INF alone can't read them, but whoever has both files can, so keep
EMAIL.KEY to yourself. Lose EMAIL.KEY (or start with a new one) and EMail asks
you to type the passwords again.

## Falcon mode

EMail works in one of two ways, chosen with **Options > Falcon mode
(TLS)** or "Falcon mode: TLS on this Atari" in Options > Settings >
Connection:

- **Off** (the default, any Atari): EMail talks plainly to the Raspberry
  Pi gateway, and the Pi encrypts the connection to your provider.
- **On**: EMail encrypts on the Atari itself (TLS 1.2) and talks straight
  to your provider; no Pi is needed. On a Falcon the DSP56001 checks the
  servers' certificates (about 0.12 s each instead of 1.3 s or more on
  the 68030). "Use the DSP for the signatures" in Settings > Connection
  switches that off, leaving all of it to the 68030 (slower, but a way to rule the DSP out if something goes wrong).

Falcon mode needs:

- a 68020 or better: a Falcon, or a TT (without the DSP part); a 68000
  ST is too slow, and EMail says so
- 4 MB of memory or more (1 MB is not enough for TLS next to your mail)
- `CACERT.PEM` and `ROOTS.DAT` next to `EMAIL.PRG`, both in the
  release. CACERT.PEM holds the root certificates EMail trusts (the
  Mozilla list); ROOTS.DAT is the same list already decoded, which saves
  over a minute on the first connection. If you replace CACERT.PEM with a
  newer one (from [curl.se/docs/caextract.html](https://curl.se/docs/caextract.html)),
  EMail decodes it once on the next connection and writes a new ROOTS.DAT
- the right date, time and time zone (Options > Settings): certificates
  are only valid between two dates
- EMail in fast (TT) RAM if you have it: EMAIL.PRG asks TOS for it, and
  TLS runs twice as fast there as in ST RAM

Each account keeps two sets of servers and logins, so you can switch back
and forth without typing them again: the gateway's (used when Falcon mode
is off) and the provider's own (used when it is on). Options > Accounts >
Edit shows the set of the mode you are in, and changing a password in one
mode leaves the other alone. The first time, Falcon mode's login starts as
a copy of the gateway's. Your name and address are shared. When you switch
it on, EMail fills them in for Gmail, iCloud, Yahoo, GMX, web.de, AOL,
Fastmail and Zoho from the e-mail address, and names any account whose
servers you still have to type in. The usual ports:

| Port | What it is                                       |
|------|--------------------------------------------------|
| 993  | IMAP over TLS                                    |
| 995  | POP3 over TLS                                    |
| 465  | SMTP over TLS                                    |
| 587  | SMTP, switched to TLS with STARTTLS              |
| 143, 110 | IMAP, POP3, switched to TLS with STARTTLS    |

For iCloud that is `imap.mail.me.com` 993 and `smtp.mail.me.com` 587; for
Gmail `imap.gmail.com` 993 and `smtp.gmail.com` 465. Either way EMail
never sends a password over an unencrypted connection in Falcon mode.

Connecting takes a few seconds on a 50 MHz Falcon; after that mail comes
in at 15 to 25 KB/s (the 68030 decrypts every byte; through the Pi it is
about 37 KB/s), which is plenty for text.
About > shows how the current connection is made (Falcon/DSP, Falcon
or Gateway), and the protocol log
records the cipher and the timings.

EMail first offers the server only ChaCha20-Poly1305, which the 68030
decrypts twice as fast as AES-GCM; a server that refuses gets every
cipher on a second try (well under a second, once per session). The log
says which was used ("secure: TLS 1.2, ECDHE-RSA, ChaCha20-Poly1305").
A bigger TCP receive window in STinG's configuration (32000 rather than
10000) lets the Falcon receive while it decrypts.

## The main window

EMail's main window has three panes, like the GFA Troll it follows and
like a modern mail program:

- **Folders** (left): each account with its folders and unread counts.
  Click a folder to open it. Click an account's name to check its mail.
- **Messages** (top right): newest first. Unread messages are bold;
  `!` in the first place is flagged, `@` in the second probably has
  attachments. In
  Sent and the Outbox it shows who you wrote to. Its header shows how
  many messages are loaded and how many there are.
- **Message** (bottom right): the selected message. Click an attachment
  line (`»`) to save it with the file selector. A calendar invitation
  shows as `» Invitation:` and its title: double-click it to add it to
  ECal (see From other programs). HTML-only mail is shown
  as text. Quoted lines are coloured by how deep they are quoted (light
  grey in black and white).

**Drag the dividers** between the panes to size them; the pointer turns
into a hand over a divider. Drag the box in the bottom right corner to
size the window. Each pane has its own scroll bar. The pane
with the dark header has the keyboard: click a pane, or press Tab to
move on. The main window's own place and size, the dividers, the
editor's place and the font are all kept in `EMAIL.INF` for next time.

Click a message to read it. Up and Down move through the list; Return
moves the keyboard to the message. In the message pane Space and
Backspace page down and up. Closing the main window quits EMail.

Where the AES can iconify windows (MultiTOS, MagiC, XaAES, N.AES), the
main window and the editor have the iconify button: each shrinks to
EMail's icon, the main window with the number of unread messages in the
inboxes under it ("3 new"). Mail is still checked while it is iconified.

**Big folders.** EMail loads a folder 100 messages at a time, newest
first (Options > Settings changes how many). When there are older ones,
the last line of the list says **Load 100 more**: click it, or move onto
it with Down and press Return. Only what you load is mirrored, so a Sent
folder with thousands of messages opens as quickly as a small one.

**Right-click** anything for what you can do with it:

| On            | Menu                                                                   |
|---------------|------------------------------------------------------------------------|
| a message     | Open, Reply, Reply to all, Forward, Mark as read/unread, Flag, Move to, Download again, Delete |
| a folder      | Open, Check for new mail, Mark all as read, Download missing messages, New folder, Rename folder, Delete folder |
| an account    | Check mail, New message, Edit account, Refresh folder list             |
| an attachment | Save attachment; an invitation: Add to ECal, Save attachment           |
| the editor    | Send now, Put in Outbox, Attach file, Address book, Hebrew keyboard, Cut line, Copy line, Paste |

Pick with a click, press-drag-release, or the arrow keys and Return;
Esc closes the menu.

**New message** opens the editor in a window of its own.

**Options > Font** opens a font selector like other GEM programs': the
font families on the left, the sizes of the chosen one beside them,
what kind of font it is (bitmap, Speedo, TrueType, Type 1) and a sample
below. The system font comes in two sizes (8×8 and 8×16); with a GDOS
such as NVDI or SpeedoGDOS its fonts are listed too. EMail lays text out
in character cells, so it needs a monospaced font: only those are
listed. The first time, EMail measures each font to find them ("Looking
for fixed-width fonts" on the status line; it can take a while with many
vector fonts) and keeps the answers in EMAIL.FNT, so the selector opens
quickly afterwards and only new fonts are measured. Delete EMAIL.FNT to
measure them all again. Menus and dialogs keep the system font.

**Hebrew letters.** Fonts keep the Hebrew letters in different places:
the standard Atari font (TOS, EmuTOS) at the Atari character set's,
Israeli system fonts and many GDOS fonts where ISO-8859-8 or DOS 862 put
them. If Hebrew shows as other symbols, open Options > Font: the three
buttons under "Hebrew in bitmap fonts: pick the sample that reads
right" each show the word shalom as that kind of
font would; pick the one that reads correctly, and the sample below
shows the result. Those buttons are for the system and bitmap fonts.
A vector font (Speedo, TrueType, Type 1) is asked through NVDI 5 for
its Hebrew letters by Unicode, as FreeMono and Liberation Mono have
them; the line "Type:" says "Hebrew from the font". A vector font
without Hebrew (Bitstream's Speedo fonts), or any vector font without
NVDI 5, says "Hebrew in the system font": the letters are then drawn in the system
font, in the same cells.

**About EMail** (first menu) shows the version, how EMail is connected
and the memory it runs in. EMail loads itself and keeps its data in TT
RAM (the fast RAM of a TT, or of a Falcon with an accelerator card)
whenever the machine has some: faster, and more room than ST RAM. The
About box says "Running in TT RAM", or "Running in ST RAM (TT RAM not
used)" when the program file's TT RAM flags were switched off. It also
names the connection: Gateway, Falcon (TLS on the 68030) or Falcon/DSP.

## Writing

The editor's buttons along the top are **Send**, **Outbox**,
**Attach...** and **Address book...**. Rest the mouse on one to see what
it does and its key. F10 switches the Hebrew keyboard on and off. A right-click in the editor gives the same
commands, in the same order, and Cut, Copy and Paste.

Below them, the header lines, and your text below the line:

```
To: Dana <dana@example.com>, avi@example.org
Cc:
Subject: Shalom
Attach: C:\PICS\FALCON.JPG
───────────────────────────
your text...
```

- Several addresses are separated by commas. Add a `Bcc:` line for
  hidden copies.
- **People show by name.** On the To:, Cc: and Bcc: lines each person
  shows framed, by the name EContact has for them, or as the address when
  EContact doesn't know it; rest the mouse on a name to see the address.
  The one the cursor is in shows as typed, to change it: click a name to
  change that one.
- Tab jumps to the next header line and, from the last one, into the text.
- **Ctrl+B (Address book)** opens EContact's window to choose people or a
  group; they go on the To: line (or the Cc: or Bcc: line the cursor is
  on). A name begun on the line is what EContact searches for, and is
  replaced by the people chosen. When EContact isn't loaded, Ctrl+B opens
  EContact's file if you chose it in Settings > Contacts (see EContact
  below), else it offers the addresses you wrote to before (ADDRESS.TXT).
  With no message open, it starts a new one.
- Ctrl+T (Message > Attach file) adds an `Attach:` line; you can also
  type one. The file selector picks one file at a time: to attach
  several at once, drop them on EMail from the desktop (see below).
- Lines wrap at 72 characters as you type (Options > Settings).
- **Shift+Ctrl+D sends now,** straight from memory, with anything
  already waiting in the Outbox; the disk is used only when the message
  can't go, and then it waits in the Outbox. Ctrl+S (Message > Put in
  Outbox) keeps it for later; it goes out with the next Check mail.
  Closing the window asks whether to keep the message in the Outbox.

Replies quote the original and keep the thread together (In-Reply-To and
References). Forwarding a message with attachments attaches the original.

## From other programs

**EContact**, the AERA address book, is EMail's address book when it is
loaded (as the accessory, which needs 2 MB or more next to EMail, or
alongside under MiNT or MagiC). EMail asks it
for the names behind the addresses it shows: in the list, in the From:,
To: and Cc: lines of a message (a name's tooltip is its address) and when
writing. Nothing is copied between their folders. Without EContact,
EMail shows the addresses as they are (the list keeps the name the
message gives).

**EContact's file, when EContact isn't loaded.** On a machine short of
memory, where EContact runs as a program now and then rather than as
an accessory, EMail can read the contacts file EContact keeps in its
folder, `CONTACTS.TXT`. Turn it on in Options > Settings > Contacts:
"When EContact isn't loaded, read its contacts file", and give
EContact's folder (the **...** button finds it with the file
selector). It is off at first, and kept as PEOPLEFILE= and PEOPLEDIR=
in EMAIL.INF. Then, whenever EContact isn't running, the list and the
messages show the file's names, and Ctrl+B opens a small Contacts
window on it: type to find, click to mark people, double-click to take
one, Return takes the marked ones (or the one the cursor is on), Esc
cancels. When EContact is loaded, EMail asks it as always and doesn't
read the file. The names are as new as EContact's last run.

**ECal**, the AERA calendar, takes the invitations that come by mail
(an `.ics` file, or a calendar part sent with the text, as most
invitations are). Double-click the `» Invitation:` line in the message,
or right-click it and choose Add to ECal: EMail writes it as
`INVITE.ICS` next to `EMAIL.PRG` and hands it to ECal, whose New Event
dialog opens filled in from it, for you to confirm. ECal must be running
(the accessory, or alongside under MiNT or MagiC); when it isn't, EMail
says so, and you can save the invitation instead.

**mail** in EContact opens a new message to the
person or group you chose. Any program, or a desktop such as Thing,
jinnee or Gemini, can do the same with the AV protocol's VA_START:

- a `mailto:` link opens a new message, with the link's subject and
  text when it has them;
- files are attached to the message being written, or to a new one.

Under MultiTOS, MagiC or XaAES you can also drag files onto EMail's
windows to attach them, or drag text from another program: it goes into
the message being written at the cursor, or starts a new message.

## Copy and paste

Edit > Cut, Copy and Paste (^X, ^C, ^V) use the GEM clipboard, which
other programs share:

- **In the message being written,** ^V puts the clipboard's text in at the
  cursor. With nothing selected, ^C copies the line the cursor is in and
  ^X cuts it; on a To:, Cc: or Subject: line only what comes after the
  name. EContact puts an address on the clipboard when EMail isn't
  running: paste it into the To: line.
- **In the main window,** ^C copies the message shown (its From, To,
  Date and Subject lines, then the text), or the folder's name when the
  folder list has the keyboard.
- **In dialogs,** ^C copies a field, ^X cuts it and ^V pastes into it.
  Nothing is ever copied out of a password field, but you can paste one
  in.

## Hebrew

EMail shows Hebrew mail in UTF-8, ISO-8859-8 or windows-1255, Hebrew
subjects and names (RFC 2047), Hebrew attachment names and Hebrew folder
names. Hebrew paragraphs are laid out right to left and aligned to the
right; numbers and Latin words inside them stay left to right.

Press **F10** (or Options > Hebrew keyboard) to type Hebrew with the
Israeli SI-1452 layout: the keys where the Hebrew letters are on an
Israeli keyboard. Press F10 again for Latin letters. In a Hebrew line the
Left arrow moves forward, as on Hebrew systems. Messages go out in UTF-8,
which every modern program reads.

The Atari font has the 27 Hebrew letters but no vowel points (niqqud),
so those are left out when showing a message.

## Keys

| Key          | Does                       | Key          | Does                    |
|--------------|----------------------------|--------------|-------------------------|
| Ctrl+N       | new message                | Shift+Ctrl+N | check mail (or Ctrl+K)  |
| Ctrl+R       | reply                      | Shift+Ctrl+R | reply to all (or Ctrl+E) |
| Shift+Ctrl+F | forward                    | Ctrl+U       | mark as unread          |
| Ctrl+G       | flag / unflag              | Ctrl+M       | move to a folder        |
| Ctrl+C       | copy                       | Ctrl+X       | cut                     |
| Ctrl+V       | paste                      |              |                         |
| Delete       | delete (to Trash)          | Ctrl+A       | select all messages     |
| Shift+Ctrl+D | send (in the editor)       | Ctrl+S       | into the Outbox (editor) |
| Ctrl+T       | attach a file              | Ctrl+B       | address book            |
| F10          | Hebrew keyboard on/off     | Help         | this guide              |
| Esc          | close the editor; stop     | Ctrl+W       | close the window        |
| Tab          | next pane                  | Ctrl+Q       | quit                    |
| Ctrl+,       | settings                   |              |                         |
| Shift+click  | add/remove one message     | Right button | what you can do there   |
| Control+click| select a range             |              |                         |

The keys are the AERA programs' own (Ctrl+S saves, Ctrl+W closes, Ctrl+,
opens the settings; Ctrl+F is kept for Find), with Mail's on the Mac for
the rest. Closing the main window
ends EMail. Without a help viewer, Help offers the list of keys.

**Esc cancels.** In any dialog box or alert, Esc or Undo does what Cancel
does (or the button that does nothing, such as Later): nothing is
deleted, moved, sent or saved, and what was typed is thrown away, even
with the cursor in a text field.

**Long text in a field.** A field takes as much as its setting holds,
not only what it shows: a long address, folder or signature line goes
in, and the field scrolls with the cursor. Left and Right move the
cursor (Shift: to the start or the end), ClrHome goes to the start,
Backspace and Delete take out a character.

**Stopping.** While the busy bee shows (checking mail, sending, reading
a folder), Esc or Undo stops what EMail is waiting for from the server,
at once rather than after the minute it would wait: the alert then says
"stopped", and EMail carries on from what it has. Anything else you type
meanwhile is kept, and comes through once EMail is done.

**Several messages at once.** Shift+click adds a message to the selection
(or takes it out), Control+click selects everything from the last message
you clicked, and Ctrl+A (Edit > Select all) the whole list. The message
pane then says how many are selected; Mark as read, Mark as unread, Flag,
Move to and Delete (from the Message menu, the keys or a right-click on the
selection) work on all of them. A plain click selects one message again.

**The status line**, under the main window's title, says what EMail is
doing while the busy bee shows: "Opening the message...", "Moving 3 of
12 to Archive...", "Inbox: downloading 2 of 40 [####......] 41%".

**Move to** shows the folders in alphabetical order, a subfolder with its
parent ("Archive / 2023"); the arrows page through a long list, a click
picks a folder and OK (or a double click) moves the messages there.

**Drag a message onto a folder** in the folder pane to move it there; a
message that is part of a selection takes the whole selection with it.
An outline follows the pointer and the folder it is over is lit when the
messages can go there (a folder of the same account, not the one they
are in, not the Outbox). Let go anywhere else and nothing happens; Esc,
Undo or the right button stops the drag.

## How your mail is kept

**On disk** each folder keeps its messages in one file, MESSAGES.DAT,
with an index (INDEX.DAT) of where each one is: a few files to look up
instead of one per message, which counts on an Atari hard disk. What
comes from the server (a check, Download missing messages) gathers in memory and is
written in one go at the end, so the disk works once, not for every
message; what is only on the Atari (the Outbox, a POP account's
folders) is written at once. If the Atari is reset in the middle, a
message that didn't make it whole is fetched from the server again.

**IMAP** accounts are mirrored: EMail keeps the headers of the newest
messages of each folder, as many as you have loaded (100 to start with),
and the messages you have read; the counts in the folder pane are the
server's. Changes made elsewhere (read, deleted, moved on your phone) show up
at the next check. The folder list is read from the server once each time
EMail starts (at the first check), so folders made, renamed or deleted
elsewhere show up then; Folder > Refresh folder list reads it any time. Opening, flagging or marking a message changes it at once
on the Atari; EMail tells the server in one go when you pause for a few
seconds, and at the latest when you quit. A folder you opened in the last two
minutes opens straight from disk; Check mail always asks the server. Check
mail fetches the Inbox and asks for the counts of every other folder too,
all the questions sent at once so it is one short wait, so mail the server
filed elsewhere shows up: "3 new messages, 1 in Junk" (new mail in Trash,
Sent and Drafts doesn't count as new). The lists of those folders load
when you open them. After a move or a delete no other message is chosen, so nothing is
opened or downloaded that you didn't ask for. Deleting moves a message to Trash on the server;
deleting in Trash removes it for good.

**Download again** (Message menu, or a right-click on the message) throws
away the copy on the Atari and fetches the server's again: for a message
that came cut off or garbled over a bad line. It works on a selection
too, and on a message that didn't open at all.

**Download missing messages** (Folder menu, or a right-click on the
folder) puts every message of an IMAP folder on the Atari, to read
without the server: all the headers, not 100 at a time, and then only
the messages the Atari hasn't got yet, counted on the status line;
nothing already there is downloaded again. It asks first, saying how
many are on the Atari already. Esc stops it; what came stays. From then
on the folder keeps all its messages listed, and doing it again fetches
only the new ones. With "Keep read messages on
disk" off (Settings), they are kept only until EMail quits.

**POP3** accounts download new mail into local folders (Inbox, Sent,
Trash) and remember what they fetched, so nothing comes twice. With
"leave mail on the server" off, EMail deletes it from the server once it
is safely on disk.

**Offline** (Options > Work offline): EMail doesn't connect, and you can
read everything already on disk. New messages wait in the Outbox.

**Not connected**: when a connection or login fails, EMail says so once
and then works from the cache for that account, as if offline: messages
already on disk open at once, and one that was never downloaded shows a
note instead of a new attempt. Marking messages read, unread or flagged
works as usual and reaches the server at the next connection. Check mail
(^K, or a click on the account's name) or the timed check connects again.

**Colours**: on a screen with 16 colours or more EMail uses a few, for
meaning only, as modern mail programs do: the selection in blue, the
focused pane's title bar dark, unread counts in blue, the flag mark in
red, dates and header labels in grey, and quoted text in blue, green and
brown by depth. With 256 colours it sets its own soft shades (and puts
the palette back when it quits). Settings > Look switches them off;
monochrome screens look as they always did.

**Settings** (Options > Settings, Ctrl+,) has five parts in its
sidebar: **Mail** (checking every few minutes, how many messages to load
at a time, keeping read messages on disk), **Writing** (where lines
wrap, starting with the Hebrew keyboard), **Signature**, **Look**
(colours), **Contacts** (EContact's file, below) and **Connection** (the
time zone, Falcon mode, the DSP and the protocol log).

**The time zone** (Settings > Connection) is chosen by name from a
list (Asia/Jerusalem, Europe/London, ...) as in ECal; summer time follows
by itself. The first time, EMail fills it in from the machine: TZ in the
environment (MiNT, MagiC; a name like Europe/Paris or POSIX like
IST-2IDT), MiNT's /etc/timezone, or the minutes an older EMAIL.INF kept;
otherwise UTC. It is kept as TIMEZONE= in EMAIL.INF.

**Settings > Signature**: up to four lines. A new message has it at the
end, after a "-- " line. A reply or a forward has it under the line
where you type, above the quoted or forwarded text, so it goes with
what you write (there without the "-- " line, after which some mail
programs hide the rest). There is one signature for all your accounts.

## Folders

Folder > New folder makes a folder on the IMAP server (Hebrew names
work). Folder > Rename folder gives the open one a new name on the
server; it stays under its parent, its subfolders move with it, and the
messages already on disk stay with it. Inbox, Sent, Trash, Drafts and the
Outbox keep their names. Folder > Delete folder removes the open one with
all its messages. Right-click a folder to rename or delete any folder,
to mark all its messages as read, or to download them all.
The folder list is read from the server when EMail starts; Folder >
Refresh folder list reads it again after you changed it elsewhere.

## The desktop icon

The release has EMail's icon for the desktop (in the zip, in `icons/`): `EMAIL.RSC` holds a black and
white and a 16-colour version to copy into `DESKICON.RSC` /
`DESKCICN.RSC` with a resource editor, and `EMAIL.ICN` / `EMAILMK.ICN` are
the image and mask for icon editors. For EmuTOS,
`tools/icon/add_to_emuicon.py`, in EMail's source on GitHub, adds it to
`EMUICON.RSC`.

## When something goes wrong

- **"can't connect"**: is the gateway running (`gateway/aera-gateway
  status`, or `sudo systemctl status emailgate`, on the Pi)? Is the address right? Is STinG (or MiNTnet) set
  up — can other network programs reach the Pi? An answer that starts
  with "emailgate:" comes from the gateway, and says what it couldn't do
  (reach your provider, trust its certificate, or let your Atari in).
- **Falcon mode: "certificate not trusted"**: is CACERT.PEM next to
  EMAIL.PRG, and is the server name right (the certificate must be for
  that name)? **"certificate out of date"**: check the date, time and
  time zone. **"not enough memory"**: close other programs, check in
  the About box that EMail runs in TT RAM, or use the gateway.
- **"login failed"**: check user and password; Gmail, iCloud, Yahoo and
  Fastmail need an app password.
- **"EMail can't write its key file"**: EMail keeps your passwords in
  EMAIL.INF encrypted, with a key it keeps in `EMAIL.KEY` next to it.
  When that file can't be written (a locked disk, a full one, a folder
  in its place) the passwords go into EMAIL.INF as they are, and EMail
  says so once each time it starts; once `EMAIL.KEY` can be written,
  they are encrypted the next time EMail saves its settings.
- **Options > Protocol log** writes the conversation with the servers to
  `EMAIL.LOG` (passwords are hidden); it shows exactly what a server said.
  It is written in blocks, and at once at every step (connecting,
  logging in, an error), so after a crash or a reset it holds what led
  up to it.
- **A crash**: if EMail itself goes wrong (what would be 2 to 8 bombs),
  it says so, writes what happened to `CRASH.LOG` and quits, leaving the
  Atari running. A crash in the system or a driver still gives the
  bombs; TOS keeps a record of them, and EMail adds it to `CRASH.LOG`
  the next time it starts (and says so if the crash was in EMail). Send
  `CRASH.LOG`, with `EMAIL.LOG` if the log was on, to the author. Under
  MiNT or MagiC, which handle crashes themselves, there is no CRASH.LOG.

## Limits

- Encryption on the Atari itself (Falcon mode) needs a 68030 and 4 MB;
  other machines use the gateway. TLS 1.2 only (every provider has it).
- No OAuth sign-in (Outlook.com, Microsoft 365).
- Messages are plain text; HTML mail is shown as text and attachments are
  saved, not shown.
- Up to 8 accounts, 64 folders per account.
