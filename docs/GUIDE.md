# EMail user guide

EMail is an e-mail program for the Atari ST, STE, Mega ST/STE, TT and
Falcon. It reads mail from IMAP or POP3 servers, sends it over SMTP,
keeps a copy of your IMAP folders on disk so you can read offline, and
reads and writes Hebrew.

## What you need

- An Atari with TOS 1.04 or newer (TOS 4 on the Falcon), EmuTOS, MagiC or
  MiNT, and a screen of at least 640×200 (ST medium); 640×400 (ST high)
  or the Falcon's 640×480 show three windows side by side
- 1 MB of memory or more: EMail itself takes about 332 KB, and about
  140 KB more on a TT or Falcon while `TLS.OVL` is loaded for a Direct
  connection; more memory holds bigger folders. With EContact's accessory loaded, 2 MB or
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
  - a **Direct connection**: EMail does the encryption itself (a Falcon,
    or a TT, with 4 MB of memory or more), see [Direct connection](#direct-connection)

## Installing

The files are in the latest release on EMail's GitHub page
(github.com/whomper/EMail/releases), each on its own and all together in
`EMAIL-x.y.zip`. Copy `EMAIL.PRG` to any folder on any drive: nothing
has to be in a particular place. It is the same program for every Atari,
from a 1 MB ST to a Falcon.

`TLS.OVL` holds the encryption for a Direct connection (see [Direct
connection](#direct-connection)). On a TT or Falcon, copy it next to
`EMAIL.PRG`; EMail loads it the first time a Direct connection needs it.
An ST never loads it (an ST is too slow for the encryption and reaches
secure servers through the Pi gateway), so on an ST you can leave it
out, and the memory stays free for your mail. EMail keeps everything in the folder it
is started from, which the desktop makes its own folder, so a folder of
its own keeps it all together. Here it is `C:\EMAIL\`:

```
C:\EMAIL\EMAIL.PRG      the program
C:\EMAIL\TLS.OVL       the encryption for a Direct connection (TT, Falcon)
C:\EMAIL\EMAIL.INF      settings and accounts (each setting has a note above it)
C:\EMAIL\EMAIL.KEY      the key the passwords in EMAIL.INF are encrypted with
C:\EMAIL\ADDRESS.TXT   the addresses you wrote to (Ctrl+B without EContact)
C:\EMAIL\EMAIL.LOG      protocol log, when switched on
C:\EMAIL\EMAIL.FNT      which fonts are fixed-width (see Options > Font)
C:\EMAIL\CRASH.LOG      what happened, if EMail ever crashes
C:\EMAIL\CACERT.PEM    root certificates, for a Direct connection only
C:\EMAIL\ROOTS.DAT     the same, decoded ahead of time (Direct connection only)
C:\EMAIL\EMAIL.HYP     this guide, for EHypView or ST-Guide (the Help key opens it)
C:\EMAIL\EMAIL.REF     its page names, for ST-Guide's reference list (optional)
C:\EMAIL\EMAIL\         your mail: one folder per account, one per mailbox,
                        each with its messages in one file (MESSAGES.DAT)
C:\EMAIL\EMAIL\SEED.DAT  randomness kept for a Direct connection's encryption
C:\EMAIL\EMAIL\SIGS.DAT  certificates' signatures already checked, so a Direct
                        connection doesn't check them again (slow on a 68030)
```

Start it. The first time, EMail asks for your account.

`EMAILCHK.ACC`, EMail check, is a desk accessory that tells you when new
mail has come while EMail isn't running. It goes in the root of the
boot drive (`C:\EMAILCHK.ACC`), with its settings beside it
(`C:\EMAILCHK.INF`); leave it out if you don't want it. See [New mail
while EMail isn't running](#new-mail-while-email-isnt-running).

This guide is also in `EMAIL.HYP`, a hypertext: with EHypView, ST-Guide
or HypView running (as an accessory, or as a program under MultiTOS or
MagiC), the Help key or **Help > EMail help** opens it, from next to
`EMAIL.PRG` or from the viewer's own help folder. Without a viewer, Help
says how to get one and offers the list of keys instead; **Help > Keys**
shows that list at any time.

## Updating

Copy the new release's files over the old ones: `EMAIL.PRG`, `EMAIL.HYP`
and `EMAIL.REF`, and for a Direct connection `TLS.OVL`, `CACERT.PEM` and
`ROOTS.DAT`. Coming from 1.55 or before on an ST, delete `EMAILST.PRG`:
`EMAIL.PRG` is for every Atari now, and reads the same `EMAIL.INF` and
folders.
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
(github.com/whomper/AERA-Gateway). On a fresh Pi, one line sets up
every AERA gateway, the mail one with your provider's servers:

```
sudo apt install -y git && git clone https://github.com/whomper/AERA-Gateway ~/AERA-Gateway &&
  ~/AERA-Gateway/update-pi.sh --allow ATARI_IP --mail IMAP_SERVER SMTP_SERVER[:587] [POP3_SERVER]
```

Or step by step, the mail gateway alone:

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
prints the Pi's address and the ports to give EMail. After
`update-pi.sh` has run once, `sudo aera-gateway` works from any folder.

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
| User, Password | your login; Gmail, iCloud, Yahoo, Fastmail: an app password, see [App passwords](#app-passwords-gmail-icloud-yahoo-fastmail) |
| POP3: leave mail on the server | keep a copy on the server after downloading    |
| Outgoing       | the Pi's IP and 587 through the gateway; leave User empty to use the incoming login, or type `-` for a server that needs none |
| Passwords      | shown as `*`; click the eye beside the field to see what you typed |

EMail holds up to eight accounts. Edit or delete one with Options >
Accounts > Edit. Passwords are kept in `EMAIL.INF` encrypted with a random
key EMail makes the first time, in `EMAIL.KEY`: someone who reads or copies
EMAIL.INF alone can't read them, but whoever has both files can, so keep
EMAIL.KEY to yourself. Lose EMAIL.KEY (or start with a new one) and EMail asks
you to type the passwords again.

## App passwords (Gmail, iCloud, Yahoo, Fastmail)

Gmail, iCloud, Yahoo and Fastmail don't let a program like EMail in with
the password you use on their website: they refuse it, and EMail says the
login failed (for Gmail: "Gmail refused the password"). Instead you make
an **app password** on the service's website, once, on a computer or a
phone, and type it into EMail in place of your normal password: in
Options > Accounts, as the Password of the incoming mail and of the
outgoing mail (or leave the outgoing User empty, and EMail uses the
incoming login for both). It is the same through the Pi gateway and with
a Direct connection. Your normal password stays as it is, and you can
delete the app password on the same page at any time; EMail then can't
log in until you give it a new one.

**Gmail (Google)**

- First, 2-Step Verification must be on: in your Google Account, Security
  & sign-in, under "How you sign in to Google", Turn on 2-Step
  Verification.
- Make the app password at https://myaccount.google.com/apppasswords and
  follow the steps there. Google shows it as 16 letters in four groups:
  type the 16 letters without the spaces.
- User: your Gmail address.
- The page isn't offered when your account uses only security keys for
  2-Step Verification, has Advanced Protection, or belongs to a work,
  school or other organisation (its administrator decides).
- Changing your Google password deletes your app passwords: make a new one.
- Google's help: https://support.google.com/accounts/answer/185833

**iCloud (Apple)**

- First, your Apple Account needs two-factor authentication.
- Sign in at https://account.apple.com, choose App-Specific Passwords in
  Sign-In and Security, and generate one, following the steps.
- User, incoming mail: usually the name of your iCloud address, without
  @icloud.com (if EMail can't log in with that, the whole address).
  Outgoing mail: the whole address, typed in the outgoing User. The same
  app-specific password for both.
- Changing or resetting your Apple Account password revokes all your
  app-specific passwords: make a new one.
- Apple's help: https://support.apple.com/en-us/102654, and its settings
  for mail programs: https://support.apple.com/en-us/102525

**Yahoo**

- Sign in to Yahoo's Account Security page, click Create app password
  under "External connections" and follow the steps.
- User: your Yahoo address.
- An app password goes on working when you change your Yahoo password:
  delete it on the same page to stop it.
- Yahoo's help: https://help.yahoo.com/kb/SLN15241.html

**Fastmail**

- Not with the Basic plan, which has no IMAP or SMTP.
- In Fastmail's settings, Privacy & Security, Connected apps & API
  tokens: Manage app passwords and access, New app password, and Generate
  password (Mail, Contacts & Calendars, the default, includes mail).
- User: your whole Fastmail address.
- Fastmail's help: https://www.fastmail.help/hc/en-us/articles/360058752854

## Direct connection

EMail works in one of two ways, chosen with **Options > Direct connection
(TLS)** or "Direct connection: TLS on this Atari" in Options > Settings >
Connection (this was called Falcon mode before 1.56):

- **Off** (the default, any Atari): EMail talks plainly to the Raspberry
  Pi gateway, and the Pi encrypts the connection to your provider.
- **On**: EMail encrypts on the Atari itself (TLS 1.2) and talks straight
  to your provider; no Pi is needed. On a Falcon the DSP56001 checks the
  servers' certificates (about 0.12 s each instead of 1.3 s or more on
  the 68030). "Use the DSP for the signatures" in Settings > Connection
  switches that off, leaving all of it to the 68030 (slower, but a way to rule the DSP out if something goes wrong).

A Direct connection needs:

- a 68020 or better: a Falcon, or a TT (without the DSP part); a 68000
  ST is too slow, and EMail says so. An `EMAIL.INF` from a Falcon with
  the Direct connection on works on an ST all the same: EMail goes
  through the gateway there and leaves the setting as it is
- `TLS.OVL` next to `EMAIL.PRG`, from the release: the encryption
  (BearSSL, and the DSP's part), about 140 KB, the same file for every
  AERA program. EMail loads it when you switch the Direct connection on,
  and says at once if it is missing or doesn't suit
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
and forth without typing them again: the gateway's (used when the Direct
connection is off) and the provider's own (used when it is on). Options > Accounts >
Edit shows the set of the mode you are in, and changing a password in one
mode leaves the other alone. The first time, the Direct connection's login starts as
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
never sends a password over an unencrypted connection in a Direct connection.

Connecting takes a few seconds on a 50 MHz Falcon; after that mail comes
in at 15 to 25 KB/s (the 68030 decrypts every byte; through the Pi it is
about 37 KB/s), which is plenty for text.
About > shows how the current connection is made (Direct, with TLS.OVL's
version and the DSP when it checks the signatures, or Gateway), and the
protocol log records the cipher, the timings and TLS.OVL's loading.

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
- **Messages** (top right): newest first. Unread messages have a dot
  in the first place, and are blue where there are colours; `!` in the
  second place is flagged, `@` in the third probably has attachments. In
  Sent and the Outbox it shows who you wrote to. Its header shows how
  many messages are loaded and how many there are.
- **Message** (bottom right): the selected message. Click an attachment
  line (`»`) to save it with the file selector. A calendar invitation
  shows as `» Invitation:` and its title: double-click it to add it to
  ECal (see From other programs). A message sent as both plain text
  and HTML, as most are, is shown from its HTML, as text: its links
  by their names, a quoted reply's lines with `>` before them, and a
  picture by its words (a picture that is a link, such as "Shop now",
  can be clicked). Quoted lines are coloured by how deep they are quoted (light
  grey in black and white). **Links** are underlined (and blue, in
  colour): the addresses written in a message (`http://`, `https://`,
  `www.`, `mailto:`) and an HTML message's links. Click one to open it:
  a web address in your browser (see From other programs), a `mailto:`
  link as a new message. Rest the mouse on a link to see where it goes.

**Drag the dividers** between the panes to size them; the pointer turns
into a hand over a divider. The window's own size box (bottom right)
sizes the window. The message pane scrolls with the window's own
slider, at its right edge; the folder and message lists have scroll
bars of their own, drawn as your desktop draws scroll bars (in 3D on
TOS 4, in its theme under XaAES): click an arrow or hold it, click the
bar above or below the slider for a page, or drag the slider. The pane
with the dark header has the keyboard: click a pane, or press Tab to
move on. The main window's own place and size, the dividers, the
editor's place and the font are all kept in `EMAIL.INF` for next time.

Click a message to read it. Up and Down move through the list, and the
message comes up once you stop: held down, they go through the list
without opening each message on the way. Return moves the keyboard to
the message. In the message pane Space and
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
| a link        | Open link, Copy link                                                   |
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
names the connection: Gateway, or Direct with TLS.OVL's version (and DSP).

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

**EWeb**, the AERA web browser, opens the links you click in a message.
When a browser is running beside EMail (EWeb, or CAB or HighWire; under
MiNT or MagiC), EMail hands it the link. When none is, as on plain TOS,
where one program runs at a time, EMail asks: **Open in EWeb** closes
EMail and starts EWeb on the link (quitting EWeb takes you back to the
desktop); under MiNT or MagiC, EWeb starts beside EMail instead. **Copy
link** puts the address on the clipboard. EMail finds `EWEB.PRG` (on an
ST `EWEBST.PRG`) next to itself; elsewhere it asks you once, with the
file selector, and keeps it as BROWSER= in EMAIL.INF. A link longer
than a command line can carry goes to EWeb as `LINK.HTM` in EMail's
folder, a page that sends it on.

**mail** in EContact opens a new message to the
person or group you chose. Any program, or a desktop such as Thing,
jinnee or Gemini, can do the same with the AV protocol's VA_START:

- a `mailto:` link opens a new message, with the link's subject and
  text when it has them;
- files are attached to the message being written, or to a new one.

Under MultiTOS, MagiC or XaAES you can also drag files onto EMail's
windows to attach them, or drag text from another program: it goes into
the message being written at the cursor, or starts a new message.

## New mail while EMail isn't running

**EMail check** (`EMAILCHK.ACC`, in the release beside `EMAIL.PRG`) is
a small desk accessory that looks for new mail while EMail isn't
running. Copy it to the root of the boot drive (`C:\`), with the other
accessories, and restart the Atari. It takes about 180 KB: itself and
64 KB for its work (more with many accounts: it measures what it needs
and takes that at the next start, MEMKB= in `EMAILCHK.INF`; if it ever
runs out, it says so and takes more at the next start).

Checking over a **Direct connection** needs about 230 KB more, for
`TLS.OVL` and the root certificates. The accessory takes that only in
TT RAM (a TT, or a Falcon with fast RAM on an accelerator), where there
is plenty. With ST RAM only, it would come out of the memory your
programs need (on a 4 MB Falcon with NVDI and the other accessories,
EMail itself wouldn't fit), so there it leaves Direct connection
accounts unchecked; Check now in its box says so, and the two ways out:
check through the gateway (Options > Direct connection off), or give
it the memory: `HEAPKB=300` in `C:\EMAILCHK.INF`, then restart the
Atari.

It reads EMail's own settings, so there is nothing to set up twice: the
accounts and passwords in `EMAIL.INF`, the Direct connection or the
gateway, and the time between checks, "Check for new mail every ...
minutes" in Options > Settings > Mail (CHECK= in `EMAIL.INF`; when it is
0, the accessory doesn't check on its own either). It changes nothing of
EMail's, here or on the server: nothing is downloaded or marked read.

The first time, open it from the Desk menu (**EMail check**): it asks
where EMail is, and you choose `EMAIL.PRG` in the file selector. It
keeps the folder in `C:\EMAILCHK.INF` (on the boot drive). From then on
it checks a minute after the Atari starts, and then as often as EMail
would, only while you aren't moving the mouse or typing (a Direct
connection's handshake holds the Atari for a second or more).

New mail is what EMail would show as new when you start it: the unread
messages that came into the Inbox of each account since EMail last
looked (for POP3, the ones EMail hasn't downloaded yet). When some has
come, it tells you:

> 2 new messages in the Inbox. Start EMail to read them.

With more than one account it says how many in each. You are told again
when more mail comes, not about the same messages. While EMail runs,
the accessory leaves the checking to it, and counts the time from when
EMail quits.

**Open EMail.** Under MagiC or MiNT, or with a desktop that speaks the
AV protocol (Thing, TeraDesk, Gemini, jinnee), the box asks "Open
EMail?", and **Open** starts it. TOS's own desktop, and EmuTOS's, can't
be asked by an accessory to start a program, so there it only tells
you; start EMail from the desktop as always.

From the Desk menu it says when it last looked and what it found, with
**Check now** and **About**.

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
| Shift+Ctrl+F | forward                    | Shift+Ctrl+U | mark as unread          |
| Ctrl+G       | flag / unflag              | Ctrl+M       | move to a folder        |
| Ctrl+C       | copy                       | Ctrl+X       | cut                     |
| Ctrl+V       | paste                      |              |                         |
| Delete       | delete (to Trash)          | Ctrl+A       | select all messages     |
| Shift+Ctrl+D | send (in the editor)       | Ctrl+S       | into the Outbox (editor) |
| Ctrl+T       | attach a file              | Ctrl+B       | address book            |
| F10          | Hebrew keyboard on/off     | Help         | this guide              |
| Ctrl+U       | close the window           | Ctrl+W       | the next window on top  |
| Tab          | next pane                  | Ctrl+Q       | quit                    |
| Ctrl+,       | settings                   | Undo         | cancel a dialog         |
| Esc          | empty a field; stop        |              |                         |
| Shift+click  | add/remove one message     | Right button | what you can do there   |
| Control+click| select a range             |              |                         |

The keys are the AERA programs' own (Ctrl+S saves, Ctrl+U closes a
window, Ctrl+W brings EMail's other window to the top, Ctrl+, opens the
settings; Ctrl+F is kept for Find), with Mail's on the Mac for the rest. Closing the main window
ends EMail. Without a help viewer, Help offers the list of keys.

**Undo cancels, Esc empties a field.** In any dialog box, Undo does what
Cancel does (or the button that does nothing, such as Later): nothing is
deleted, moved, sent or saved, and what was typed is thrown away. Esc
empties the text field the cursor is in, as GEM's own dialogs do, and the
dialog stays open; in a dialog without a text field, and in an alert,
Esc cancels too. In a message you are writing Esc does nothing; Ctrl+U
closes its window.

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

**Offline** (Options > Work offline): EMail doesn't connect and doesn't
check on its timer, and you can read everything already on disk. New
messages wait in the Outbox. Switched off again, EMail checks at once and
then on its timer. The menu switches it for this session; whether EMail
starts offline is the setting in Settings > Mail.

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
brown by depth, links in blue. With 256 colours it sets its own soft
shades (and puts the palette back when it quits). With four (ST medium
resolution) only links and unread counts are blue: EMail makes the red
blue while it runs, and red again when it quits. Settings > Look
switches them off; monochrome screens look as they always did (links
are underlined).

**Settings** (Options > Settings, Ctrl+,) has six parts in its
sidebar: **Mail** (checking every few minutes, how many messages to load
at a time, keeping read messages on disk, starting offline), **Writing** (where lines
wrap, starting with the Hebrew keyboard), **Signature**, **Look**
(colours; dialogs have the AES's own 3D look wherever it draws one: TOS
4, MagiC, XaAES and EmuTOS in colour), **Contacts** (EContact's file, below) and **Connection** (the
time zone, the Direct connection, the DSP and the protocol log).

**Start offline, with the mail on disk** (Settings > Mail, off at
first): EMail starts at once, offline, with the folders and messages it
has on disk, and doesn't connect, nor check on its timer. When you switch
Options > Work offline off, it connects and checks at once (the folder
list, every Inbox, the other folders' counts, the Outbox), and from then
on checks every few minutes as set. Off (as before), EMail checks every
account as it starts, then opens the first Inbox.

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
The folder list is read from the server at the first check of a session:
when EMail starts, or, when it starts offline, as it goes online. Folder >
Refresh folder list reads it
again after you changed it elsewhere.

## The desktop icon

EMail's icon is a letter coming out of an open blue envelope. The
release has it for the desktop (in the zip, in `icons/`): `EMAIL.RSC`
holds a black and white icon for TOS 1 and 2 and a colour one (256
colours, 16 and black and white) for TOS 4 and EmuTOS, to copy into
`DESKICON.RSC` / `DESKCICN.RSC` with a resource editor, and `EMAIL.ICN` / `EMAILMK.ICN` are
the image and mask for icon editors. Take `EMAIL.RSC` under TOS, EmuTOS,
or NVDI with TOS's palette loaded (COLORVDI); under NVDI with its own
palette take `EMAILNV.RSC`, the same icon with its 256-colour image in
NVDI's colours. For EmuTOS,
`tools/icon/add_to_emuicon.py`, in EMail's source on GitHub, adds it to
`EMUICON.RSC`.

## When something goes wrong

- **"can't connect"**: is the gateway running (`gateway/aera-gateway
  status`, or `sudo systemctl status emailgate`, on the Pi)? Is the address right? Is STinG (or MiNTnet) set
  up — can other network programs reach the Pi? An answer that starts
  with "emailgate:" comes from the gateway, and says what it couldn't do
  (reach your provider, trust its certificate, or let your Atari in).
- **"TLS.OVL is missing"** (or doesn't suit): copy `TLS.OVL` from the
  release next to `EMAIL.PRG`, the one that came with it.
- **Direct connection: "certificate not trusted"**: is CACERT.PEM next to
  EMAIL.PRG, and is the server name right (the certificate must be for
  that name)? **"certificate out of date"**: check the date, time and
  time zone. **"not enough memory"**: close other programs, check in
  the About box that EMail runs in TT RAM, or use the gateway.
- **"login failed"**: check user and password; Gmail, iCloud, Yahoo and
  Fastmail need an app password (see [App passwords](#app-passwords-gmail-icloud-yahoo-fastmail)).
- **"Gmail refused the password: it takes an app password"**: Gmail lets
  programs like EMail in only with an app password, not your Google
  password. Turn on 2-Step Verification in your Google Account, make an
  app password at myaccount.google.com/apppasswords and type it in as
  EMail's password (Options > Accounts > Edit); [App passwords](#app-passwords-gmail-icloud-yahoo-fastmail)
  has the steps. EMail says this whenever Google's servers refuse a
  login, through the gateway too. With a work or school account, its
  administrator may have turned app passwords off.
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
- **A crash**: if EMail itself goes wrong (what would be 2 to 8 or 11 bombs),
  it says so, writes what happened to `CRASH.LOG` and quits, leaving the
  Atari running. A crash in the system or a driver still gives the
  bombs; TOS keeps a record of them, and EMail adds it to `CRASH.LOG`
  the next time it starts (and says so if the crash was in EMail). Send
  `CRASH.LOG`, with `EMAIL.LOG` if the log was on, to the author. Under
  MiNT or MagiC, which handle crashes themselves, there is no CRASH.LOG.

## Limits

- Encryption on the Atari itself (a Direct connection) needs a 68020 or better
  (a Falcon or TT) and 4 MB;
  other machines use the gateway. TLS 1.2 only (every provider has it).
- No OAuth sign-in (Outlook.com, Microsoft 365).
- Messages are plain text; HTML mail is shown as text and attachments are
  saved, not shown.
- Up to 8 accounts, 64 folders per account.
