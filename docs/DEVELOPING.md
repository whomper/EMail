<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 AERA | Erez Yaary -->
# Building and testing EMail

The detail behind the README's Building, Source and Tests sections.

## Building

```sh
sudo apt install gcc-m68k-linux-gnu python3   # Debian/Ubuntu; no MiNTLib needed
make                          # binary/EMAIL.PRG, EMAILCHK.ACC, TLS.OVL (common's, as it is), CACERT.PEM and ROOTS.DAT
make test                     # unit tests on this computer, the fuzz inputs again, the gateway's tests
make itest                    # against a local Dovecot and a test SMTP server
make help HCP=/path/to/hcp    # binary/EMAIL.HYP and EMAIL.REF from docs/GUIDE.md
make dist HCP=/path/to/hcp    # dist/: each release file, and EMAIL-x.y.zip, as on the Releases page
make icons                    # icons/, ui/icon*.h, docs/icon*.png: the icon, from common/icons/EMAIL.txt (committed)
```

The ST-Guide help file `binary/EMAIL.HYP` is made from `docs/GUIDE.md` by `tools/mkhelp.py` and the HCP
compiler from the [hypview](https://github.com/th-otto/hypview) project (`common/tools/build-hcp.sh DIR`
builds it).

**Versions and releases.** `binary/` and `dist/` are what make builds; they are never committed, and a
build changes no tracked file. `VERSION` holds the last release, major.minor (e.g. 1.30), and the build
never changes it. Every build shows it as it is, in the About box and the User-Agent header. To release: `tools/bump.py`, commit and push, then
**Actions > Release > Run workflow** on `main` with what is new:
[`.github/workflows/release.yml`](../.github/workflows/release.yml) tags the commit `v1.31`, tests, builds
the program and the help file, and publishes the files and the zip as a GitHub Release with those
notes. Pushing a tag `v1.31` made with `git tag -a` does the same, with the tag's message as the notes.

No MiNTLib is needed: the program is freestanding (`common/atari/` and `atari/`) and
`common/tools/elf2tos.py` writes the TOS executable. It is built for the 68000, so it runs on every
Atari from the ST to the Falcon. A Direct connection's TLS is not in it but in `TLS.OVL` (the AERA
guidelines, 7.4): BearSSL, `tls.c` and the DSP code, 68030 code built once for the family in AERA-Atari
(`common/tlsovl/TLS.OVL`) and copied to `binary/` as it is. `common/atari/tls_ovl.c` loads it the
first time a Direct connection needs it, on a 68020 or better; an ST never does, and an `EMAIL.INF`
from a Falcon with `FALCON=1` keeps it while the ST goes through the gateway. The builds on this
computer (`make test`, `make itest`) link `tls.c` and BearSSL themselves. `ROOTS.DAT` is
`CACERT.PEM` decoded on the build computer by `common/tools/mkroots.c`, so the Atari doesn't have to.
The DSP program is `common/dsp/rsa.a56`; `common/dsp/mkh.py` turns it into `common/dsp/rsa_prog.h`
(with the `a56` assembler: `cd common/dsp && a56 rsa.a56 && python3 mkh.py a56.out rsa_prog.h
dsp_rsa_prog`). Like everything in `common/`, change it in AERA-Atari (below).

## Source

```
src/      the mail core: IMAP, POP3, SMTP, MIME, the local store, composing, mail operations
          (also built for this computer in the tests)
ui/       the GEM program: windows, drawing, the folder and message lists, the reader, the editor,
          dialogs, fonts; emailchk.c is EMAILCHK.ACC, with src/newmail.c (what is new in an Inbox)
atari/    EMail's own part of the TOS runtime: files, folders and time (plat_tos.c)
common/   code shared with the other AERA programs, from AERA-Atari: the TOS runtime (start-up,
          system calls, a small C library, AES/VDI, TCP over STinG/STiK or MiNTnet with a DNS
          client), connections and TLS (tls.c, and TLS.OVL in tlsovl/), charsets, bidi, dates and time zones, passwords,
          GEM building blocks (menus, alerts, the About box, help, iconify, clipboard, drag and
          drop), BearSSL, the DSP RSA program, Mozilla's root certificates, build tools,
          FAKESTNG.PRG for the Hatari tests (tools/fakesting).
          Change it in AERA-Atari, then run tools/sync-common.sh
gateway/  the Raspberry Pi gateways (emailgate.py is EMail's) and aera-gateway, their installer:
          a copy of AERA-Gateway's; change it there, then run tools/sync-gateway.sh
tests/    unit, fuzz and integration tests, the Hatari rigs (tests/hatari)
tools/    add_to_emuicon.py (the icon in EmuTOS's desktop), mkhelp.py (the help file), sync-common.sh, sync-gateway.sh, bump.py,
          the TLS measurements (tlsbench)
icons/    the desktop icon (RSC and ICN files), made by make icons from common/icons/EMAIL.txt
docs/     the user guide (GUIDE.md and front.json, its first page: the source of EMAIL.HYP) and the pictures
binary/   what make builds for the Atari (not committed: the releases have it)
```

## Tests

The mail core (`src/`) also builds on Linux, where it is tested against real servers before it runs on
the Atari:

```sh
make test           # charsets (Hebrew), MIME, message building, bidi
make itest          # IMAP, POP3, SMTP against a local Dovecot and a test SMTP server
TLS=1 make itest    # the same through the gateway (emailgate.py), TLS-only servers
FALCON=1 make itest # a Direct connection: EMail's own TLS, implicit TLS and STARTTLS
```

`make itest` needs `dovecot-imapd` and `dovecot-pop3d`; `TLS=1` and `FALCON=1` also need `openssl`.
`make test` also runs the gateway's own tests, and the fuzz targets' inputs again (`tests/fuzz`;
`make fuzz` with clang fuzzes the IMAP, POP3, SMTP and MIME readers).

`tests/hatari/` runs `EMAIL.PRG` itself in the [Hatari](https://hatari.tuxfamily.org/) emulator, with
Xvfb and xdotool, on the AERA family's test machines (`common/tools/machines.py list`: `ST`, `ST-1`,
`TT`, `Falcon`, each with its screens, as in `ST:low` or `Falcon:256`). Each test takes the machine
after its work folder, or runs on its usual one; the TOS is EmuTOS 1.4, fetched once:

```sh
make -C common/tools/fakesting
tests/hatari/net_test.py /tmp/mail-net             # Falcon, real servers (or: ... /tmp/mail-net ST)
tests/hatari/stop_test.py /tmp/mail-stop           # ST: Esc stops a server that never answers
tests/hatari/ui_tour.py WORKDIR                    # screenshots of the interface
tests/hatari/screenshots.py WORKDIR                # the pictures in docs/, English sample mail
tests/hatari/falcon_test.py /tmp/mail-tls          # a Direct connection: TLS.OVL and the DSP (or: ... TT)
tests/hatari/tlsovl_test.py /tmp/mail-ovl          # TLS.OVL missing (an alert), there (loaded), and on an ST
tests/hatari/badge_test.py /tmp/mail-badge         # the Hebrew keyboard's badge (F10): top right and Settings
make build/fakecontact.acc && tests/hatari/people_test.py /tmp/mail-pp   # names and ^B with (a stand-in for) EContact
tests/hatari/people_file_test.py /tmp/mail-pf      # EContact's CONTACTS.TXT without EContact (or: ... Falcon econtact)
make build/fakecal.acc && tests/hatari/invite_test.py /tmp/mail-inv   # an invitation to (a stand-in for) ECal
make build/fakeweb.acc build/fakeweb.prg && tests/hatari/link_test.py /tmp/mail-link Falcon:16   # links to (a stand-in for) a browser
make help build/fakehelp.acc && tests/hatari/help_test.py /tmp/mail-help   # the Help key asks (a stand-in for) EHypView
tests/hatari/langs_test.py /tmp/mail-langs         # Settings > Languages... and Translate into, against common's stand-in translategate
tests/hatari/emailchk_test.py /tmp/mail-chk        # EMAILCHK.ACC: new mail told at the desktop, none while EMail runs
                                                   # (Falcon; also ST-1 or TT, never ST or Falcon-4: no accessory
                                                   # is tested on the 4 MB machines, Erez, 4 Nov 2026)
AERA_WITH_NVDI=1 tests/hatari/font_test.py /tmp/mail-font   # a GDOS font under NVDI (Falcon, Falcon-4)
tests/hatari/net_test.py /tmp/mail-st ST-1         # the same EMAIL.PRG on a 1 MB ST, through the gateway
make build/trace/EMAIL.PRG && tests/hatari/scroll_test.py /tmp/mail-scroll   # what the panes draw while scrolling and choosing
make build/crashtest/EMAIL.PRG && common/tests/crash/app_crash.py /tmp/mail-crash ETOS.IMG build/crashtest/EMAIL.PRG EMail \
    --wait 30 --copy EMAIL.INF                     # F1, F2 and F5 crash on purpose: CRASH.LOG, the alert, the reset
                                                   # (an EMAIL.INF with an account: without one EMail waits in its
                                                   # first dialog; on an ST: add --machine st)
```

In `net_test.py` EMail logs in to Dovecot, mirrors the inbox, opens a
Hebrew message and sends a Hebrew reply; the test checks the reply and its copy in Sent on the server.
`disk_test.py` counts each job's file opens and lookups from Hatari's GEMDOS trace and fails above its
budget (the AERA guidelines, 6.5), and cuts a folder's file short to see the message fetched again.
`select_test.py`, `rename_test.py`, `drag_test.py` (messages dragged onto a folder), `tip_test.py` (a
tooltip over a name, subject or folder cut short, none where it fits), `find_test.py` (Find on disk as
you type, Esc, Search server for a word in a body, Undo and Ctrl+F to leave), `esc_test.py`
(Undo cancels every dialog, Esc empties a field and cancels a dialog without one and every alert), `field_test.py` (more typed into a field than it shows, and
all of it kept: the AERA guidelines, 5.4) and `colour_tour.py` run the same way. `font_test.py` chooses the
first GDOS font in Options > Font, checks the windows redraw in it and that it is saved, then starts EMail
again in it and reads a Hebrew message from disk; under NVDI it needs Erez's NVDI (`AERA_WITH_NVDI=1`,
`common/tools/machines.py`), and without a GDOS it checks the restart in the system font. EMail's tests run EMail alone (the AERA guidelines, 1.2): no other AERA
program is loaded, started or fetched for them, and where EMail talks to one a stand-in built here
plays the other side. `people_test.py` checks names and ^B with a stand-in for EContact
(`build/fakecontact.acc`), and `invite_test.py` an
invitation handed to a stand-in for ECal (`build/fakecal.acc`), and the alert when ECal isn't there.
`link_test.py` clicks the links in a plain and an HTML message: they go to a stand-in for a running
browser (`build/fakeweb.acc`), and without one EMail quits and starts the browser program
(`build/fakeweb.prg` as `C:\EWEB\EWEB.PRG`) on the link, a long one through `LINK.HTM`. `help_test.py`
checks that the Help key asks a stand-in for EHypView (`build/fakehelp.acc`) for `EMAIL.HYP`. EMail with
the real programs is the hub's family check. Hatari has no network card,
so `FAKESTNG.PRG` (the AERA apps' shared one, in `common/tools/fakesting`) stands in for STinG and
carries EMail's connections over the emulated serial port to `serial_bridge.py` next to it. It's for
the emulator only; never install it on a real Atari.
