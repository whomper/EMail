#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# EMail's mail core against Google's own servers, both ways a user reaches
# them, with a login that can't exist (no-such-user@gmail.com: Gmail names
# have no '-'), so no one's account is touched:
#   Direct connection  EMail's TLS (BearSSL) to imap.gmail.com:993,
#                      pop.gmail.com:995, smtp.gmail.com:465 and :587
#                      (STARTTLS), Google's certificates checked against
#                      the release's CACERT.PEM;
#   the gateway        gateway/emailgate.py here as on the Pi, EMail plain
#                      to it.
# Each must get as far as Google refusing the login, and EMail must then
# say that Gmail takes an app password. Needs the Internet (the GitHub
# runner: Actions > Gmail > Run workflow); `make build/mail-cli` first.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CLI=$ROOT/build/mail-cli
D=$(mktemp -d /tmp/gmail-probe.XXXXXX)
PASS=0; FAIL=0
APPPW="Gmail refused the password: it takes an app password from myaccount.google.com/apppasswords"
USER_=no-such-user@gmail.com
PW=not-an-app-password
export TZ=UTC MAIL_LOG=1 ASAN_OPTIONS=detect_leaks=0

expect() {  # expect DESCRIPTION PATTERN OUTPUT
  if grep -q -- "$2" <<<"$3"; then PASS=$((PASS+1)); echo "ok   $1"
  else FAIL=$((FAIL+1)); echo "FAIL $1"; echo "     wanted: $2"; sed 's/^/     | /' <<<"$3"; fi
}
cat > "$D/msg.txt" <<'MSG'
To: no-such-user@gmail.com
Subject: never sent

The login is refused first.
MSG

# the Direct connection: EMail's own TLS
W=$D/direct; mkdir -p "$W"
cp "$ROOT/common/certs/CACERT.PEM" "$W/CACERT.PEM"
cat > "$W/EMAIL.INF" <<INF
[options]
tz=0
falcon=1
log=1
[account]
name=Gmail IMAP
email=$USER_
in=imap
host=192.168.1.2
port=143
user=$USER_
pass=$PW
dhost=imap.gmail.com
dport=993
dsec=1
dsmtphost=smtp.gmail.com
dsmtpport=465
dsmtpsec=1
[account]
name=Gmail POP
email=$USER_
in=pop3
host=192.168.1.2
port=110
user=$USER_
pass=$PW
dhost=pop.gmail.com
dport=995
dsec=1
dsmtphost=smtp.gmail.com
dsmtpport=587
dsmtpsec=2
INF
run() { "$CLI" "$W" "$@" 2>&1; }
expect "Direct, IMAP 993: Google refuses the login, EMail says app password" "$APPPW" "$(run check 1)"
expect "Direct, POP3 995: the same" "$APPPW" "$(run check 2)"
expect "Direct, SMTP 465: the same" "$APPPW" "$(run send 1 "$D/msg.txt")"
expect "Direct, SMTP 587 with STARTTLS: the same" "$APPPW" "$(run send 2 "$D/msg.txt")"
for p in IMAP POP3 SMTP; do
  expect "Direct, $p over EMail's TLS" "^[0-9:.]* $p -- secure: TLS 1.2" "$(cat "$W/EMAIL.LOG" 2>/dev/null)"
done
echo "     the sessions:"; grep -h -- "-- secure:" "$W/EMAIL.LOG" | cut -c13- | sort -u | sed 's/^/     | /'

# the gateway, as on the Pi
G=$D/gateway; mkdir -p "$G"
python3 "$ROOT/gateway/emailgate.py" --bind 127.0.0.1 -v --ports 10143,10587,10110 \
  imap.gmail.com smtp.gmail.com pop.gmail.com 2>"$D/emailgate.log" & GW=$!
trap 'kill $GW 2>/dev/null; rm -rf "$D"' EXIT
sleep 2
cat > "$G/EMAIL.INF" <<INF
[options]
tz=0
log=1
[account]
name=Gmail IMAP
email=$USER_
in=imap
host=127.0.0.1
port=10143
user=$USER_
pass=$PW
smtphost=127.0.0.1
smtpport=10587
[account]
name=Gmail POP
email=$USER_
in=pop3
host=127.0.0.1
port=10110
user=$USER_
pass=$PW
INF
grun() { "$CLI" "$G" "$@" 2>&1; }
expect "Gateway, IMAP: the same" "$APPPW" "$(grun check 1)"
expect "Gateway, POP3: the same" "$APPPW" "$(grun check 2)"
expect "Gateway, SMTP: the same" "$APPPW" "$(grun send 1 "$D/msg.txt")"
echo "     what Google said:"; grep -h " << " "$W/EMAIL.LOG" "$G/EMAIL.LOG" | cut -c13- | grep -v "^ *SMTP << 250-" | sort -u | sed 's/^/     | /'
echo "     the gateway:"; sed 's/^/     | /' "$D/emailgate.log"
echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
