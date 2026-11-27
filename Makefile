# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# EMail - an email client for the Atari ST/STE/TT/Falcon, in C.
#
#   make            builds EMAIL.PRG, for every Atari, with a stock m68k
#                   ELF cross compiler
#                   (Debian/Ubuntu: apt install gcc-m68k-linux-gnu).
#                   No MiNTLib needed: the program is freestanding and
#                   common/tools/elf2tos.py writes the TOS header.
#   make test       builds the mail core for this computer and runs the
#                   unit tests (tests/); `make itest` also runs the
#                   protocol tests against a local Dovecot.
#   make help       binary/email.hyp, the guide for ST-Guide (needs hcp:
#                   common/tools/build-hcp.sh builds it)
#   make dist       a release's files in dist/: each file, and email-x.y.zip
#
# binary/ is what make builds, never committed: releases carry the
# program (.github/workflows/release.yml).
#
# The Atari build follows Claude ST (github.com/whomper/ClaudeST). The code
# EMail shares with the other AERA programs (TLS.OVL, bidi, dates,
# passwords, the libc headers, the build tools) is in common/, a git
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
# common's dialogs drawn by the CPU (-DDLG_CPU) would be faster on an ST
# but take 12 KB, which a 1 MB ST with EMail check doesn't have to spare
# (the sync of 6 Nov 2026: 312 KB free with 1.73, 293 with it)
APPCF   ?=
CFLAGS  += $(TRACECF) $(APPCF)
# dialogs ask the AES for one click and tell a double click themselves
# (common's click.c: C16, as ECal and EFTP)
CFLAGS  += -DDLG_CLICK
# each function and variable in a section of its own, and the link
# leaves out those nothing uses: common/ brings code EMail doesn't call
CFLAGS  += -ffunction-sections -fdata-sections
LDFLAGS := -m68000 -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld \
           -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments

# A Direct connection's TLS (BearSSL 0.6, the DSP's RSA) is TLS.OVL
# (the AERA guidelines, 7.4): built once in AERA-Atari as 68030 code and
# kept in common/tlsovl, loaded by a TT or Falcon when it first needs it,
# never by an ST. EMail itself is 68000 code, one program for every
# Atari; atari/tls_ovl.c and ui/dsp_ovl.c stand where tls.c and dsp.c
# were. The builds on this computer (make test, itest) link src/tls.c and
# BearSSL, which answer the same calls.
BSSL    := common/third_party/bearssl
BSSLSRC := $(wildcard $(BSSL)/src/*/*.c)
HBSSLOBJ = $(patsubst $(BSSL)/src/%.c,$(OBJDIR)/hbssl/%.o,$(BSSLSRC))

CORE    := util charset mime links conn imap pop3 smtp store compose mail futil bidi secret ctime tz
ATARI   := libc tos plat_tos net_tos sting gem tls_ovl crash bigbuf
UI      := main draw text notify tooltip aboutline win folders list find reader editor dialogs popup font fontsel about dsp_ovl theme etheme help av alert mbar aboutbox iconify clip clipedit dd busy people_ask dialog field av_people people_file panebar hebkey xlate translate textsel langs click arrowed printmsg print filepick fsel dirlist bell

OBJDIR  := build
# EMAIL.PRG's objects with a pad after each file 2.20 grew or shrank
# (atari/pad_*.S): the code after it at 2.19's addresses mod 256, so the
# 68030's cache falls as it did (the start was 10 ms slower without; the
# hub, 15 Nov 2026, as ECal's). EMAILCHK.ACC links the files without them
PCORE   := $(patsubst conn,conn pad_conn,$(patsubst store,store pad_store,$(CORE)))
PATARI  := $(patsubst plat_tos,plat_tos pad_plat,$(patsubst tls_ovl,tls_ovl pad_tlsovl,$(ATARI)))
PUI     := $(patsubst main,main pad_main,$(UI))
PADS    := $(foreach p,nf conn store ovl plat tlsovl main,-Wl,-u,pad_$(p)) -Wl,-u,pad_store_ro -Wl,-u,pad_tlsovl_ro
OBJS0   := $(OBJDIR)/crt0.o $(OBJDIR)/sting_s.o $(OBJDIR)/nf.o $(OBJDIR)/pad_nf.o $(OBJDIR)/crash_s.o \
           $(addprefix $(OBJDIR)/,$(addsuffix .o,$(PCORE) ovl pad_ovl $(PATARI) $(PUI) cpucheck bell_snd))
# 2.21's pads (the syncs of 17 Nov 2026 moved nearly every function): after
# each object, code, read-only data, data and BSS in bytes, so everything
# outside the files the syncs changed is at 2.20's places modulo 256, as
# common/tools/profpad.py found them against 2.20's build. Each is
# common/atari/codepad.S, its symbols kept with -u
KEEP := nf:194:0:0:0 pad_nf:0:0:252:0 util:194:183:0:0 conn:192:0:0:0 pad_conn:122:0:0:88 smtp:192:0:252:0 ovl:20:0:0:0 tos:60:0:4:0 \
	net_tos:78:0:0:0 gem:122:0:254:252 crash:194:0:0:0 bigbuf:110:0:250:2 text:228:0:0:238 find:100:0:0:6 dialogs:118:0:0:164 av:218:0:0:252 \
	aboutbox:160:0:252:0 iconify:0:232:0:64 dd:0:0:0:182 field:170:0:0:0 pad_main:132:0:0:0 draw:154:0:254:0 tooltip:22:0:0:0 \
	aboutline:80:0:246:154 win:242:0:0:0 list:6:0:0:0 editor:244:0:0:248 people_ask:200:0:0:240 dialog:140:0:0:232 compose:140:0:0:142 \
	notify:0:0:0:66 mbar:0:0:248:0 main:112:0:0:0 reader:6:0:232:0 about:208:0:0:0 folders:0:0:248:4 sting_s:0:0:200:0 charset:170:0:0:0 \
	mime:172:0:0:0 imap:174:0:0:0 pop3:152:0:0:0 store:22:0:0:0 mail:48:0:0:252 arrowed:202:0:0:0 help:220:0:0:0 print:186:0:102:152 \
	crash_s:0:0:0:176 libc:62:0:252:0 popup:230:0:0:0 dsp_ovl:60:0:0:0 theme:34:0:0:0 dirlist:36:0:200:162 etheme:0:0:0:244 alert:190:0:0:0 \
	pad_ovl:94:0:0:244 bell:240:0:0:8
kp_obj   = $(word 1,$(subst :, ,$(1)))
kp_size  = $(word $(2),$(subst :, ,$(1)))
OBJS    := $(foreach o,$(OBJS0),$(o) $(foreach k,$(KEEP),$(if $(filter $(OBJDIR)/$(call kp_obj,$(k)).o,$(o)),$(OBJDIR)/keep_$(call kp_obj,$(k)).o)))
PADS    += $(foreach k,$(KEEP),$(foreach s,keep_$(call kp_obj,$(k)) $(if $(filter-out 0,$(call kp_size,$(k),3)),keep_$(call kp_obj,$(k))_ro) \
           $(if $(filter-out 0,$(call kp_size,$(k),4)),keep_$(call kp_obj,$(k))_data) $(if $(filter-out 0,$(call kp_size,$(k),5)),keep_$(call kp_obj,$(k))_bss),-Wl,-u,$(s)))
HDRS    := $(wildcard src/*.h atari/*.h atari/include/*.h ui/*.h \
           common/src/*.h common/atari/*.h common/atari/include/*.h common/ui/*.h common/dsp/*.h)

# what goes onto the Atari: binary/ (built, never committed)
all: binary/email.prg binary/emailchk.acc binary/tls.ovl binary/roots.dat \
	binary/firebee/email.prg binary/firebee/emailchk.acc binary/firebee/tls.ovl

binary/email.prg: $(OBJDIR)/mail.elf common/tools/elf2tos.py | binary
	PRGFLAGS=7 python3 common/tools/elf2tos.py $< $@

# the FireBee package (decided 22 Nov 2026; the guidelines, 7.4):
# binary/firebee/, EMAIL.PRG and EMAILCHK.ACC built again for the ColdFire
# by common's mkfirebee.py from the commands make gives for the 68000 build
# (cpucheck.c becomes cpucheck_cf.S, which sends any other machine to the
# standard package), with common's tlsfb.ovl as tls.ovl (a FireBee build loads tls.ovl)
binary/firebee:
	mkdir -p $@
binary/firebee/email.prg: $(OBJDIR)/mail.elf common/tools/mkfirebee.py | binary/firebee
	python3 common/tools/mkfirebee.py --elf $(OBJDIR)/mail.elf --outdir $(OBJDIR)/cf/ $@
binary/firebee/emailchk.acc: binary/emailchk.acc common/tools/mkfirebee.py | binary/firebee
	python3 common/tools/mkfirebee.py --elf $(CHKDIR)/emailchk.elf \
		--make 'OBJDIR=$(CHKDIR) APPCF= TRACECF="-ffunction-sections -fdata-sections"' --outdir $(OBJDIR)/cfchk/ $@
binary/firebee/tls.ovl: common/tlsovl/tlsfb.ovl | binary/firebee
	cp $< $@

# the family's TLS.OVL as AERA-Atari built it: copied, never built here
binary/tls.ovl: common/tlsovl/tls.ovl | binary
	cp $< $@


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

$(OBJDIR)/mail.elf: $(OBJS) $(OBJDIR)/build.o common/atari/link.ld
	$(CC) $(LDFLAGS) -Wl,--gc-sections $(PADS) -Wl,-Map,$@.map -o $@ $(OBJS) $(OBJDIR)/build.o -lgcc

# EMail's icon (the AERA guidelines, 5.5): drawn in common/icons/EMAIL.txt,
# made into icons/ (the desktop's RSC and ICN files), ui/icon.h and
# icon16.h (the About box, the iconified window) and docs/icon*.png;
# committed, and a release carries icons/
icons:
	python3 common/tools/appicon.py EMAIL --rsc icons/EMAIL.RSC --rscnv icons/EMAILNV.RSC --icn icons/EMAIL.ICN --mask icons/EMAILMK.ICN --headers ui --docs docs

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
# 2.21's pads (KEEP above)
$(OBJDIR)/keep_%.o: common/atari/codepad.S Makefile | $(OBJDIR)
	$(CC) -m68000 -DPAD_NAME=keep_$* $(foreach k,$(KEEP),$(if $(filter $*,$(call kp_obj,$(k))),-DPAD_TEXT=$(call kp_size,$(k),2) -DPAD_RO=$(call kp_size,$(k),3) -DPAD_DATA=$(call kp_size,$(k),4) -DPAD_BSS=$(call kp_size,$(k),5))) -c -o $@ $<
# the pads that keep the code after a changed file where it was (2.20)
$(OBJDIR)/pad_%.o: atari/pad_%.S | $(OBJDIR)
	$(CC) -m68000 -c -o $@ $<

# EMAILCHK.ACC, "EMail check" (ui/emailchk.c): the accessory that looks
# for new mail while EMail isn't running. EMail's mail core without the
# windows, with its own few drawing calls for the shared About box and
# alerts. Its own objects (build/chk), each function in a section of its
# own, so the linker leaves out what the accessory never calls; a stack
# of 16 KB (its most, a Direct check with RSA on the DSP: 8.4 KB)
CHKDIR  := build/chk
CHKOBJ  := sting_s nf util charset mime links conn imap pop3 store newmail futil bidi secret ctime tz \
           ovl libc tos plat_tos net_tos sting gem tls_ovl bigbuf \
           emailchk notify aboutline dsp_ovl av alert dialog aboutbox accmem bgwork twin busy field clip clipedit crash crash_s text click cpucheck \
           bell bell_snd
binary/emailchk.acc: FORCE | binary
	$(MAKE) OBJDIR=$(CHKDIR) APPCF= TRACECF="-ffunction-sections -fdata-sections" $(CHKDIR)/emailchk.elf
	PRGFLAGS=7 python3 common/tools/elf2tos.py $(CHKDIR)/emailchk.elf $@
$(OBJDIR)/emailchk.elf: $(OBJDIR)/crt0chk.o $(addprefix $(OBJDIR)/,$(addsuffix .o,$(CHKOBJ))) $(OBJDIR)/build.o common/atari/link.ld
	$(CC) $(LDFLAGS) -Wl,--gc-sections -o $@ $(OBJDIR)/crt0chk.o $(addprefix $(OBJDIR)/,$(addsuffix .o,$(CHKOBJ))) \
		$(OBJDIR)/build.o -lgcc
$(OBJDIR)/crt0chk.o: common/atari/crt0.S | $(OBJDIR)
	$(CC) -m68000 -DSTACK_SIZE=16384 -c -o $@ $<

# the bell's sample, made at build time (common/tools/mkbell.py; decided 27 Nov 2026)
$(OBJDIR)/bell_snd.o: common/tools/mkbell.py | $(OBJDIR)
	python3 common/tools/mkbell.py $(OBJDIR)/bell_snd.c
	$(CC) $(CFLAGS) -c -o $@ $(OBJDIR)/bell_snd.c

# EMail saying on the emulator's console what each pane draws ("DRAW
# list y h", "COPY list ..."), for tests/hatari/scroll_test.py (never
# released): its own objects, so binary/ is left as it is
$(OBJDIR)/trace/email.prg: FORCE
	$(MAKE) OBJDIR=$(OBJDIR)/trace TRACECF=-DDRAW_TRACE $(OBJDIR)/trace/mail.elf
	PRGFLAGS=7 python3 common/tools/elf2tos.py $(OBJDIR)/trace/mail.elf $@

# EMail and EMail check saying how long they kept the AES waiting
# (-DAES_PROFILE=20: each stretch of 20 ms or more as "AESGAP <ms> ms",
# the start as "AESSTART"), for tests/hatari/speed.py (never released)
# EMAIL.PRG's timed build is linked at the shipped build's places modulo
# 256 (common/tools/profpad.py; the hub, 17 Nov 2026): the shipped
# objects, with only the ones AES_PROFILE changes (gem, reader) taken
# from build/speed, so the rig times the layout that ships
PROFOBJ := gem reader
TIMED   := $(foreach o,$(OBJS),$(if $(filter $(addprefix $(OBJDIR)/,$(addsuffix .o,$(PROFOBJ))),$(o)),$(OBJDIR)/speed/$(notdir $(o)),$(o)))
$(OBJDIR)/speed/email.prg: FORCE
	$(MAKE) $(OBJDIR)/mail.elf
	$(MAKE) OBJDIR=$(OBJDIR)/speed TRACECF=-DAES_PROFILE=20 $(addprefix $(OBJDIR)/speed/,$(addsuffix .o,$(PROFOBJ)))
	python3 common/tools/profpad.py --ship $(OBJDIR)/mail.elf --out $(OBJDIR)/speed/mail.elf \
		--cpu=-m68000 --dir $(OBJDIR)/speed/pads \
		$(foreach o,$(PROFOBJ),--exclude $(OBJDIR)/$(o).o --exclude $(OBJDIR)/speed/$(o).o) \
		-- $(CC) $(LDFLAGS) -Wl,--gc-sections $(PADS) $(TIMED) $(OBJDIR)/build.o -lgcc
	PRGFLAGS=7 python3 common/tools/elf2tos.py $(OBJDIR)/speed/mail.elf $@
$(OBJDIR)/speed/emailchk.acc: FORCE
	$(MAKE) OBJDIR=$(OBJDIR)/speed/chk APPCF= TRACECF="-ffunction-sections -fdata-sections -DAES_PROFILE=20" \
		$(OBJDIR)/speed/chk/emailchk.elf
	PRGFLAGS=7 python3 common/tools/elf2tos.py $(OBJDIR)/speed/chk/emailchk.elf $@

# EMail crashing on purpose with F1 to F5 (-DCRASH_TEST), for
# common/tests/crash/app_crash.py (never released)
$(OBJDIR)/crashtest/email.prg: FORCE
	$(MAKE) OBJDIR=$(OBJDIR)/crashtest TRACECF=-DCRASH_TEST $(OBJDIR)/crashtest/mail.elf
	PRGFLAGS=7 python3 common/tools/elf2tos.py $(OBJDIR)/crashtest/mail.elf $@

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

# the same, started as the browser program on plain TOS (C:\EWEB\EWEB.PRG):
# it writes its command line into C:\EWEB.LOG (never released)
$(OBJDIR)/fakeweb.prg: $(OBJDIR)/fakeweb.acc
	python3 common/tools/elf2tos.py $(OBJDIR)/fakeweb.elf $@

# the programs that come after an accessory, without the fast-load bit:
# tests/hatari/emailchk_test.py's held case (never released)
$(OBJDIR)/fillmem.prg: tests/atari/fillmem.c $(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKECAL))) $(OBJDIR)/nf.o
	$(CC) $(CFLAGS) -c -o $(OBJDIR)/fillmem.o $<
	$(CC) $(LDFLAGS) -o $(OBJDIR)/fillmem.elf $(OBJDIR)/fillmem.o \
		$(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKECAL))) $(OBJDIR)/nf.o -lgcc
	PRGFLAGS=0 python3 common/tools/elf2tos.py $(OBJDIR)/fillmem.elf $@

# a stand-in for a player holding the sound (EPlay), for
# tests/hatari/emailchk_test.py's busy case (never released)
$(OBJDIR)/fakeplay.acc: tests/atari/fakeplay.c $(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKECAL))) $(OBJDIR)/nf.o
	$(CC) $(CFLAGS) -c -o $(OBJDIR)/fakeplay.o $<
	$(CC) $(LDFLAGS) -o $(OBJDIR)/fakeplay.elf $(OBJDIR)/fakeplay.o \
		$(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKECAL))) $(OBJDIR)/nf.o -lgcc
	python3 common/tools/elf2tos.py $(OBJDIR)/fakeplay.elf $@

# a stand-in for a help viewer's receiving side (EHYPVIEW's VA_START), for
# tests/hatari/help_test.py: what EMail asks for goes into C:\EHYPVIEW.LOG
# (never released)
$(OBJDIR)/fakehelp.acc: tests/atari/fakeweb.c $(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKECAL))) $(OBJDIR)/nf.o
	$(CC) $(CFLAGS) -DFAKE_NAME='"EHYPVIEW"' -DFAKE_LOG='"C:\\EHYPVIEW.LOG"' -c -o $(OBJDIR)/fakehelp.o $<
	$(CC) $(LDFLAGS) -o $(OBJDIR)/fakehelp.elf $(OBJDIR)/fakehelp.o \
		$(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKECAL))) $(OBJDIR)/nf.o -lgcc
	python3 common/tools/elf2tos.py $(OBJDIR)/fakehelp.elf $@

# a stand-in for EPDF (its VA_START, and its program on plain TOS), for
# tests/hatari/pdf_test.py: C:\EPDF.LOG (never released)
$(OBJDIR)/fakeepdf.acc: tests/atari/fakeweb.c $(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKECAL))) $(OBJDIR)/nf.o
	$(CC) $(CFLAGS) -DFAKE_NAME='"EPDF"' -DFAKE_LOG='"C:\\EPDF.LOG"' -c -o $(OBJDIR)/fakeepdf.o $<
	$(CC) $(LDFLAGS) -o $(OBJDIR)/fakeepdf.elf $(OBJDIR)/fakeepdf.o \
		$(addprefix $(OBJDIR)/,$(addsuffix .o,$(FAKECAL))) $(OBJDIR)/nf.o -lgcc
	python3 common/tools/elf2tos.py $(OBJDIR)/fakeepdf.elf $@

$(OBJDIR)/fakeepdf.prg: $(OBJDIR)/fakeepdf.acc
	python3 common/tools/elf2tos.py $(OBJDIR)/fakeepdf.elf $@

$(OBJDIR) binary:
	mkdir -p $@

# ---- host build: the same mail core, for tests and mail-cli ----
HOSTCC     ?= cc
HOSTCFLAGS := -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare \
              -Wno-format-truncation -Isrc -Icommon/src -fsanitize=address,undefined
# each core file is the app's own (src/) or shared (common/src/)
HOSTSRC    := $(foreach m,$(CORE) newmail tls,$(firstword $(wildcard src/$(m).c common/src/$(m).c))) \
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
binary/cacert.pem: common/certs/CACERT.PEM | binary
	cp $< $@
binary/roots.dat: binary/cacert.pem $(OBJDIR)/mkroots
	$(OBJDIR)/mkroots binary/cacert.pem $@

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

# the FireBee edition's mail core under qemu-m68k -cpu cfv4e (decided 22 Nov
# 2026): tests/coldfire/core.c with the FireBee package's own ColdFire objects
# (build/cf/) and on the build machine; each input and 300 changed copies
# through the MIME parser and the IMAP client, the two builds' sums the same
CFT     := $(OBJDIR)/cftest
CFTCC   := $(CC) -mcpu=5475 -Os -fomit-frame-pointer -ffreestanding -fno-builtin -fno-pic -fno-pie \
           -fno-tree-loop-distribute-patterns -fno-asynchronous-unwind-tables -Wall -Wno-pointer-sign -DMAIL_TOS \
           -nostdinc -isystem $(GCCINC) -Iatari/include -Icommon/atari/include -Isrc -Icommon/src -Iatari \
           -Icommon/atari -Icommon/tests/fuzz
CFTOBJ  := mime imap charset util links conn libc ctime store
CFTIN_MIME := tests/mail/*.eml tests/fuzz/seeds/mime/*
CFTIN_IMAP := tests/fuzz/seeds/imap/* tests/fuzz/found/imap/*
$(CFT)/core_cf: tests/coldfire/core.c tests/coldfire/main_cf.c tests/coldfire/stubs_cf.c common/tests/fuzz/fuzz_net.c \
		common/tests/coldfire/rt.S binary/firebee/email.prg $(HDRS)
	@mkdir -p $(CFT)
	$(CC) -mcpu=5475 -c -o $(CFT)/rt.o common/tests/coldfire/rt.S
	for f in tests/coldfire/core.c tests/coldfire/main_cf.c tests/coldfire/stubs_cf.c common/tests/fuzz/fuzz_net.c; do \
	  $(CFTCC) -c -o $(CFT)/$$(basename $$f .c).o $$f || exit 1; done
	$(CC) -mcpu=5475 -nostdlib -static -no-pie -Wl,-z,noexecstack -Wl,--gc-sections -o $@ \
		$(addprefix $(CFT)/,rt.o main_cf.o core.o fuzz_net.o stubs_cf.o) \
		$(addprefix $(OBJDIR)/cf/,$(addsuffix .o,$(CFTOBJ))) $(OBJDIR)/cf/rt/ll64.o
$(CFT)/core_host: tests/coldfire/core.c tests/coldfire/main_host.c common/tests/fuzz/fuzz_net.c $(HOSTSRC) $(HDRS) $(HOSTLIB)
	@mkdir -p $(CFT)
	$(HOSTCC) $(REPLAYFLAGS) -o $@ tests/coldfire/main_host.c tests/coldfire/core.c $(HOSTSRC) \
		common/tests/fuzz/fuzz_net.c $(HOSTLIB)
cftest: $(CFT)/core_cf $(CFT)/core_host
	$(CFT)/core_host mime $(CFTIN_MIME) > $(CFT)/host.txt
	$(CFT)/core_host imap $(CFTIN_IMAP) >> $(CFT)/host.txt
	qemu-m68k -cpu cfv4e $(CFT)/core_cf mime $(CFTIN_MIME) > $(CFT)/cf.txt
	qemu-m68k -cpu cfv4e $(CFT)/core_cf imap $(CFTIN_IMAP) >> $(CFT)/cf.txt
	diff $(CFT)/host.txt $(CFT)/cf.txt
	@echo "cftest: $$(wc -l < $(CFT)/cf.txt) inputs, each with 300 changed copies: the ColdFire's core gives the host's every sum"

# the 68000 builds looked at for word and long accesses at an odd stack
# or frame offset, an address error on a 68000 (the guidelines, The
# machines: common/tools/oddaccess.py)
test: $(OBJDIR)/unit $(addprefix $(OBJDIR)/replay_,$(FUZZERS)) binary/email.prg binary/emailchk.acc
	python3 common/tools/oddaccess.py $(OBJDIR)/mail.elf $(CHKDIR)/emailchk.elf
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
help: binary/email.hyp
binary/email.hyp: docs/GUIDE.md tools/mkhelp.py VERSION docs/front.json docs/help/WINDOW.IMG \
                  common/help/EMAIL.IMG common/tools/helpfront.py common/tools/gemimg.py | $(OBJDIR) binary
	python3 tools/mkhelp.py docs/GUIDE.md VERSION $(OBJDIR)/EMAIL.STG
	python3 common/tools/helpfront.py page docs/front.json $(OBJDIR)/EMAIL.STG $(OBJDIR)/EMAIL.STG
	$(HCP) --references -o $@ $(OBJDIR)/EMAIL.STG >/dev/null

# a release's files: each one in dist/, and all of them in
# dist/email-x.y.zip (the icons in icons/), for the release workflow;
# every name in lowercase (the guidelines' 6.6: MiNT's ext2 drives tell
# EMAIL.PRG and email.prg apart)
# a release: two zips and nothing else, email-x.y.zip and
# email-x.y-firebee.zip, the same names inside (decided 22 Nov 2026;
# EUpdate takes the one for the machine it runs on)
SHARED  := binary/cacert.pem binary/roots.dat binary/email.hyp binary/email.ref
DIST    := binary/email.prg binary/emailchk.acc binary/tls.ovl $(SHARED)
DISTFB  := binary/firebee/email.prg binary/firebee/emailchk.acc binary/firebee/tls.ovl $(SHARED)
ICONS   := icons/EMAIL.RSC icons/EMAILNV.RSC icons/EMAIL.ICN icons/EMAILMK.ICN
dist: all help
	rm -rf dist
	mkdir -p dist/zip/icons dist/zipfb/icons
	cp $(DIST) dist/zip/
	cp $(DISTFB) dist/zipfb/
	for f in $(ICONS); do b=$$(basename $$f | tr A-Z a-z); cp $$f dist/zip/icons/$$b; \
	  cp $$f dist/zipfb/icons/$$b; done
	cd dist/zip && zip -q -9 -r ../email-$(VERSION_STR).zip .
	cd dist/zipfb && zip -q -9 -r ../email-$(VERSION_STR)-firebee.zip .
	rm -rf dist/zip dist/zipfb

clean:
	rm -rf $(OBJDIR) binary dist

.PHONY: help all icons dist test itest cftest clean fuzz FORCE
