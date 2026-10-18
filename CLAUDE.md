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
- **Only formal releases leave the build machine.** Erez gets vX.Y releases made by the Release workflow from `main`, nothing else: no test builds, no CI artifacts, no builds from a branch. Work lands on `main`; to try a fix on real hardware, release it. If a release doesn't work, roll back with a release: revert on `main`, release the next number, and say in the bad release's notes that it is withdrawn and which one to use.
- **Testing with another AERA program uses its latest release**, what Erez runs, never a build of its
  `main`: `common/tools/getrelease.py APP DIR FILE...` fetches it. Only for a change both programs are
  still making, test against the other's `main` at a named commit, say so, and test again against its
  release when it is out.
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

- **Talking with Erez** (his rule for the family): no progress or
  in-between messages. Write to him when he has to decide something: a
  short context and the decision. Findings: the information, your
  assessment and what he should do, nothing more. Messages to the hub
  stay complete, with the evidence.
- **Docs-only changes** (README, docs/, CLAUDE.md) take the short path
  (guidelines 1.2): push once they read right and `make help` still
  builds the help file with the version; no rigs, itest or real-TOS run.
  A docs-only release still needs Erez's yes.
- **A release** carries `EMAIL.PRG`, `EMAIL.HYP` and `.REF` (hcp from `common/tools/build-hcp.sh`),
  `CACERT.PEM`, `ROOTS.DAT` and the icon. The icons are committed in `icons/` (`make icons`).
- The help file's front page (guidelines 4.3) is `docs/front.json`, laid out by
  `common/tools/helpfront.py`. Its window picture `docs/help/WINDOW.IMG` is committed: when
  `docs/screenshot.png` changes, make it again with
  `python3 common/tools/helpfront.py thumb docs/screenshot.png docs/help/WINDOW.IMG 0,0,600,336 118,122 448,90 560,264`.
- Before pushing: `make test` (the core, the fuzz inputs again, the
  gateway), `make itest` (against Dovecot: dovecot-imapd, dovecot-pop3d;
  also `TLS=1` and `FALCON=1` when the connection code changes), and
  `make` with no warnings. When the GEM side changes,
  `tests/hatari/net_test.py` and `tests/hatari/stop_test.py` in Hatari
  with EmuTOS. With EContact: `econtact_test.py` (its latest release);
  the help file: `help_test.py` (EHypView's latest release).
- `gateway/` is a read-only copy of
  [AERA-Gateway](https://github.com/whomper/AERA-Gateway)'s (emailgate,
  the other gateways, `aera-gateway`): change it there, with that
  repository's owner session, and bring it here with
  `tools/sync-gateway.sh`. EMail decides emailgate's protocol (STARTTLS,
  the "emailgate:" refusal lines, ports 143/587/110): review changes to
  it there. Its log names who connected where, never what went through.
- EContact is EMail's address book through the family's PEOPLE messages
  (`common/ui/av_people.c`, `people_ask.c`); EContact has no patch for
  EMail any more. Check it with `econtact_test.py` against EContact's
  latest release.
