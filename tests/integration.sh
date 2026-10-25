#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# Integration test of EMail's mail core against a real IMAP/POP3 server
# (Dovecot) and an SMTP server, all on localhost. Needs dovecot-imapd and
# dovecot-pop3d installed; run with `make itest`.
#
# TLS=1 runs the same tests through the Raspberry Pi gateway: the servers
# only speak TLS (IMAPS, POP3S, SMTP on 465 style) and gateway/emailgate.py,
# as on the Pi, offers the plain ports EMail uses. Needs openssl.
#
# FALCON=1 tests Falcon mode: EMail speaks TLS itself (BearSSL), to the
# TLS-only ports (IMAP and SMTP of the first account) and with STARTTLS
# on the plain ports (POP3 STLS and SMTP STARTTLS of the second).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CLI=$ROOT/build/mail-cli
D=${MAIL_TEST_DIR:-$(mktemp -d /tmp/mail-itest.XXXXXX)}
IMAP=${IMAP_PORT:-10143}; POP3=${POP3_PORT:-10110}; SMTP=${SMTP_PORT:-10025}
PASS=0; FAIL=0

RUN=$(mktemp -d /tmp/tdv.XXXXXX)   # short: Unix socket paths are limited
mkdir -p "$D"/{state,home/dana/Maildir/{new,cur,tmp},work}
echo "dana:{PLAIN}secret" > "$D/users"
[ -n "${FALCON:-}" ] && TLS=1
if [ -n "${TLS:-}" ]; then
  # servers on TLS-only ports, the gateway in front as on the Pi
  openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj /CN=localhost \
    -addext subjectAltName=DNS:localhost -keyout "$D/key.pem" -out "$D/cert.pem" 2>/dev/null
  chmod 644 "$D/key.pem"
  S_IMAP=0; S_POP3=0; S_SMTP=$((SMTP + 440))
  # Falcon mode keeps the plain ports too, where Dovecot offers STARTTLS
  P_IMAP=0; P_POP3=0
  [ -n "${FALCON:-}" ] && { P_IMAP=$IMAP; P_POP3=$POP3; }
  sed -e "s|@RUN@|$RUN|g" -e "s|@DIR@|$D|g" -e "s|@IMAP@|$P_IMAP|" -e "s|@POP3@|$P_POP3|" \
      -e "s|@UID@|$(id -u nobody)|" -e "s|@GID@|$(id -g nobody)|" \
      -e "s|^ssl = no|ssl = yes\nssl_cert = <$D/cert.pem\nssl_key = <$D/key.pem|" \
      "$ROOT/tests/dovecot.conf.in" > "$D/dovecot.conf"
  python3 - "$D/dovecot.conf" $((IMAP + 850)) $((POP3 + 885)) <<'PY'
import sys
p, imaps, pop3s = sys.argv[1], sys.argv[2], sys.argv[3]
s = open(p).read()
s = s.replace("inet_listener imaps {\n    port = 0", "inet_listener imaps {\n    port = " + imaps)
s = s.replace("inet_listener pop3s {\n    port = 0", "inet_listener pop3s {\n    port = " + pop3s)
open(p, "w").write(s)
PY
  # (the test certificate the only root it trusts, as the Pi trusts the
  # system's)
  if [ -n "${FALCON:-}" ]; then TUNPID=; else
  SSL_CERT_FILE="$D/cert.pem" python3 "$ROOT/gateway/emailgate.py" --bind 127.0.0.1 -v \
    --ports $IMAP,$SMTP,$POP3 localhost:$((IMAP + 850)) localhost:$S_SMTP localhost:$((POP3 + 885)) \
    2>"$D/emailgate.log" & TUNPID=$!
  fi
else
  S_SMTP=$SMTP
  sed -e "s|@RUN@|$RUN|g" -e "s|@DIR@|$D|g" -e "s|@IMAP@|$IMAP|" -e "s|@POP3@|$POP3|" \
      -e "s|@UID@|$(id -u nobody)|" -e "s|@GID@|$(id -g nobody)|" "$ROOT/tests/dovecot.conf.in" > "$D/dovecot.conf"
  TUNPID=
fi
chmod 755 "$D"
dovecot -c "$D/dovecot.conf" || { echo "can't start dovecot"; exit 1; }
if [ -n "${TLS:-}" ]; then
  python3 "$ROOT/tests/smtp_server.py" $S_SMTP "$D" dana secret "$D/cert.pem" "$D/key.pem" & SMTPPID=$!
  if [ -n "${FALCON:-}" ]; then
    python3 "$ROOT/tests/smtp_server.py" $SMTP "$D" dana secret "$D/cert.pem" "$D/key.pem" starttls & SMTPPID="$SMTPPID $!"
  fi
else
  python3 "$ROOT/tests/smtp_server.py" $S_SMTP "$D" dana secret & SMTPPID=$!
fi
trap 'kill $SMTPPID $TUNPID 2>/dev/null; doveadm -c "$D/dovecot.conf" stop 2>/dev/null; rm -rf "$RUN"; [ -z "${KEEP:-}" ] && rm -rf "$D"' EXIT
sleep 1

# three messages waiting in the inbox: Hebrew multipart, windows-1255, HTML only
cp "$ROOT"/tests/mail/*.eml "$D/home/dana/Maildir/new/"
chown -R nobody: "$D/home"   # Dovecot won't serve mail as root

W=$D/work
cat > "$W/EMAIL.INF" <<INF
[options]
tz=180
headers=300
signature=Sent from my Atari Falcon
[account]
name=Test IMAP
fullname=Yaary
email=dana@test.local
in=imap
host=127.0.0.1
port=$IMAP
user=dana
pass=secret
smtphost=127.0.0.1
smtpport=$SMTP
[account]
name=Test POP
email=dana@test.local
in=pop3
host=127.0.0.1
port=$POP3
user=dana
pass=secret
leave=1
smtphost=127.0.0.1
smtpport=$SMTP
INF

if [ -n "${FALCON:-}" ]; then
  # Falcon mode: EMail talks TLS to the servers itself; the test
  # certificate is the only root it trusts
  cp "$D/cert.pem" "$W/CACERT.PEM"
  python3 - "$W/EMAIL.INF" $((IMAP + 850)) $S_SMTP $POP3 $SMTP <<'PY'
import sys
p, imaps, smtps, pop3, smtp = sys.argv[1:]
s = open(p).read()
s = s.replace("[options]\n", "[options]\nfalcon=1\nlog=1\n", 1)
a, b = s.split("[account]\n")[1:]
a += "dhost=localhost\ndport=%s\ndsec=1\ndsmtphost=localhost\ndsmtpport=%s\ndsmtpsec=1\n" % (imaps, smtps)
b += "dhost=localhost\ndport=%s\ndsec=2\ndsmtphost=localhost\ndsmtpport=%s\ndsmtpsec=2\n" % (pop3, smtp)
open(p, "w").write(s.split("[account]\n")[0] + "[account]\n" + a + "[account]\n" + b)
PY
fi

run() { ASAN_OPTIONS=detect_leaks=0 "$CLI" "$W" "$@" 2>&1; }
# Falcon mode checks certificate dates against the clock and tz=180: the
# local clock must be UTC+3 too, as on an Atari set to Israeli time
[ -n "${FALCON:-}" ] && export TZ=Etc/GMT-3 MAIL_LOG=1
expect() {  # expect DESCRIPTION PATTERN OUTPUT
  if grep -q -- "$2" <<<"$3"; then PASS=$((PASS+1)); echo "ok   $1"
  else FAIL=$((FAIL+1)); echo "FAIL $1"; echo "     wanted: $2"; sed 's/^/     | /' <<<"$3"; fi
}
uid_of() { awk -F'\t' -v s="$2" 'index($0, s) { print $1; exit }' <<<"$1"; }

out=$(run folders 1)
expect "IMAP folder list has Sent with its role" "^Sent .*role=2" "$out"
expect "IMAP folder list has Trash with its role" "^Trash .*role=3" "$out"
expect "local Outbox exists" "^Outbox .*role=7 local=1" "$out"

python3 - $IMAP <<'PY'
import imaplib, sys
m = imaplib.IMAP4("127.0.0.1", int(sys.argv[1]))
m.login("dana", "secret")
for i in range(1, 31):          # more than one batch of questions
    m.create("F%02d" % i)
m.logout()
PY
out=$(run check 1)
expect "check finds 3 new messages" "new: 3" "$out"

# every folder's counts on each check; new mail outside Trash is new
python3 - $IMAP <<'PY'
import imaplib, sys
m = imaplib.IMAP4("127.0.0.1", int(sys.argv[1]))
m.login("dana", "secret")
m.append("Junk", "()", None, b"From: spam@example.com\r\nSubject: Win\r\n\r\nwin\r\n")
m.append("Trash", "(\\Seen)", None, b"From: old@example.com\r\nSubject: Old\r\n\r\nold\r\n")
m.append("F30", "()", None, b"From: list@example.com\r\nSubject: List\r\n\r\nlist\r\n")
m.logout()
PY
out=$(MAIL_LOG=1 run check 1)
expect "check: new messages in Junk and F30 count" "new: 2" "$out"
out=$(run folders 1)
expect "check: Junk counts updated" "^Junk .*role=5 .*total=1 unread=1" "$out"
expect "check: Trash counts updated" "^Trash .*role=3 .*total=1 unread=0" "$out"
expect "check: F30's counts (34 folders: two batches of questions)" "^F30 .*total=1 unread=1" "$out"
# GEMDOS keeps some of its little internal memory for each directory it
# has been in: only folders opened get one, not all 30 at every start
ndirs=$(find "$W/EMAIL" -mindepth 2 -maxdepth 2 -type d -name 'F*' | wc -l)
if [ "$ndirs" -le 4 ]; then PASS=$((PASS+1)); echo "ok   only opened folders have a directory ($ndirs)"
else FAIL=$((FAIL+1)); echo "FAIL $ndirs folder directories made, most never opened"; fi
python3 - $IMAP <<'PY'
import imaplib, sys
m = imaplib.IMAP4("127.0.0.1", int(sys.argv[1]))
m.login("dana", "secret")
m.select("Trash"); m.store("1:*", "+FLAGS", "(\\Deleted)"); m.expunge()
m.logout()
PY
out=$(run list 1 INBOX)
expect "Hebrew subject in the index" "שלום" "$out"
expect "windows-1255 subject decoded" "מכתב" "$out"
expect "attachment marker on the multipart message" "@" "$out"
U1=$(uid_of "$out" "שלום")
out=$(run show 1 INBOX "$U1")
expect "the HTML part shown (as text)" "שלום 1990" "$out"
expect "attachment with RFC 2231 Hebrew name" "Attachment: ת.png" "$out"
out=$(run list 1 INBOX)
expect "reading marks the message seen" "^$U1	 " "$out"
U3=$(uid_of "$out" "Newsletter")
out=$(run show 1 INBOX "$U3")
expect "HTML-only message turned into text" "Hello Atari fans" "$out"
expect "HTML list item" "  - Falcon030" "$out"

run flag 1 INBOX "$U1" +flagged >/dev/null
run sync 1 INBOX >/dev/null
out=$(run list 1 INBOX)
expect "flag stored on the server and synced back" "^$U1	 !" "$out"

# not connected: what is cached still reads, a message that isn't says
# why, and read/flag changes wait for the next connection
U2=$(uid_of "$out" "מכתב")
cp "$W/EMAIL.INF" "$W/EMAIL.INF.net"
python3 - "$W/EMAIL.INF" <<'PY'
import re, sys
p = sys.argv[1]
s = open(p, newline="").read()
i = s.index("[account]", s.index("[account]") + 1)   # the first account only
s = re.sub(r"(?m)^(d?port)=\d+", r"\1=9", s[:i]) + s[i:]   # nothing listens there
open(p, "w", newline="").write(s)
PY
out=$(run flag 1 INBOX "$U1" -seen)
expect "not connected: marked unread here" "^ok" "$out"
out=$(run show 1 INBOX "$U1")
expect "not connected: a cached message still reads" "שלום 1990" "$out"
out=$(run flag 1 INBOX "$U1" -seen)
out=$(run show 1 INBOX "$U2")
expect "not connected: an uncached message says why" "not connected\|can't connect\|refused" "$out"
cp "$W/EMAIL.INF.net" "$W/EMAIL.INF"
run sync 1 INBOX >/dev/null
srv=$(python3 - $IMAP "$U1" <<'PY'
import imaplib, sys
m = imaplib.IMAP4("127.0.0.1", int(sys.argv[1]))
m.login("dana", "secret")
m.select("INBOX")
print(m.uid("FETCH", sys.argv[2], "(FLAGS)")[1][0].decode())
PY
)
if grep -q "Flagged" <<<"$srv" && ! grep -q "Seen" <<<"$srv"; then PASS=$((PASS+1)); echo "ok   offline change reached the server"
else FAIL=$((FAIL+1)); echo "FAIL offline change not on the server: $srv"; fi
out=$(run list 1 INBOX)
expect "and stays after the sync" "^$U1	N!" "$out"
run flag 1 INBOX "$U1" +seen >/dev/null

cat > "$W/msg.txt" <<'MSG'
To: Dana <dana@test.local>
Cc:
Bcc: dana@test.local
Subject: תשובה from the Falcon

שלום דנה,
this was written in EMail.
.a line starting with a dot
MSG
out=$(run send 1 "$W/msg.txt")
expect "message sent over SMTP" "sent: 1" "$out"
# a folder made elsewhere (on a phone): the first check of a session reads the folder list
python3 - $IMAP <<'PY'
import imaplib, sys
m = imaplib.IMAP4("127.0.0.1", int(sys.argv[1]))
m.login("dana", "secret")
m.create("Fresh")
m.logout()
PY
out=$(run check 1)
expect "the sent message arrives (To + Bcc)" "new: 2" "$out"
grep -q "Fresh" "$W/EMAIL/ACCT1/FOLDERS.LST" && { PASS=$((PASS+1)); echo "ok   a folder made elsewhere shows up at the next start"; } || { FAIL=$((FAIL+1)); echo "FAIL new folder not listed"; }
out=$(run list 1 INBOX)
expect "Hebrew subject of the new mail" "תשובה from the Falcon" "$out"
U4=$(uid_of "$out" "תשובה")
out=$(run show 1 INBOX "$U4")
expect "Hebrew body arrives intact" "שלום דנה," "$out"
expect "dot-stuffing round trip" "^\.a line starting with a dot" "$out"
if grep -rq "X-Mail-Bcc" "$D/home/dana/Maildir"; then FAIL=$((FAIL+1)); echo "FAIL Bcc header leaked"; else PASS=$((PASS+1)); echo "ok   Bcc header not sent"; fi
out=$(run sync 1 Sent)
expect "copy saved in Sent with APPEND" "synced: 1 total" "$out"

out=$(run delete 1 INBOX "$U3")
expect "delete" "ok" "$out"
out=$(run sync 1 Trash)
expect "deleted message went to Trash" "synced: 1 total" "$out"
out=$(run list 1 INBOX)
if grep -q "Newsletter" <<<"$out"; then FAIL=$((FAIL+1)); echo "FAIL deleted message still listed"; else PASS=$((PASS+1)); echo "ok   deleted message gone from the index"; fi

out=$(run mkdir 1 "ארכיון")
expect "create a folder with a Hebrew name" "ok" "$out"
out=$(run folders 1)
expect "Hebrew folder name decoded from modified UTF-7" "disp=ארכיון" "$out"
SERVER=$(grep "disp=ארכיון" <<<"$out" | awk '{print $1}')
out=$(run move 1 INBOX "$U1" "$SERVER")
expect "move to the Hebrew folder" "ok" "$out"
out=$(run sync 1 "$SERVER")
expect "message is in the Hebrew folder" "synced: 1 total" "$out"
out=$(run list 1 "$SERVER")
expect "moved with its flag (and read state)" "^[0-9]*	 !" "$out"
out=$(run list 1 INBOX)
if grep -q "^$U1	" <<<"$out"; then FAIL=$((FAIL+1)); echo "FAIL moved message still in the Inbox"; else PASS=$((PASS+1)); echo "ok   moved message gone from the Inbox"; fi
U5=$(uid_of "$(run list 1 "$SERVER")" "שלום")
run show 1 "$SERVER" "$U5" >/dev/null

# rename it: the server has the new name only, the cache comes along
out=$(run rename 1 "$SERVER" "Archive 2026")
expect "rename a folder" "^ok Archive 2026" "$out"
srv=$(python3 - $IMAP <<'PY'
import imaplib, sys
m = imaplib.IMAP4("127.0.0.1", int(sys.argv[1]))
m.login("dana", "secret")
print(" | ".join(l.decode() for l in m.list()[1]))
PY
)
if grep -q '"Archive 2026"' <<<"$srv" && ! grep -q "&BdA" <<<"$srv"; then PASS=$((PASS+1)); echo "ok   renamed on the server"
else FAIL=$((FAIL+1)); echo "FAIL rename on the server: $srv"; fi
out=$(run list 1 "Archive 2026")
expect "renamed folder keeps its cached messages" "שלום" "$out"
out=$(run show 1 "Archive 2026" "$U5")
expect "and their downloaded bodies" "שלום 1990" "$out"
out=$(run rename 1 INBOX "Other")
expect "Inbox can't be renamed" "can't be renamed" "$out"
SERVER="Archive 2026"

# a server that drops the connection on RENAME after doing it: EMail
# connects again, sees the new name and finishes the job
if [ -z "${TLS:-}" ] && [ -z "${FALCON:-}" ]; then
  python3 "$ROOT/tests/drop_proxy.py" $((IMAP + 900)) $IMAP & SMTPPID="$SMTPPID $!"
  sleep 1
  cp "$W/EMAIL.INF" "$W/EMAIL.INF.keep"
  sed -i "0,/^port=$IMAP/s//port=$((IMAP + 900))/" "$W/EMAIL.INF"
  out=$(run rename 1 "Archive 2026" "Archive 2027")
  expect "rename survives a dropped connection" "^ok Archive 2027" "$out"
  cp "$W/EMAIL.INF.keep" "$W/EMAIL.INF"
  out=$(run list 1 "Archive 2027")
  expect "and the folder keeps its messages" "שלום" "$out"
  SERVER="Archive 2027"
fi

# a message expunged by another client disappears from the mirror
doveadm -c "$D/dovecot.conf" expunge -u dana mailbox INBOX header Message-ID "cp1255@example.org" || echo "doveadm expunge failed"
out=$(run sync 1 INBOX)
out=$(run list 1 INBOX)
if grep -q "מכתב" <<<"$out"; then FAIL=$((FAIL+1)); echo "FAIL expunged message still mirrored"; else PASS=$((PASS+1)); echo "ok   expunge by another client mirrored"; fi

# a big folder: only a page of 100 is mirrored, more on request
python3 - $IMAP <<'PY'
import imaplib, sys, time
m = imaplib.IMAP4("127.0.0.1", int(sys.argv[1]))
m.login("dana", "secret")
m.create("Big")
for i in range(1, 251):
    msg = ("From: robot@example.com\r\nTo: dana@test.local\r\nSubject: Big %03d\r\n"
           "Date: Mon, 05 Oct 2026 10:%02d:00 +0000\r\n\r\nmessage %d\r\n" % (i, i % 60, i))
    m.append("Big", "(\\Seen)" if i < 245 else "()", None, msg.encode())
m.logout()
PY
out=$(MAIL_DEBUG=1 run sync 1 Big)
expect "progress: headers counted with a bar" "Big - headers 100 of 100  \[##########\] 100%" "$out"
expect "big folder: server count, only 100 mirrored" "synced: 250 total, 6 unread, .* 100 loaded" "$out"
out=$(run list 1 Big)
expect "big folder: newest message mirrored" "Big 250" "$out"

# a message with a 1 MB attachment: downloaded, saved and opened whole
python3 - $IMAP <<'PY'
import imaplib, sys, base64, os
m = imaplib.IMAP4("127.0.0.1", int(sys.argv[1]))
m.login("dana", "secret")
att = base64.encodebytes(os.urandom(1024 * 1024)).replace(b"\n", b"\r\n")
msg = (b"From: big@example.com\r\nTo: dana@test.local\r\nSubject: Large attachment\r\n"
       b"Date: Mon, 12 Oct 2026 10:00:00 +0000\r\nMIME-Version: 1.0\r\n"
       b"Content-Type: multipart/mixed; boundary=XX\r\n\r\n--XX\r\n"
       b"Content-Type: text/plain\r\n\r\nSee the file.\r\n--XX\r\n"
       b"Content-Type: application/octet-stream; name=big.bin\r\n"
       b"Content-Transfer-Encoding: base64\r\n\r\n" + att + b"--XX--\r\n")
m.create("Huge")
m.append("Huge", "()", None, msg)
m.logout()
PY
run folders 1 >/dev/null
run sync 1 Huge >/dev/null
out=$(run list 1 Huge)
UB=$(uid_of "$out" "Large attachment")
out=$(run show 1 Huge "$UB")
expect "1 MB attachment: the message opens" "See the file." "$out"
expect "1 MB attachment: listed" "Attachment: big.bin" "$out"
if grep -q "Big 150" <<<"$out"; then FAIL=$((FAIL+1)); echo "FAIL big folder: old message mirrored too early"; else PASS=$((PASS+1)); echo "ok   big folder: older messages stay on the server"; fi
out=$(run more 1 Big)
expect "load more: the next 100" "added: 100, loaded 200 of 250" "$out"
out=$(run more 1 Big)
expect "load more: the rest" "added: 50, loaded 250 of 250" "$out"
out=$(run list 1 Big)
expect "big folder: oldest message now mirrored" "Big 001" "$out"
out=$(run sync 1 Big)
expect "resync keeps the loaded window" "250 loaded" "$out"
U=$(uid_of "$(run list 1 Big)" "Big 250")
out=$(run show 1 Big "$U")
out=$(run sync 1 Big)
expect "reading updates the server's unread count" "synced: 250 total, 5 unread" "$out"

# Download all messages: every one mirrored and its body on disk, none
# marked read; and the window stays "all" when the folder is synced again
out=$(run getall 1 Big)
expect "download all: the whole folder" "loaded 250 of 250, downloaded 2[0-9][0-9], had [0-9]" "$out"
out=$(run sync 1 Big)
expect "download all: downloading doesn't mark them read" "synced: 250 total, 5 unread" "$out"
python3 - $IMAP <<'PY'
import imaplib, sys
m = imaplib.IMAP4("127.0.0.1", int(sys.argv[1]))
m.login("dana", "secret")
for i in range(251, 311):           # past the window Load more left (300)
    m.append("Big", "()", None, b"From: robot@example.com\r\nTo: dana@test.local\r\nSubject: Big %d\r\n\r\nmessage %d\r\n" % (i, i))
m.logout()
PY
out=$(run sync 1 Big)
expect "download all: the folder mirrors all of it after" "310 total, .* 310 loaded" "$out"
out=$(run getall 1 Big)
expect "download all again: only the new ones" "loaded 310 of 310, downloaded 60, had 250" "$out"

# Download again: a copy that came wrong is replaced by the server's
U=$(uid_of "$(run list 1 Big)" "Big 007")
run garble 1 Big "$U" >/dev/null
out=$(run show 1 Big "$U")
expect "download again: the bad copy is what shows first" "garbled" "$out"
out=$(run again 1 Big "$U")
expect "download again: ok" "^ok" "$out"
out=$(run show 1 Big "$U")
expect "download again: the server's copy" "message 7" "$out"
out=$(run again 1 Big 999999)
expect "download again: a message that isn't there" "no message" "$out"

# The pack: a folder's bodies in one file, MESSAGES.DAT, and each record
# where the index says (its header names the UID and the length)
pack_ok() {  # pack_ok FOLDERDIR: every record the index has, checked
  python3 - "$1" <<'PY'
import os, struct, sys
d = sys.argv[1]
pack = open(os.path.join(d, "MESSAGES.DAT"), "rb").read() if os.path.exists(os.path.join(d, "MESSAGES.DAT")) else b""
bad = n = 0
for line in open(os.path.join(d, "INDEX.DAT"), encoding="latin-1").read().split("\n")[1:]:
    f = line.split("\t")
    if len(f) < 9 or int(f[7]) < 0:
        continue
    uid, off, ln = int(f[0]), int(f[7]), int(f[8])
    rec = pack[off:off + 10]
    n += 1
    end = off + 10 + ln
    whole = end == len(pack) or pack[end:end + 2] == b"EM"   # followed by a record, or the end
    if len(rec) < 10 or rec[:2] != b"EM" or struct.unpack(">II", rec[2:]) != (uid, ln) or not whole:
        bad += 1
print("records %d, bad %d" % (n, bad))
PY
}
read -r PACK OFF LEN <<<"$(run pack 1 Big "$U")"
BIGDIR=$(dirname "$PACK")
[ -s "$PACK" ] && [ -z "$(find "$BIGDIR" -name '*.EML')" ] && { PASS=$((PASS+1)); echo "ok   pack: the bodies are in MESSAGES.DAT, no .EML files"; } \
  || { FAIL=$((FAIL+1)); echo "FAIL pack: no MESSAGES.DAT, or .EML files left"; }
expect "pack: every record is where the index says" "records 3[01][0-9], bad 0" "$(pack_ok "$BIGDIR")"
# a reset in the middle of an append: the pack ends inside a record
UL=$(uid_of "$(run list 1 Big)" "Big 310")
read -r PACK OFF LEN <<<"$(run pack 1 Big "$UL")"
truncate -s $((OFF + 20)) "$PACK"
out=$(run show 1 Big "$UL")
expect "pack cut short: the message comes from the server again" "message 310" "$out"
expect "pack cut short: the index points at whole records again" "bad 0" "$(pack_ok "$BIGDIR")"
# a record that isn't what the index says is never shown
U9=$(uid_of "$(run list 1 Big)" "Big 009")
read -r PACK OFF LEN <<<"$(run pack 1 Big "$U9")"
printf 'XX' | dd of="$PACK" bs=1 seek="$OFF" conv=notrunc 2>/dev/null
out=$(run show 1 Big "$U9")
expect "a damaged record: the server's copy instead" "message 9" "$out"
# compaction: most bodies dropped, the pack written again without them
before=$(stat -c %s "$PACK")
out=$(run drop 1 Big 250)
after=$(stat -c %s "$PACK")
[ "$after" -lt $((before / 2)) ] && [ ! -e "$BIGDIR/MESSAGES.DA\$" ] && { PASS=$((PASS+1)); echo "ok   compaction: the pack without its gaps ($before -> $after bytes)"; } \
  || { FAIL=$((FAIL+1)); echo "FAIL compaction: $before -> $after bytes"; }
expect "compaction: every record still where the index says" "records [0-9]*, bad 0" "$(pack_ok "$BIGDIR")"
out=$(run show 1 Big "$UL")
expect "compaction: a message kept reads from the pack" "message 310" "$out"

out=$(run allread 1 Big)
out=$(run list 1 Big)
if grep -qP "^\d+\tN" <<<"$out"; then FAIL=$((FAIL+1)); echo "FAIL mark all as read: the list still has unread ones"
else PASS=$((PASS+1)); echo "ok   mark all as read: the list shows them read at once"; fi
out=$(run sync 1 Big)
expect "mark all as read reaches the server" "synced: 310 total, 0 unread" "$out"

out=$(run clearcache)
out=$(run show 1 INBOX "$U4")
expect "body downloaded again after clearing the cache" "שלום דנה," "$out"

# EMAILCHK.ACC's look (src/newmail.c): what EMail would show as new,
# changing nothing. POP3: the messages not downloaded yet
out=$(run newmail 2)
expect "checker: POP3, the messages EMail hasn't downloaded" "^newmail: 2 fresh=1" "$out"
expect "checker: POP3, told once" "^again: 2 fresh=0" "$out"
out=$(run check 2)
expect "POP3 downloads the inbox" "new: 2" "$out"
out=$(run check 2)
expect "POP3 UIDL: nothing new the second time" "new: 0" "$out"
out=$(run list 2 Inbox)
expect "POP3 message in the local inbox" "תשובה" "$out"
P1=$(uid_of "$out" "תשובה")
out=$(run show 2 Inbox "$P1")
expect "POP3 message readable" "שלום דנה," "$out"
out=$(run delete 2 Inbox "$P1")
out=$(run list 2 Trash)
expect "POP3 delete goes to the local Trash" "תשובה" "$out"
out=$(run newmail 2)
expect "checker: POP3, nothing new after EMail downloaded it" "^newmail: 0 fresh=0" "$out"

# IMAP: the unread messages that came after EMail's last look at the
# Inbox (its INDEX.DAT), and nothing is marked read or changed
run sync 1 INBOX >/dev/null
out=$(run newmail 1)
expect "checker: IMAP, nothing new after EMail synced" "^newmail: 0 fresh=0" "$out"
python3 - $IMAP <<'PY2'
import imaplib, sys
m = imaplib.IMAP4("127.0.0.1", int(sys.argv[1]))
m.login("dana", "secret")
m.append("INBOX", "()", None, b"From: avi@example.com\r\nSubject: Checker 1\r\n\r\none\r\n")
m.append("INBOX", "(\\Seen)", None, b"From: avi@example.com\r\nSubject: Read elsewhere\r\n\r\nread\r\n")
m.append("INBOX", "()", None, b"From: avi@example.com\r\nSubject: Checker 2\r\n\r\ntwo\r\n")
m.logout()
PY2
idx=$(md5sum "$W"/EMAIL/ACCT1/*/INDEX.DAT | md5sum)
out=$(run newmail 1)
expect "checker: IMAP, two unread came (one read elsewhere doesn't count)" "^newmail: 2 fresh=1" "$out"
expect "checker: IMAP, told once" "^again: 2 fresh=0" "$out"
[ "$idx" = "$(md5sum "$W"/EMAIL/ACCT1/*/INDEX.DAT | md5sum)" ] && { PASS=$((PASS+1)); echo "ok   checker: EMail's files left as they were"; } \
  || { FAIL=$((FAIL+1)); echo "FAIL checker changed EMail's indexes"; }
out=$(run check 1)
expect "checker: EMail still finds them new" "new: 2" "$out"
out=$(run newmail 1)
expect "checker: IMAP, nothing new once EMail has them" "^newmail: 0 fresh=0" "$out"

# Send now: straight from memory. An IMAP account writes nothing to the
# disk (the copy goes to the server's Sent); SEED.DAT is Falcon mode's
touch "$D/before-send"
out=$(run sendnow 1 "$W/msg.txt")
expect "Send now sends at once" "sent: 1 result: 1" "$out"
written=$(find "$W/EMAIL" -type f -newer "$D/before-send" ! -name SEED.DAT)
[ -z "$written" ] && { PASS=$((PASS+1)); echo "ok   Send now writes nothing to the disk"; } \
  || { FAIL=$((FAIL+1)); echo "FAIL Send now wrote to the disk:"; sed 's/^/     | /' <<<"$written"; }
# a POP account keeps its copy in the local Sent
out=$(run sendnow 2 "$W/msg.txt")
expect "Send now from a POP account" "sent: 1 result: 1" "$out"
out=$(run list 2 Sent)
expect "its copy is in the local Sent" "תשובה" "$out"
# the server away: the message waits in the Outbox, and goes with the next one
cp "$W/EMAIL.INF" "$W/EMAIL.INF.keep"
sed -i "0,/^smtpport=.*/s//smtpport=$((SMTP + 900))/; 0,/^dsmtpport=.*/s//dsmtpport=$((SMTP + 900))/" "$W/EMAIL.INF"
out=$(run sendnow 1 "$W/msg.txt")
expect "no server: queued in the Outbox" "^queued: " "$out"
expect "no server: nothing sent" "sent: 0 result: -1" "$out"
cp "$W/EMAIL.INF.keep" "$W/EMAIL.INF"
out=$(run sendnow 1 "$W/msg.txt")
expect "the next Send now takes the Outbox along" "sent: 2 result: 1" "$out"
out=$(run list 1 Outbox)
[ -z "$(grep -v '^ *$' <<<"$out")" ] && { PASS=$((PASS+1)); echo "ok   the Outbox is empty again"; } \
  || { FAIL=$((FAIL+1)); echo "FAIL the Outbox still holds:"; sed 's/^/     | /' <<<"$out"; }

# invitations for ECal: an inline text/calendar alternative and an .ics
# attachment, each decoded to INVITE.ICS as it came (Add to ECal)
python3 - $IMAP "$D" <<'PY'
import base64, imaplib, sys
ics1 = ("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nMETHOD:REQUEST\r\nBEGIN:VEVENT\r\n"
        "UID:lunch-1@example.com\r\nDTSTART:20261023T120000Z\r\nDTEND:20261023T130000Z\r\n"
        "SUMMARY;LANGUAGE=en:Team lunch\\, Friday שלום and a ver\r\n y long title\r\n"
        "END:VEVENT\r\nEND:VCALENDAR\r\n").encode("utf-8")
ics2 = (b"BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:review-2@example.com\r\n"
        b"DTSTART:20261026T090000Z\r\nSUMMARY:Design review\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n")
open(sys.argv[2] + "/invite1.ics", "wb").write(ics1)
open(sys.argv[2] + "/invite2.ics", "wb").write(ics2)
b64 = lambda b: b"\r\n".join(base64.encodebytes(b).splitlines())
m1 = (b"From: Dana <dana@example.com>\r\nTo: dana@test.local\r\nSubject: Lunch\r\nMIME-Version: 1.0\r\n"
      b"Content-Type: multipart/alternative; boundary=AA\r\n\r\n--AA\r\nContent-Type: text/plain\r\n\r\n"
      b"Lunch on Friday?\r\n--AA\r\nContent-Type: text/calendar; method=REQUEST; charset=utf-8\r\n"
      b"Content-Transfer-Encoding: base64\r\n\r\n" + b64(ics1) + b"\r\n--AA--\r\n")
m2 = (b"From: Avi <avi@example.org>\r\nTo: dana@test.local\r\nSubject: Review\r\nMIME-Version: 1.0\r\n"
      b"Content-Type: multipart/mixed; boundary=BB\r\n\r\n--BB\r\nContent-Type: text/plain\r\n\r\nSee the invite.\r\n"
      b"--BB\r\nContent-Type: application/ics; name=meeting.ics\r\nContent-Disposition: attachment; filename=meeting.ics\r\n"
      b"Content-Transfer-Encoding: base64\r\n\r\n" + b64(ics2) + b"\r\n--BB--\r\n")
m3 = b"From: Avi <avi@example.org>\r\nTo: dana@test.local\r\nSubject: Plain\r\n\r\nNo invitation here.\r\n"
m = imaplib.IMAP4("127.0.0.1", int(sys.argv[1]))
m.login("dana", "secret")
m.create("Invites")
for x in (m1, m2, m3):
    m.append("Invites", "()", None, x)
m.logout()
PY
out=$(run sync 1 Invites)
expect "the invitations' folder synced" "synced: 3 total" "$out"
out=$(run show 1 Invites 1)
expect "an inline invitation is a part, for ECal" "Attachment: invite.ics (text/calendar" "$out"
expect "its summary: unescaped, unfolded, Hebrew in the Atari set" "Invitation: Team lunch, Friday .* and a very long title" "$out"
expect "the inline invitation's text is still the message" "Lunch on Friday?" "$out"
out=$(run invite 1 Invites 1 "$W/INVITE.ICS")
cmp -s "$W/INVITE.ICS" "$D/invite1.ics" && { PASS=$((PASS+1)); echo "ok   INVITE.ICS is the inline invitation, byte for byte"; } \
  || { FAIL=$((FAIL+1)); echo "FAIL INVITE.ICS differs from the invitation sent"; echo "$out"; }
out=$(run show 1 Invites 2)
expect "an .ics attachment is an invitation" "Invitation: Design review" "$out"
out=$(run invite 1 Invites 2 "$W/INVITE.ICS")
cmp -s "$W/INVITE.ICS" "$D/invite2.ics" && { PASS=$((PASS+1)); echo "ok   INVITE.ICS overwritten by the attachment, byte for byte"; } \
  || { FAIL=$((FAIL+1)); echo "FAIL INVITE.ICS differs from the attachment"; echo "$out"; }
out=$(run invite 1 Invites 3 "$W/INVITE.ICS")
expect "a message without one has no invitation" "no invitation" "$out"

if [ -n "${FALCON:-}" ]; then
  out=$(run send 2 "$W/msg.txt")
  expect "message sent with SMTP STARTTLS" "sent: 1" "$out"

  # IMAP STARTTLS on the plain port
  cp "$W/EMAIL.INF" "$W/EMAIL.INF.keep"
  sed -i "0,/^dport=.*/s//dport=$IMAP/; 0,/^dsec=1/s//dsec=2/" "$W/EMAIL.INF"
  out=$(run folders 1)
  expect "IMAP STARTTLS" "^Sent " "$out"
  grep -q "^[0-9:.]* IMAP >> T[0-9]* STARTTLS" "$W/EMAIL.LOG" && { PASS=$((PASS+1)); echo "ok   IMAP sent STARTTLS"; } || { FAIL=$((FAIL+1)); echo "FAIL IMAP STARTTLS not in the log"; }
  cp "$W/EMAIL.INF.keep" "$W/EMAIL.INF"

  # Falcon mode has its own password: a wrong one there fails in words,
  # and the log shows the login's shape but not the login
  cp "$W/EMAIL.INF" "$W/EMAIL.INF.keep"
  python3 - "$W/EMAIL.INF" <<'PY'
import sys
p = sys.argv[1]
s = open(p, newline="").read()
i = s.index("[account]", s.index("[account]") + 1)   # end of the first account
s = s[:i] + "duser=dana\r\ndpass=wrong-pass\r\n" + s[i:]
open(p, "w", newline="").write(s)
PY
  out=$(run folders 1)
  expect "wrong Falcon-mode password refused, in words" "refused the user name or password" "$out"
  grep -q "^[0-9:.]* IMAP -- logging in: user name 4 characters without @, password 10 characters (1 '-'" "$W/EMAIL.LOG" \
    && { PASS=$((PASS+1)); echo "ok   login shape in the log"; } || { FAIL=$((FAIL+1)); echo "FAIL no login shape in the log"; }
  grep -q "wrong-pass" "$W/EMAIL.LOG" && { FAIL=$((FAIL+1)); echo "FAIL password in the log"; } || { PASS=$((PASS+1)); echo "ok   password not in the log"; }
  grep -q "^pass=secret" "$W/EMAIL.INF" && { PASS=$((PASS+1)); echo "ok   the gateway's password kept apart"; } || { FAIL=$((FAIL+1)); echo "FAIL gateway password changed"; }
  cp "$W/EMAIL.INF.keep" "$W/EMAIL.INF"

  # a server whose certificate no trusted authority signed
  openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj /CN=other -keyout "$D/k2.pem" -out "$D/c2.pem" 2>/dev/null
  cp "$W/CACERT.PEM" "$W/CACERT.keep"; cp "$D/c2.pem" "$W/CACERT.PEM"
  out=$(run folders 1)
  expect "unknown authority refused, in words" "isn't signed by any authority in CACERT.PEM" "$out"
  cp "$W/CACERT.keep" "$W/CACERT.PEM"

  # the right authority, the wrong name: 127.0.0.1 against localhost's
  # certificate (TLS.OVL 1.1 checks the chain after the handshake, the
  # name too, before anything is sent)
  cp "$W/EMAIL.INF" "$W/EMAIL.INF.keep"
  sed -i "0,/^dhost=localhost/s//dhost=127.0.0.1/" "$W/EMAIL.INF"
  out=$(run folders 1)
  expect "a certificate for another name refused, in words" "a different server name" "$out"
  grep -q "^[0-9:.]* TLS -- the server's [0-9]* certificates\?, [0-9]* bytes: kept, to be checked after the handshake" "$W/EMAIL.LOG" \
    && { PASS=$((PASS+1)); echo "ok   the chain kept during the handshake, checked after it"; } \
    || { FAIL=$((FAIL+1)); echo "FAIL the chain not checked after the handshake"; }
  cp "$W/EMAIL.INF.keep" "$W/EMAIL.INF"

  # a server with only an ECDSA certificate: refused for RSA-only, then accepted
  openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -days 2 -subj /CN=localhost \
    -addext subjectAltName=DNS:localhost -keyout "$D/eck.pem" -out "$D/ecc.pem" 2>/dev/null
  chmod 644 "$D/eck.pem"
  cat "$D/ecc.pem" >> "$W/CACERT.PEM"
  python3 "$ROOT/tests/smtp_server.py" $((SMTP + 441)) "$D" dana secret "$D/ecc.pem" "$D/eck.pem" & SMTPPID="$SMTPPID $!"
  sleep 1
  sed -i "0,/^dsmtpport=.*/s//dsmtpport=$((SMTP + 441))/" "$W/EMAIL.INF"
  out=$(run send 1 "$W/msg.txt")
  expect "ECDSA-only server: sent after the retry" "sent: 1" "$out"
  grep -q "^[0-9:.]* SMTP -- secure: TLS 1.2, ECDHE-ECDSA" "$W/EMAIL.LOG" && { PASS=$((PASS+1)); echo "ok   ECDSA certificate accepted on the second try"; } || { FAIL=$((FAIL+1)); echo "FAIL no ECDSA session"; }
  cp "$W/EMAIL.INF.keep" "$W/EMAIL.INF"

  # a server without ChaCha20: refused with ChaCha20 alone, then AES-GCM
  CIPHERS=ECDHE-RSA-AES128-GCM-SHA256 python3 "$ROOT/tests/smtp_server.py" $((SMTP + 442)) "$D" dana secret \
    "$D/cert.pem" "$D/key.pem" & SMTPPID="$SMTPPID $!"
  sleep 1
  sed -i "0,/^dsmtpport=.*/s//dsmtpport=$((SMTP + 442))/" "$W/EMAIL.INF"
  out=$(run send 1 "$W/msg.txt")
  expect "AES-only server: sent after offering every cipher" "sent: 1" "$out"
  grep -q "^[0-9:.]* SMTP -- no ChaCha20 here" "$W/EMAIL.LOG" && grep -q "^[0-9:.]* SMTP -- secure: TLS 1.2, ECDHE-RSA, AES-128-GCM" "$W/EMAIL.LOG" \
    && { PASS=$((PASS+1)); echo "ok   AES-GCM after ChaCha20 alone was refused"; } || { FAIL=$((FAIL+1)); echo "FAIL no fallback to AES-GCM"; }
  cp "$W/EMAIL.INF.keep" "$W/EMAIL.INF"
  # every protocol went over EMail's own TLS, both ways of starting it
  for p in IMAP POP3 SMTP; do
    if grep -q "^[0-9:.]* $p -- secure: TLS 1.2" "$W/EMAIL.LOG" 2>/dev/null; then PASS=$((PASS+1)); echo "ok   $p over TLS: $(grep -m1 "^[0-9:.]* $p -- secure" "$W/EMAIL.LOG" | cut -c29-)"
    else FAIL=$((FAIL+1)); echo "FAIL $p not over TLS"; fi
  done
  if grep -q "^[0-9:.]* IMAP >> T[0-9]* STARTTLS\|^[0-9:.]* POP3 >> STLS" "$W/EMAIL.LOG" && grep -q "^[0-9:.]* SMTP >> STARTTLS" "$W/EMAIL.LOG"; then
    PASS=$((PASS+1)); echo "ok   STARTTLS used where asked"
  else FAIL=$((FAIL+1)); echo "FAIL STARTTLS not used"; fi
  if grep -q " >> .*\(LOGIN\|AUTH\|PASS\)" "$W/EMAIL.LOG" && ! grep -q "secret" "$W/EMAIL.LOG"; then
    PASS=$((PASS+1)); echo "ok   password not in the log"
  else FAIL=$((FAIL+1)); echo "FAIL password check in the log"; fi
  [ -s "$W/EMAIL/SEED.DAT" ] && { PASS=$((PASS+1)); echo "ok   random seed kept"; } || { FAIL=$((FAIL+1)); echo "FAIL no random seed"; }
fi
# Gmail refuses a normal Google password: EMail says it takes an app
# password, known by what Google's servers say (the gateway passes it on
# as it is); another server's refusal stays as it was
G=$((IMAP + 700))
python3 "$ROOT/tests/gmail_like.py" $G $((G + 1)) $((G + 2)) & GPID=$!
python3 "$ROOT/tests/gmail_like.py" $((G + 3)) $((G + 4)) $((G + 5)) appspecific & GPID="$GPID $!"
mkdir -p "$D/gmail"
cat > "$D/gmail/EMAIL.INF" <<INF
[account]
name=Gmail IMAP
email=dana@gmail.com
in=imap
host=127.0.0.1
port=$G
user=dana@gmail.com
pass=my-google-password
smtphost=127.0.0.1
smtpport=$((G + 2))
[account]
name=Gmail POP
email=dana@gmail.com
in=pop3
host=127.0.0.1
port=$((G + 1))
user=dana@gmail.com
pass=my-google-password
[account]
name=Gmail 2-step
email=dana@gmail.com
in=imap
host=127.0.0.1
port=$((G + 3))
user=dana@gmail.com
pass=my-google-password
[account]
name=Not Gmail
email=dana@test.local
in=imap
host=127.0.0.1
port=$IMAP
user=dana
pass=not-the-password
INF
sleep 1
grun() { ASAN_OPTIONS=detect_leaks=0 "$CLI" "$D/gmail" "$@" 2>&1; }
APPPW="Gmail refused the password: it takes an app password from myaccount.google.com/apppasswords"
expect "Gmail IMAP: the refusal says it takes an app password" "$APPPW" "$(grun check 1)"
expect "Gmail POP3: the same" "$APPPW" "$(grun check 2)"
expect "Gmail with 2-step verification (Application-specific password required): the same" "$APPPW" "$(grun check 3)"
expect "Gmail SMTP: the same" "$APPPW" "$(grun send 1 "$W/msg.txt")"
out=$(grun check 4)
expect "another server's refusal stays as it was" "refused the user name or password" "$out"
if grep -q "app password" <<<"$out"; then FAIL=$((FAIL+1)); echo "FAIL Gmail's message for another server"; else PASS=$((PASS+1)); echo "ok   ... and doesn't mention Gmail"; fi
kill $GPID 2>/dev/null

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
