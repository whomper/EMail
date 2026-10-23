# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# EMail - an email client for the Atari ST/STE/TT/Falcon, in C.
#
#   make            builds EMAIL.PRG and the ST edition EMAILST.PRG with a
#                   stock m68k ELF cross compiler
#                   (Debian/Ubuntu: apt install gcc-m68k-linux-gnu).
#                   No MiNTLib needed: the program is freestanding and
#                   common/tools/elf2tos.py writes the TOS header.
#   make test       builds the mail core for this computer and runs the
#                   unit tests (tests/); `make itest` also runs the
#                   protocol tests against a local Dovecot.
#   make help       binary/EMAIL.HYP, the guide for ST-Guide (needs hcp:
#                   common/tools/build-hcp.sh builds it)
#   make dist       a release's files in dist/: each file, and EMAIL-x.y.zip
#
# binary/ is what make builds, never committed: releases carry the
# program (.github/workflows/release.yml).
#
# The Atari build follows Claude ST (github.com/whomper/ClaudeST). The code
# EMail shares with the other AERA programs (BearSSL, the DSP RSA, bidi,
# dates, passwords, the libc headers, the build tools) is in common/, a git
# subtree of github.com/whomper/AERA-Atari: change it there, then
# tools/sync-common.sh.

CROSS   ?= m68k-linux-gnu-
CC      := $(CROSS)gcc
GCCINC  := $(shell $(CC) -print-file-name=include)
CFLAGS  := -m68000 -Os -fomit-frame-pointer -ffreestanding -fno-builtin \
           -fno-pic -fno-pie -fno-tree-loop-distribute-patterns -fno-store-merging -fdisable-tree-bswap \
           -fno-asynchronous-unwind-tables -Wall -Wno-pointer-sign \
           -DMAIL_TOS -nostdinc -isystem $(GCCINC) -Iatari/include -Icommon/atari/include \
           -Isrc -Icommon/src -Iatari -Icommon/atari -Iui -Icommon/ui -Icommon/third_party/bearssl/inc
CFLAGS  += $(TRACECF)
LDFLAGS := -m68000 -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld \
           -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments

# BearSSL 0.6 (common/third_party/bearssl) for Falcon mode. On the Atari it is
# built for the 68030: Falcon mode needs one, the rest of EMail stays
# 68000 code and runs on any ST.
BSSL    := common/third_party/bearssl
BSSLSRC := $(wildcard $(BSSL)/src/*/*.c)
BSSLDEF := -DBR_USE_URANDOM=0 -DBR_USE_GETENTROPY=0 -DBR_USE_UNIX_TIME=0 \
           -DBR_USE_WIN32_TIME=0 -DBR_USE_WIN32_RAND=0 -DBR_RDRAND=0
BSSLCF  := -m68030 -O2 -fomit-frame-pointer -ffreestanding -fno-builtin -fno-pic -fno-pie \
           -fno-tree-loop-distribute-patterns -fno-asynchronous-unwind-tables -w \
           -nostdinc -isystem $(GCCINC) -Iatari/include -Icommon/atari/include -I$(BSSL)/inc -I$(BSSL)/src $(BSSLDEF)
BSSLOBJ  = $(patsubst $(BSSL)/src/%.c,$(OBJDIR)/bssl/%.o,$(BSSLSRC))
HBSSLOBJ = $(patsubst $(BSSL)/src/%.c,$(OBJDIR)/hbssl/%.o,$(BSSLSRC))

CORE    := util charset mime links conn tls imap pop3 smtp store compose mail futil bidi secret ctime tz
ATARI   := libc tos plat_tos net_tos sting gem dsprsa crash bigbuf
UI      := main draw win folders list reader editor dialogs popup font fontsel about dsp theme help av alert mbar aboutbox iconify clip clipedit dd busy people_ask dialog field av_people people_file

OBJDIR  := build
OBJS    := $(OBJDIR)/crt0.o $(OBJDIR)/sting_s.o $(OBJDIR)/nf.o $(OBJDIR)/crash_s.o \
           $(addprefix $(OBJDIR)/,$(addsuffix .o,$(CORE) $(ATARI) $(UI)))
HDRS    := $(wildcard src/*.h atari/*.h atari/include/*.h ui/*.h \
           common/src/*.h common/atari/*.h common/atari/include/*.h common/ui/*.h common/dsp/*.h)

# the ST edition, EMAILST.PRG: no TLS of its own (common/src/tls_none.c),
# so no BearSSL and no DSP: about 115 KB less for a 1 MB ST. It reaches
# secure servers through the Pi gateway, as EMAIL.PRG does out of Falcon
# mode, with the same EMAIL.INF and folders.
STDIR   := $(OBJDIR)/st
STCORE  := $(filter-out tls,$(CORE)) tls_none
STATARI := $(filter-out dsprsa,$(ATARI))
STUI    := $(filter-out dsp,$(UI))
STOBJS  := $(OBJDIR)/crt0.o $(OBJDIR)/sting_s.o $(OBJDIR)/nf.o $(OBJDIR)/crash_s.o \
           $(addprefix $(STDIR)/,$(addsuffix .o,$(STCORE) $(STATARI) $(STUI)))
STCF    := $(CFLAGS) -DMAIL_ST -DTLS_FULL_EDITION=\"EMAIL.PRG\" -DNO_ICON256

# what goes onto the Atari: binary/ (built, never committed)
all: binary/EMAIL.PRG binary/EMAILST.PRG binary/ROOTS.DAT

binary/EMAIL.PRG: $(OBJDIR)/mail.elf common/tools/elf2tos.py | binary
	PRGFLAGS=7 python3 common/tools/elf2tos.py $< $@
binary/EMAILST.PRG: $(STDIR)/mail.elf common/tools/elf2tos.py | binary
	PRGFLAGS=7 python3 common/tools/elf2tos.py $< $@

# the version: VERSION holds the last release (major.minor, e.g. 1.30),
# and the build never writes it. Every build shows it as it is, in the
# About box and the User-Agent header. tools/bump.py starts a release.
VERSION_STR := $(shell cat VERSION)

# written again whenever the string changes
$(OBJDIR)/build.c: FORCE | $(OBJDIR)
	@printf '/* written by the Makefile: do not edit */\nconst char mail_version[] = "%s";\n' "$(VERSION_STR)" > $@.new
	@if cmp -s $@.new $@; then rm $@.new; else mv $@.new $@; fi
FORCE:
$(OBJDIR)/build.o: $(OBJDIR)/build.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/mail.elf: $(OBJS) $(OBJDIR)/build.o $(OBJDIR)/libbearssl.a common/atari/link.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(OBJDIR)/build.o $(OBJDIR)/libbearssl.a -lgcc
$(STDIR)/mail.elf: $(STOBJS) $(OBJDIR)/build.o common/atari/link.ld
	$(CC) $(LDFLAGS) -o $@ $(STOBJS) $(OBJDIR)/build.o -lgcc

# EMail's icon (the AERA guidelines, 5.5): drawn in common/icons/EMAIL.txt,
# made into icons/ (the desktop's RSC and ICN files), ui/icon.h and
# icon16.h (the About box, the iconified window) and docs/icon*.png;
# committed, and a release carries icons/
icons:
	python3 common/tools/appicon.py EMAIL --rsc icons/EMAIL.RSC --icn icons/EMAIL.ICN --mask icons/EMAILMK.ICN --headers ui --docs docs

$(OBJDIR)/bssl/%.o: $(BSSL)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(BSSLCF) -c -o $@ $<

$(OBJDIR)/libbearssl.a: $(BSSLOBJ)
	rm -f $@
	$(CROSS)ar rcs $@ $^

$(OBJDIR)/%.o: src/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
$(OBJDIR)/%.o: atari/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
$(OBJDIR)/%.o: ui/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
$(OBJDIR)/%.o: common/src/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
$(OBJDIR)/%.o: common/atari/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
$(OBJDIR)/%.o: common/ui/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
$(STDIR)/%.o: src/%.c $(HDRS) | $(STDIR)
	$(CC) $(STCF) -c -o $@ $<
$(STDIR)/%.o: atari/%.c $(HDRS) | $(STDIR)
	$(CC) $(STCF) -c -o $@ $<
$(STDIR)/%.o: ui/%.c $(HDRS) | $(STDIR)
	$(CC) $(STCF) -c -o $@ $<
$(STDIR)/%.o: common/src/%.c $(HDRS) | $(STDIR)
	$(CC) $(STCF) -c -o $@ $<
$(STDIR)/%.o: common/atari/%.c $(HDRS) | $(STDIR)
	$(CC) $(STCF) -c -o $@ $<
$(STDIR)/%.o: common/ui/%.c $(HDRS) | $(STDIR)
	$(CC) $(STCF) -c -o $@ $<
# the stack: 32 KB instead of crt0's 48. The most EMail was seen to use,
# with the stack painted in falcon_test (RSA on the 68030), net_test,
# stop_test and a 1.4 MB message, is 15 KB; the 16 KB saved help a 1 MB ST
$(OBJDIR)/crt0.o: common/atari/crt0.S | $(OBJDIR)
	$(CC) -m68000 -DSTACK_SIZE=32768 -c -o $@ $<
$(OBJDIR)/sting_s.o: common/atari/sting.S | $(OBJDIR)
	$(CC) -m68000 -c -o $@ $<
$(OBJDIR)/crash_s.o: common/atari/crash.S | $(OBJDIR)
	$(CC) -m68000 -c -o $@ $<
$(OBJDIR)/nf.o: common/atari/nf.S | $(OBJDIR)
	$(CC) -m68000 -c -o $@ $<

# EMail saying on the emulator's console what each pane draws ("DRAW
# list y h", "COPY list ..."), for tests/hatari/scroll_test.py (never
# released): its own objects, so binary/ is left as it is
$(OBJDIR)/trace/EMAIL.PRG: FORCE
	$(MAKE) OBJDIR=$(OBJDIR)/trace TRACECF=-DDRAW_TRACE $(OBJDIR)/trace/mail.elf
	PRGFLAGS=7 python3 common/tools/elf2tos.py $(OBJDIR)/trace/mail.elf $@
$(OBJDIR)/trace/EMAILST.PRG: FORCE
	$(MAKE) OBJDIR=$(OBJDIR)/trace TRACECF=-DDRAW_TRACE $(OBJDIR)/trace/st/mail.elf
	PRGFLAGS=7 python3 common/tools/elf2tos.py $(OBJDIR)/trace/st/mail.elf $@

# EMail crashing on purpose with F1 to F5 (-DCRASH_TEST), for
# common/tests/crash/app_crash.py (never released)
$(OBJDIR)/crashtest/EMAIL.PRG: FORCE
	$(MAKE) OBJDIR=$(OBJDIR)/crashtest TRACECF=-DCRASH_TEST $(OBJDIR)/crashtest/mail.elf
	PRGFLAGS=7 python3 common/tools/elf2tos.py $(OBJDIR)/crashtest/mail.elf $@
$(OBJDIR)/crashtest/EMAILST.PRG: FORCE
	$(MAKE) OBJDIR=$(OBJDIR)/crashtest TRACECF=-DCRASH_TEST $(OBJDIR)/crashtest/st/mail.elf
	PRGFLAGS=7 python3 common/tools/elf2tos.py $(OBJDIR)/crashtest/st/mail.elf $@

# a stand-in for EContact's answering side, for tests/hatari/people_test.py
# (never released)
FAKEC   := crt0 gem libc tos av av_people people_ask
$(OBJDIR)/fakecontact.acc: tests/atari/fakecontact.c $(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKEC))) $(OBJDIR)/nf.o
	$(CC) $(CFLAGS) -c -o $(OBJDIR)/fakecontact.o $<
	$(CC) $(LDFLAGS) -o $(OBJDIR)/fakecontact.elf $(OBJDIR)/fakecontact.o \
		$(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKEC))) $(OBJDIR)/nf.o -lgcc
	python3 common/tools/elf2tos.py $(OBJDIR)/fakecontact.elf $@

# a stand-in for ECal's receiving side, for tests/hatari/invite_test.py
# (never released)
FAKECAL := crt0 gem libc tos av av_people
$(OBJDIR)/fakecal.acc: tests/atari/fakecal.c $(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKECAL))) $(OBJDIR)/nf.o
	$(CC) $(CFLAGS) -c -o $(OBJDIR)/fakecal.o $<
	$(CC) $(LDFLAGS) -o $(OBJDIR)/fakecal.elf $(OBJDIR)/fakecal.o \
		$(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKECAL))) $(OBJDIR)/nf.o -lgcc
	python3 common/tools/elf2tos.py $(OBJDIR)/fakecal.elf $@

# a stand-in for a web browser (EWeb's VA_START), for tests/hatari/link_test.py
# (never released)
$(OBJDIR)/fakeweb.acc: tests/atari/fakeweb.c $(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKECAL))) $(OBJDIR)/nf.o
	$(CC) $(CFLAGS) -c -o $(OBJDIR)/fakeweb.o $<
	$(CC) $(LDFLAGS) -o $(OBJDIR)/fakeweb.elf $(OBJDIR)/fakeweb.o \
		$(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKECAL))) $(OBJDIR)/nf.o -lgcc
	python3 common/tools/elf2tos.py $(OBJDIR)/fakeweb.elf $@

$(OBJDIR) $(STDIR) binary:
	mkdir -p $@

# ---- host build: the same mail core, for tests and mail-cli ----
HOSTCC     ?= cc
HOSTCFLAGS := -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare \
              -Wno-format-truncation -Isrc -Icommon/src -fsanitize=address,undefined
# each core file is the app's own (src/) or shared (common/src/)
HOSTSRC    := $(foreach m,$(CORE),$(firstword $(wildcard src/$(m).c common/src/$(m).c))) \
              tests/host/plat_posix.c tests/host/version.c
HOSTLIB    := $(OBJDIR)/libbearssl-host.a

$(OBJDIR)/hbssl/%.o: $(BSSL)/src/%.c
	@mkdir -p $(dir $@)
	$(HOSTCC) -O2 -w -I$(BSSL)/inc -I$(BSSL)/src -c -o $@ $<

$(HOSTLIB): $(HBSSLOBJ)
	rm -f $@
	ar rcs $@ $^

$(OBJDIR)/mail-cli: tests/host/mail_cli.c $(HOSTSRC) $(HDRS) $(HOSTLIB) | $(OBJDIR)
	$(HOSTCC) $(HOSTCFLAGS) -I$(BSSL)/inc -o $@ tests/host/mail_cli.c $(HOSTSRC) $(HOSTLIB)

$(OBJDIR)/unit: tests/unit.c $(HOSTSRC) $(HDRS) $(HOSTLIB) | $(OBJDIR)
	$(HOSTCC) $(HOSTCFLAGS) -I$(BSSL)/inc -o $@ tests/unit.c $(HOSTSRC) $(HOSTLIB)

# the root certificates, decoded ahead of time for Falcon mode
$(OBJDIR)/mkroots: common/tools/mkroots.c $(HOSTSRC) $(HDRS) $(HOSTLIB) | $(OBJDIR)
	$(HOSTCC) $(HOSTCFLAGS) -I$(BSSL)/inc -o $@ common/tools/mkroots.c $(HOSTSRC) $(HOSTLIB)

# (Mozilla's list, kept in common/, next to the program)
binary/CACERT.PEM: common/certs/CACERT.PEM | binary
	cp $< $@
binary/ROOTS.DAT: binary/CACERT.PEM $(OBJDIR)/mkroots
	$(OBJDIR)/mkroots binary/CACERT.PEM $@

# ---- fuzzing (the AERA guidelines, 7.1): what servers send, from
# libFuzzer, with AddressSanitizer and UBSan. The network is
# common/tests/fuzz/fuzz_net.c: the server says the fuzz input. Needs
# clang with libFuzzer (Debian/Ubuntu: clang, libclang-rt-dev); make fuzz
# runs each target FUZZTIME seconds from tests/fuzz/seeds (mkseeds.py)
FUZZCC    ?= clang
FUZZTIME  ?= 60
FUZZERS   := imap pop3 smtp mime
FUZZFLAGS := -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -DFUZZ_NET \
             -Isrc -Icommon/src -Icommon/tests/fuzz -I$(BSSL)/inc

$(OBJDIR)/fuzz_%: tests/fuzz/fuzz_%.c $(HOSTSRC) $(HDRS) $(HOSTLIB) common/tests/fuzz/fuzz_net.c | $(OBJDIR)
	$(FUZZCC) $(FUZZFLAGS) -o $@ $< $(HOSTSRC) common/tests/fuzz/fuzz_net.c $(HOSTLIB)

fuzz: $(addprefix $(OBJDIR)/fuzz_,$(FUZZERS))
	@for f in $(FUZZERS); do \
	  mkdir -p $(OBJDIR)/corpus/$$f; \
	  echo "fuzz_$$f: $(FUZZTIME) s"; \
	  $(OBJDIR)/fuzz_$$f -max_total_time=$(FUZZTIME) -timeout=10 -rss_limit_mb=1024 \
	    -artifact_prefix=$(OBJDIR)/crash-$$f- $(OBJDIR)/corpus/$$f tests/fuzz/seeds/$$f \
	    $$(ls -d tests/fuzz/found/$$f 2>/dev/null) \
	    > $(OBJDIR)/fuzz_$$f.log 2>&1 || { tail -40 $(OBJDIR)/fuzz_$$f.log; exit 1; }; \
	  grep -E "^#[0-9]+.*DONE" $(OBJDIR)/fuzz_$$f.log | tail -1; \
	done

# the same targets without libFuzzer (common/tests/fuzz/replay.c), with
# this computer's compiler: make test runs the seeds again, and the inputs
# that once broke a parser (tests/fuzz/found)
REPLAYFLAGS := $(HOSTCFLAGS) -fno-sanitize-recover=undefined -DFUZZ_NET -Icommon/tests/fuzz -I$(BSSL)/inc
REPLAYSRC   := common/tests/fuzz/fuzz_net.c common/tests/fuzz/replay.c

$(OBJDIR)/replay_%: tests/fuzz/fuzz_%.c $(HOSTSRC) $(HDRS) $(HOSTLIB) $(REPLAYSRC) | $(OBJDIR)
	$(HOSTCC) $(REPLAYFLAGS) -o $@ $< $(HOSTSRC) $(REPLAYSRC) $(HOSTLIB)

test: $(OBJDIR)/unit $(addprefix $(OBJDIR)/replay_,$(FUZZERS))
	$(OBJDIR)/unit
	@for f in $(FUZZERS); do \
	  $(OBJDIR)/replay_$$f tests/fuzz/seeds/$$f $$(ls -d tests/fuzz/found/$$f 2>/dev/null) || exit 1; \
	done
	python3 -m unittest discover -s gateway -p 'test_aeragate.py'
	python3 -m unittest discover -s gateway -p 'test_emailgate.py'

itest: $(OBJDIR)/mail-cli
	tests/integration.sh

# the ST-Guide help file from the user guide (HCP from github.com/th-otto/hypview)
HCP ?= hcp
# (and EMAIL.REF, its page names for ST-Guide's reference list)
help: binary/EMAIL.HYP
binary/EMAIL.HYP: docs/GUIDE.md tools/mkhelp.py VERSION docs/front.json docs/help/WINDOW.IMG \
                  common/help/EMAIL.IMG common/tools/helpfront.py common/tools/gemimg.py | $(OBJDIR) binary
	python3 tools/mkhelp.py docs/GUIDE.md VERSION $(OBJDIR)/EMAIL.STG
	python3 common/tools/helpfront.py page docs/front.json $(OBJDIR)/EMAIL.STG $(OBJDIR)/EMAIL.STG
	$(HCP) --references -o $@ $(OBJDIR)/EMAIL.STG >/dev/null

# a release's files: each one in dist/, and all of them in
# dist/EMAIL-x.y.zip (the icons in icons/), for the release workflow
DIST    := binary/EMAIL.PRG binary/EMAILST.PRG binary/CACERT.PEM binary/ROOTS.DAT binary/EMAIL.HYP binary/EMAIL.REF
ICONS   := icons/EMAIL.RSC icons/EMAIL.ICN icons/EMAILMK.ICN
dist: all help
	rm -rf dist
	mkdir -p dist/zip/icons
	cp $(DIST) $(ICONS) dist/
	cp $(DIST) dist/zip/
	cp $(ICONS) dist/zip/icons/
	cd dist/zip && zip -q -9 -r ../EMAIL-$(VERSION_STR).zip .
	rm -rf dist/zip

clean:
	rm -rf $(OBJDIR) binary dist

.PHONY: help all icons dist test itest clean fuzz FORCE
