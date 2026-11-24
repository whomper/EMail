# Notes for working on EMail

EMail is one of the AERA Atari apps (ECal, EContact, EMail, EFTP, EHypView, ClaudeST). They are meant to
look and behave as one family.

## For every AERA app

These come from AERA-Atari's repo template and are the same in every app's CLAUDE.md.

- **Follow the AERA Atari app guidelines** ([ATARI-APP-GUIDELINES.md](https://github.com/whomper/AERA-Atari/blob/main/standards/ATARI-APP-GUIDELINES.md)). Pay most
  attention to:
  - About box
  - family shortcuts
  - PRG/ACC rules
  - context menus
  - VA_START, help, alerts
  - INF location

  When a change touches one of these, do it the guideline's way, even if the old code here did it
  differently.
- **Speed is set by the stock machines** (decided 26 Oct 2026; the guidelines' "Speed"): an 8 MHz ST
  and a stock Falcon (`ST`, `ST-1`, `Falcon-4`), not the DFB1. Design and code for them: draw only
  what changed, keep the 68000's slow operations out of inner loops, do work once. A change to
  drawing, a loop over the data or the start gives its `AESGAP` stretches on `ST` and `Falcon-4`,
  before and after, in its commit message. A GEM release passes the speed rig within the
  guidelines' times.
- **Shared code lives in [AERA-Atari](https://github.com/whomper/AERA-Atari).** Files under `common/` come from it. Change them
  there, not here, and pull them in with `tools/sync-common.sh`. A fix made only in this repo will drift
  again.
- **Build and test before every push:** `make` with no warnings and `make test`, plus this repo's
  own checks (below): `make itest` when network code changed, the Hatari rigs in `tests/hatari/` when
  the GEM side changed.
- **Versions and releases:** `VERSION` changes only in a release commit. A release: `tools/bump.py`,
  commit, push, then run the Release workflow on `main` with the notes (or push a tag `vX.Y` made with
  `git tag -a`); it tests, builds, tags vX.Y and publishes each file and the zip as a
  GitHub Release. A normal build must leave every tracked file unchanged and shows the version in `VERSION` as it is (there is no `-dev` label). Don't
  commit built programs, help files or certificates (`binary/`, `dist/`). Never reuse a number.
- **Only formal releases leave the build machine.** The owner gets vX.Y releases made by the Release workflow from `main`, nothing else: no test builds, no CI artifacts, no builds from a branch. Work lands on `main`; to try a fix on real hardware, release it. If a release doesn't work, fix
  forward: a new release with the next number, before any other work; never a rollback (decided 7 Nov 2026).
- **Releases go out without asking the owner** (decided 23 Oct 2026; guidelines 1.2), once the candidate has
  passed **the regression gate** (decided 17 Nov 2026: "Since we are now at an evolved phase, we need to be extra
  careful with regression issues as there are users of our software. This means testing across all
  configurations" and "make sure we don't loose performance"; guidelines 1.2, AERA-Atari a22574e). In order:
  1. this repo's own checks;
  2. EMail's rigs on every row of the gate table at the top of AERA-Atari's `standards/TEST-MATRIX.md` it runs
     on, on real TOS images and NVDI (`~/.aera`; EmuTOS is no evidence for a release): the reference configuration,
     Falcon-4:16 without NVDI, Falcon true colour, TT, MiNT+XaAES (plain, ct60, ext2), a 68060, the FireBee
     stand-in, ST mono and colour and ST-1, STE, EMail check on first and second boot, each network path
     (plain, the Gateway, Direct) and one boot with NatFeats off (`natoff_test.py`). Each row named in the
     report with `describe()`; a row it can't run, named with why;
  3. the paired speed run against the last release on the reference configuration, `Falcon-4:16` and `ST:mono`, the
     timed build at the shipped layout (`common/tools/profpad.py`, `make build/speed/email.prg`): no row slower
     beyond a tick on any; a slower row is fixed first, and only the owner accepts one;
  4. the candidate commit and the results to the hub, which runs the family check and answers go or the
     faults it found;
  5. only then the release: give the owner the link in one line, with anything to do, and send the hub
     the link.

  A fault a user reports becomes a case in the rigs, in the same release that fixes it. Decisions that are
  the owner's still go to the owner first.
- **Who decides what** (decided 31 Oct 2026; the guidelines' "Who decides what"). The owner's part, put
  to the owner before the work as a decision with a short context:
  - new programs and ideas, their core capabilities, and how they work with the other programs;
  - the user experience: the look, the layout, the way through a task, the words, the icons, the
    defaults;
  - phases: work stays inside the approved phase; at its end, one phase report (what it set out
    to do, what was done with the release links, what is left, the next phase as a proposal), and
    the next phase starts when the owner says so;
  - priorities and conflicts between programs or sessions (through the hub, with its options and
    recommendation);
  - packages on the Pi and ports.

  How it is made is the team's, without asking: code, structure, shared code, tests, measurements,
  fixes, docs and releases inside an approved phase.
- **No AERA app tests itself with another AERA program or accessory** (decided 8 Nov 2026; guidelines
  1.2, AERA-Atari 76417f7): EMail's own tests run EMail alone, with nothing else AERA loaded beside it,
  started by it or fetched for it. Where EMail talks to another program, a stand-in built here plays
  the other side (`tests/atari/`). EMAIL.PRG with EMAILCHK.ACC is EMail alone. EMail with the real
  programs is the hub's family check.
- **Every new source file starts with the SPDX line and copyright** (see any `.c` file); a generator
  writes them into the files it makes.
- **The name** reads "AERA | Erez Yaary" wherever it is given: copyright lines
  (`Copyright (C) 2026 AERA | Erez Yaary`), the About box, the help file's `@author`, the docs, and as
  the author of every commit: `git commit --author="AERA | Erez Yaary <yaary@me.com>"`, with Claude as
  the signing committer, so GitHub shows the commits as Verified (AERA-Atari,
  `standards/GITHUB-IDENTITY.md`).
- **Never commit:**
  - real addresses from the owner's network: use `192.168.1.x`, `PI_IP`, `ATARI_IP`
  - passwords, session keys, `.KEY` / `.INF` files from a test run
  - photos that still carry EXIF/GPS data
- **README:** keep the section order from the template (Highlights, Getting started, How it reaches the
  servers, Building, Source, Tests, Licence, family footer). Use British English.

## EMail

- **Talking with the owner** (the family's rule): no progress or
  in-between messages. Write when there is something to decide: a
  short context and the decision. Findings: the information, your
  assessment and what to do, nothing more. Messages to the hub
  stay complete, with the evidence.
- **Docs-only changes** (README, docs/, CLAUDE.md) take the short path
  (guidelines 1.2): push once they read right and `make help` still
  builds the help file with the version; no rigs, itest or real-TOS run.
  A change to the docs alone needs no release of its own: it goes out
  with the next one.
- **A release** carries `EMAIL.PRG` (the ST, STE, TT and Falcon), `EMAILCHK.ACC` (EMail check, the
  new-mail accessory: `ui/emailchk.c`, `src/newmail.c`), `TLS.OVL` (common's, copied as it
  is, never built here: guidelines 7.4), `EMAIL.HYP` and `.REF` (hcp from `common/tools/build-hcp.sh`),
  `CACERT.PEM`, `ROOTS.DAT` and the icon, in `email-x.y.zip`; and `email-x.y-firebee.zip`, the
  FireBee package with the same names inside: nothing else, no loose files (decided 22 Nov 2026;
  guidelines 7.4). `EMAIL.PRG` and `EMAILCHK.ACC` built for the ColdFire by
  `common/tools/mkfirebee.py` (`binary/firebee/`), common's `tlsfb.ovl` as `TLS.OVL`, each
  package's alert naming the other (`make cftest` and `tests/hatari/firebee_test.py` when they
  change). The icons are committed in `icons/` (`make icons`).
- The help file's front page (guidelines 4.3) is `docs/front.json`, laid out by
  `common/tools/helpfront.py`. Its window picture `docs/help/WINDOW.IMG` is committed: when
  `docs/screenshot.png` changes, make it again with
  `python3 common/tools/helpfront.py thumb docs/screenshot.png docs/help/WINDOW.IMG 0,0,600,336 118,122 448,90 560,264`.
- Before pushing: `make test` (the core, the fuzz inputs again, the
  gateway), `make itest` (against Dovecot: dovecot-imapd, dovecot-pop3d;
  also `TLS=1` and `FALCON=1` when the connection code changes), and
  `make` with no warnings. When the GEM side changes,
  `tests/hatari/net_test.py`, `tests/hatari/stop_test.py` and
  `tests/hatari/disk_test.py` (each job's opens within its budget,
  guidelines 6.5) in Hatari with EmuTOS, and `tests/hatari/xaaes_test.py`
  (FreeMiNT with XaAES, a multitasking AES: common's rig in
  `common/tests/xaaes`, `make base` there once); `emailchk_test.py` (on `ST-1`, `TT` or `Falcon`: no accessory
  runs on the 4 MB machines, `ST` and `Falcon-4`; decided 4 Nov 2026) when the
  accessory or `src/newmail.c` changes. With EContact: `people_test.py` (its stand-in);
  the Help key: `help_test.py` (EHypView's stand-in).
- `gateway/` is a read-only copy of
  [AERA-Gateway](https://github.com/whomper/AERA-Gateway)'s (emailgate,
  the other gateways, `aera-gateway`): change it there, with that
  repository's owner session, and bring it here with
  `tools/sync-gateway.sh`. EMail decides emailgate's protocol (STARTTLS,
  the "emailgate:" refusal lines, ports 143/587/110): review changes to
  it there. Its log names who connected where, never what went through.
  EMail tests emailgate itself (decided 8 Nov 2026; guidelines 1.2, AERA-Atari
  4169af2): every change to it is run here from the `gateway/` copy (`make
  itest`, the rigs through the gateway), and those tests are EMail's.
- EContact is EMail's address book through the family's PEOPLE messages
  (`common/ui/av_people.c`, `people_ask.c`); EContact has no patch for
  EMail any more. Check it with `people_test.py` and its stand-in
  (`build/fakecontact.acc`).
- **A candidate's test round** is EMail's section of AERA-Atari's
  `standards/TEST-MATRIX.md` (hub 5e0039a, 7 Nov 2026):
  - `net_test.py` on the reference configuration, `Falcon-4:256` with `AERA_WITH_NVDI=1` (decided 10 Nov 2026;
    AERA-Atari 148df17, `machines.ERZ_MACHINE`): it replaces the Falcon:256 run and the plain Falcon
    (16-colour) net run, at the next release EMail makes anyway (the rig change goes with that commit).
    Without TOS images or NVDI here it runs on EmuTOS and is reported "stand-in, not the reference configuration";
  - `emailchk_test.py` on ST-1 only, with the KB-free reading in the same boot (the Falcon monthly);
  - no `drag_test.py` on ST:mono, no ST:med badge run;
  - `people_file_test.py` only when the people code changes;
  - the speed rig as one interleaved base/candidate pair, with a second and third pair only for a row
    the first flags;
  - the NVDI rows (redraw, speed, `font_test.py`) are the hub's;
  - at least 60% of a candidate's boots run under NVDI (decided 7 Nov 2026; guidelines 1.2), with
    the ST boots counted, though TOS 1.04 can't run NVDI: the Falcon and TT rigs with
    `AERA_WITH_NVDI=1` once `~/.aera/nvdi/x` is in the session, and one run without NVDI.

  When those rigs are next touched, trim `disk_test.py`'s mailbox and merge select, esc, field,
  find, rename, scroll and tip into one boot.
- **The interface backlog** (the UI decisions of 9 Nov 2026, AERA-Atari `standards/ui`) is
  [docs/UI-BACKLOG.md](docs/UI-BACKLOG.md): taken only with a release EMail makes anyway, as it says.

## Maintenance mode (decided 1 Nov 2026; changed 17 Nov 2026)

EMail is in maintenance mode while EWeb, EPDF and EPlay go to 1.0 (AERA-Atari, ATARI-APP-GUIDELINES.md,
section 8; AERA-Atari 8ddb463). Until the owner ends it:
- **Lifted for one item** (decided 24 Nov 2026): the message list's short dates, today "18:20",
  this year "09.10", older "09.10.25", the column 9 wide and the 6 characters to From and Subject;
  the message's header keeps the full date. Its own release, through the regression gate with the
  pairs against 2.28 and the hub's family check; `tests/hatari/dates_test.py` reads the three forms
  back from the screen. Maintenance comes back once it is released.
- Decided 19 Nov 2026 (the redraw rule, released in 2.25): the whole main window on an ST is drawn
  whole (380-480 ms; pieces of 100 ms cost 80-115 % more there; now the family's rule); Settings
  on an ST takes 330 ms (the AES draws it in one piece); the message pane's slider follows a scroll
  a turn later; D51 stays "Open | Later".
- No new features and no speed rounds of its own.
- **Every family rule holds for EMail as for the focus apps** (decided 17 Nov 2026: "there needs to be a
  constant behavior across all applications. In the future, all applications must adhere to family
  rules and be notified. We will review whether they need to issue a release." and "Applications on
  maintenance does not mean they contain faults"). The hub notifies EMail of each new or changed rule
  and each shared fix; EMail adopts it, with the shared code it needs (`tools/sync-common.sh`), and
  its row in section 8's table is ticked. EMail's ✗, ~ and ? there are EMail's work.
- Whether a rule or a shared fix needs a release of its own is reviewed with the owner through the hub; a
  fault fix always gets one.
- Otherwise a release only for:
  - a security or data-loss fault;
  - a crash or breakage;
  - something outside that stops it working (a certificate, a server);
  - something EWeb, EPDF or EPlay need from it.

  Its notes say why.
- Nothing runs in the background; the session waits for the owner or the hub.
- Only the owner's explicit word ends this mode. The hub passes it on, quoted.
