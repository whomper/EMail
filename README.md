# EMail

<img src="docs/icon.png" width="96" align="right" alt="EMail icon: a letter coming out of an open blue envelope">

An e-mail program for the Atari ST, STE, TT and Falcon, written in C for GEM.

**Highlights**

- **IMAP and POP3 in, SMTP out**, with an Outbox for sending later and copies in Sent.
- **Secure on any Atari:** through AERA-Gateway, or a Direct connection with TLS 1.2 on a Falcon or TT.
- **Hebrew throughout:** right-to-left layout, the Israeli keyboard on F10, Hebrew in vector fonts.
- **Works offline:** your folders mirrored on disk; read, flag and move, and the server catches up later.
- **Find** narrows the list as you type; **Search server** looks through the messages' text.
- **Many messages at once:** mark, flag, move or delete a selection together.
- **Translate** messages, or a few chosen words, on your own network.
- **People by name** from EContact, and calendar invitations straight into ECal.
- **EMail check**, a small accessory, tells you about new mail while EMail isn't running.
- **Any Atari:** TOS 1.04 to 4.x, EmuTOS, MagiC and MiNT, from a 1 MB ST to a Falcon.

![EMail on a Falcon in 256 colours: folders, the inbox and a message with its quoted reply in colour](docs/screenshot.png)

## Getting started

**Needs:** TOS 1.04 or newer (TOS 4 on the Falcon), EmuTOS, MagiC or MiNT; 640×200 or larger; 1 MB of
memory (EMail takes about 332 KB); STinG, STiK, MiNTnet or MagiC-Net; and for encrypted mail (nearly all
providers) the Gateway or a Direct connection (a Falcon or TT with 4 MB).

Download the files from the **[latest release](https://github.com/whomper/EMail/releases/latest)**, one by
one or all together in `EMAIL-x.y.zip`:

| File | What it is |
|---|---|
| `EMAIL.PRG` | the program, for every Atari |
| `EMAIL.HYP`, `EMAIL.REF` | the user guide, opened by the Help key |
| `TLS.OVL`, `CACERT.PEM`, `ROOTS.DAT` | a Direct connection on a TT or Falcon: TLS and the root certificates |
| `EMAILCHK.ACC` | EMail check, the new-mail accessory (optional) |
| `EMAIL.RSC`, `EMAILNV.RSC`, `*.ICN` | the desktop icon ([which one](docs/GUIDE.md#the-desktop-icon)) |

1. Copy `EMAIL.PRG`, `EMAIL.HYP` and `EMAIL.REF` to a folder of their own, such as `C:\EMAIL\`.
2. Set up [AERA-Gateway](https://github.com/whomper/AERA-Gateway) on a Raspberry Pi with your
   provider's servers. Or, on a Falcon or TT, copy `TLS.OVL`, `CACERT.PEM` and `ROOTS.DAT` beside
   `EMAIL.PRG` and switch on **Options > Direct connection**.
3. Start EMail and fill in your account in **Options > Accounts**. Gmail, iCloud, Yahoo and Fastmail need
   an [app password](docs/GUIDE.md#app-passwords-gmail-icloud-yahoo-fastmail).
4. Press **Help** for the [user guide](docs/GUIDE.md).

To update, copy the new files over the old ones. Every step and tip: [docs/DETAILS.md](docs/DETAILS.md).

## How it reaches the servers

Mail providers want encrypted connections, which an ST can't make at a usable speed.
[AERA-Gateway](https://github.com/whomper/AERA-Gateway), a Raspberry Pi on your home network, does that
for any Atari: EMail talks plain IMAP, POP3 and SMTP to it, and it reaches your provider over TLS. It also
translates. On a Falcon or TT it is optional: a Direct
connection does TLS on the Atari itself. Setting it up, every provider and option: the
[Gateway's README](https://github.com/whomper/AERA-Gateway#readme); speeds:
[docs/DETAILS.md](docs/DETAILS.md#performance-gateway-68030-and-dsp).

## Building

```sh
sudo apt install gcc-m68k-linux-gnu python3   # no MiNTLib needed
make                                          # binary/: EMAIL.PRG, EMAILCHK.ACC and the rest
make help HCP=/path/to/hcp                    # the help file from docs/GUIDE.md
make dist HCP=/path/to/hcp                    # the release files and the zip
```

Releases, `TLS.OVL` and the DSP code: [docs/DEVELOPING.md](docs/DEVELOPING.md).

## Source

```
src/      the mail core: IMAP, POP3, SMTP, MIME, the local store
ui/       the GEM program, and EMail check (emailchk.c)
common/   code shared with the other AERA programs, from AERA-Atari
tests/    unit, fuzz and integration tests, the Hatari rigs
```

The other folders, and every test and rig: [docs/DEVELOPING.md](docs/DEVELOPING.md).

## Tests

`make test` runs the unit and fuzz tests and the gateway's; the rest is in [docs/DEVELOPING.md](docs/DEVELOPING.md#tests).

## Licence

GPL 2 or later: see [COPYING](COPYING). Copyright (C) 2026 AERA | Erez Yaary. It includes
[BearSSL](common/third_party/bearssl) (MIT) and Mozilla's root certificates (MPL 2.0).

---

Part of **AERA** for Atari: [ECal](https://github.com/whomper/ECal) · [EContact](https://github.com/whomper/EContact) · [EMail](https://github.com/whomper/EMail) · [EFTP](https://github.com/whomper/EFTP) · [EHypView](https://github.com/whomper/EHypView) · [ClaudeST](https://github.com/whomper/ClaudeST) · [EWeb](https://github.com/whomper/EWeb) · [EUpdate](https://github.com/whomper/EUpdate) · 8-bit: [EDOS](https://github.com/whomper/EDOS) · [EFTP_XL](https://github.com/whomper/EFTP_XL) · Gateway: [AERA-Gateway](https://github.com/whomper/AERA-Gateway)
