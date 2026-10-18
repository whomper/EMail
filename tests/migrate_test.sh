#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
#
# The .EML files of EMail 1.41 and before moved into the folder's pack
# (src/migrate_eml.c), with no server to fetch from: what was on disk
# must still read afterwards, in an IMAP folder and in the local Outbox.
#
# TEMPORARY, with migrate_eml.c: this file and its line in the Makefile's
# itest go once Erez has run the release that brought the pack.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CLI=$ROOT/build/mail-cli
W=$(mktemp -d /tmp/mail-migrate.XXXXXX)
PASS=0 FAIL=0
run() { ASAN_OPTIONS=detect_leaks=0 "$CLI" "$W" "$@" 2>&1; }
expect() {
  if grep -q -- "$2" <<<"$3"; then PASS=$((PASS+1)); echo "ok   $1"
  else FAIL=$((FAIL+1)); echo "FAIL $1"; echo "     wanted: $2"; sed 's/^/     | /' <<<"$3"; fi
}

# an account whose server isn't there (port 1): everything comes from disk
printf '[options]\r\ntz=0\r\n[account]\r\nname=Old\r\nemail=dana@test.local\r\nin=imap\r\nhost=127.0.0.1\r\nport=1\r\nuser=dana\r\npass=x\r\nsmtphost=127.0.0.1\r\nsmtpport=1\r\n' >"$W/EMAIL.INF"
A=$W/EMAIL/ACCT1 IN=$W/EMAIL/ACCT1/FD53E5C5 OB=$W/EMAIL/ACCT1/F8AD0D07
mkdir -p "$IN" "$OB"
printf '7\t0\t1\t1\t0\tOutbox\tF8AD0D07\r\n1\t0\t0\t3\t0\tINBOX\tFD53E5C5\r\n' >"$A/FOLDERS.LST"
msg() { printf 'From: Old Friend <old@example.com>\r\nTo: dana@test.local\r\nSubject: %s\r\n\r\n%s\r\n' "$1" "$2"; }
# as 1.41 left a folder: a version 3 index, one .EML per body (flags 101:
# read and cached); the third says cached but its file is gone
printf 'MAILIDX 3 1 4 100 3\n1\t101\t80\t1700000000\tOld Friend\tFirst old\tdana@test.local\n2\t101\t80\t1700000600\tOld Friend\tSecond old\tdana@test.local\n3\t101\t80\t1700001200\tOld Friend\tThird old\tdana@test.local\n' >"$IN/INDEX.DAT"
msg "First old" "the first body, from before the pack" >"$IN/00000001.EML"
msg "Second old" "the second body, from before the pack" >"$IN/00000002.EML"
printf 'MAILIDX 3 0 2 0 0\n1\t101\t80\t1700002000\tDana\tWaiting old\told@example.com\n' >"$OB/INDEX.DAT"
msg "Waiting old" "a message waiting in the Outbox since 1.41" >"$OB/00000001.EML"

out=$(run list 1 INBOX)
expect "migrated folder lists its messages" "First old" "$out"
[ -z "$(find "$IN" -name '*.EML')" ] && [ -s "$IN/MESSAGES.DAT" ] && { PASS=$((PASS+1)); echo "ok   the .EML files are in the pack, and gone"; } \
  || { FAIL=$((FAIL+1)); echo "FAIL .EML files left, or no pack:"; ls -l "$IN" | sed 's/^/     | /'; }
expect "the index is the pack's now (version 4)" "^MAILIDX 4" "$(head -1 "$IN/INDEX.DAT")"
out=$(run show 1 INBOX 1)
expect "a migrated body reads with no server" "the first body, from before the pack" "$out"
out=$(run show 1 INBOX 2)
expect "the second one too" "the second body, from before the pack" "$out"
out=$(run show 1 INBOX 3)
expect "a body whose file was gone is not shown as something else" "error: fetch" "$out"
out=$(run list 1 Outbox)
[ -z "$(find "$OB" -name '*.EML')" ] && [ -s "$OB/MESSAGES.DAT" ] && { PASS=$((PASS+1)); echo "ok   the Outbox's message is in its pack"; } \
  || { FAIL=$((FAIL+1)); echo "FAIL the Outbox's .EML is still there"; }
out=$(run show 1 Outbox 1)
expect "the Outbox's message, only on the Atari, still reads" "a message waiting in the Outbox since 1.41" "$out"
# once is enough: opening again finds nothing to move
before=$(stat -c %Y "$IN/INDEX.DAT")
sleep 1
out=$(run list 1 INBOX)
[ "$(stat -c %Y "$IN/INDEX.DAT")" = "$before" ] && { PASS=$((PASS+1)); echo "ok   opened again: nothing moved, nothing written"; } \
  || { FAIL=$((FAIL+1)); echo "FAIL opened again: the index was written"; }

rm -rf "$W"
echo "migration: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
