#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# EMail's mail core against the providers' own servers, the ones EMail
# fills in for a Direct connection (src/store.c's presets): Gmail, iCloud,
# Yahoo, GMX, web.de, AOL, Fastmail and Zoho. For each, both ways a user
# reaches them:
#   Direct connection  EMail's TLS (BearSSL) to the provider's IMAP (993),
#                      POP3 (995, where there is one) and SMTP (465, or
#                      587 with STARTTLS), the certificates checked against
#                      the release's CACERT.PEM;
#   the gateway        gateway/emailgate.py here as on the Pi, installed for
#                      that provider, EMail plain to it.
# Each must get as far as the provider refusing the login: connected,
# secure (Direct), greeted, the login answered with a refusal. The login
# is one no one has (aera-probe-...), so no one's account is touched;
# Gmail's refusal must say Gmail takes an app password. What comes after a
# real login needs real accounts. Needs the Internet (the GitHub runner:
# Actions > Providers > Run workflow); `make build/mail-cli` first.
#
#   providers_probe.sh [PROVIDER...]      (all of them without)
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CLI=$ROOT/build/mail-cli
D=$(mktemp -d /tmp/providers-probe.XXXXXX)
PASS=0; FAIL=0
APPPW="Gmail refused the password: it takes an app password from myaccount.google.com/apppasswords"
PW=not-an-app-password
export TZ=UTC MAIL_LOG=1 ASAN_OPTIONS=detect_leaks=0
GW=
trap 'kill $GW 2>/dev/null; rm -rf "$D"' EXIT

# name domain IMAP POP3 SMTP SMTP-port (POP3 "-": none)
PROVIDERS="
gmail     gmail.com    imap.gmail.com      pop.gmail.com      smtp.gmail.com      465
icloud    icloud.com   imap.mail.me.com    -                  smtp.mail.me.com    587
yahoo     yahoo.com    imap.mail.yahoo.com pop.mail.yahoo.com smtp.mail.yahoo.com 465
gmx       gmx.net      imap.gmx.net        pop.gmx.net        mail.gmx.net        465
webde     web.de       imap.web.de         pop3.web.de        smtp.web.de         587
aol       aol.com      imap.aol.com        pop.aol.com        smtp.aol.com        465
fastmail  fastmail.com imap.fastmail.com   pop.fastmail.com   smtp.fastmail.com   465
zoho      zoho.com     imap.zoho.com       pop.zoho.com       smtp.zoho.com       465
"
REFUSED="app password\|refused the user name or password\|login failed\|SMTP login failed"
BROKEN="can't find\|can't connect\|certificate\|TLS error\|connection was lost\|timed out\|did not greet\|closed the connection\|doesn't offer"
SUMMARY=

cat > "$D/msg.txt" <<'MSG'
To: aera-probe@example.com
Subject: never sent

The login is refused first.
MSG

# one try: refused by the provider (ok), or broken before the login (FAIL)
try() {  # try NAME WANT OUTPUT
  local name=$1 want=$2 out=$3 why
  why=$(grep -m1 "^error:" <<<"$out" | cut -c1-150)
  if grep -q -- "$want" <<<"$out" && ! grep -q -- "$BROKEN" <<<"$out"; then
    PASS=$((PASS+1)); echo "ok   $name  ($why)"; RES="$RES ok"
  else
    FAIL=$((FAIL+1)); echo "FAIL $name"; sed 's/^/     | /' <<<"$out" | head -8; RES="$RES FAIL"
    # what was said, from EMail's protocol log (the password never is)
    [ -f "$LOG" ] && tail -n 12 "$LOG" | sed 's/^/     log | /'
  fi
}

account() {  # account NAME USER IN HOST PORT [Direct: DHOST DPORT DSEC DSMTP DSMTPPORT DSMTPSEC]
  printf '[account]\r\nname=%s\r\nemail=%s\r\nin=%s\r\nhost=%s\r\nport=%s\r\nuser=%s\r\npass=%s\r\nsmtphost=%s\r\nsmtpport=%s\r\n' \
    "$1" "$2" "$3" "$4" "$5" "$2" "$PW" "$4" "$6"
  [ $# -gt 6 ] && printf 'dhost=%s\r\ndport=%s\r\ndsec=%s\r\ndsmtphost=%s\r\ndsmtpport=%s\r\ndsmtpsec=%s\r\n' \
    "$7" "$8" "$9" "${10}" "${11}" "${12}"
}

want_for() { [ "$1" = gmail ] && echo "$APPPW" || echo "$REFUSED"; }

port=10200
while read -r name domain imap pop smtp sport; do
  [ -z "$name" ] && continue
  if [ $# -gt 0 ] && ! grep -qw "$name" <<<"$*"; then continue; fi
  user="aera-probe-q7x2k9@$domain"
  [ "$name" = gmail ] && user="no-such-user@gmail.com"   # Gmail names have no '-'
  want=$(want_for "$name")
  ssec=$([ "$sport" = 587 ] && echo 2 || echo 1)
  echo "== $name"
  RES=

  # Direct: EMail's own TLS
  W=$D/$name-direct; mkdir -p "$W"
  cp "$ROOT/common/certs/CACERT.PEM" "$W/CACERT.PEM"
  {
    printf '[options]\r\ntz=0\r\nfalcon=1\r\nlog=1\r\n'
    account "$name IMAP" "$user" imap 192.168.1.2 143 587 "$imap" 993 1 "$smtp" "$sport" "$ssec"
    [ "$pop" != - ] && account "$name POP" "$user" pop3 192.168.1.2 110 587 "$pop" 995 1 "$smtp" "$sport" "$ssec"
  } > "$W/EMAIL.INF"
  LOG=$W/EMAIL.LOG; rm -f "$LOG"
  try "$name, Direct, IMAP $imap:993" "$want" "$("$CLI" "$W" check 1 2>&1)"
  if [ "$pop" != - ]; then try "$name, Direct, POP3 $pop:995" "$want" "$("$CLI" "$W" check 2 2>&1)"; else RES="$RES -"; fi
  try "$name, Direct, SMTP $smtp:$sport" "$want" "$("$CLI" "$W" send 1 "$D/msg.txt" 2>&1)"
  grep -h -- "-- secure:" "$W/EMAIL.LOG" 2>/dev/null | cut -c13- | sort -u | sed 's/^/     | /'

  # the gateway, as on the Pi, for this provider
  G=$D/$name-gateway; mkdir -p "$G"
  pi=$port; ps=$((port + 1)); pp=$((port + 2)); port=$((port + 3))
  gsmtp=$smtp; [ "$sport" = 587 ] && gsmtp="$smtp:587"
  gpop=; [ "$pop" != - ] && gpop=$pop
  python3 "$ROOT/gateway/emailgate.py" --bind 127.0.0.1 -v --ports $pi,$ps,$pp $imap $gsmtp $gpop \
    2>"$D/$name-emailgate.log" & GW=$!
  sleep 2
  {
    printf '[options]\r\ntz=0\r\nlog=1\r\n'
    account "$name IMAP" "$user" imap 127.0.0.1 $pi $ps
    [ "$pop" != - ] && account "$name POP" "$user" pop3 127.0.0.1 $pp $ps
  } > "$G/EMAIL.INF"
  LOG=$G/EMAIL.LOG
  try "$name, gateway, IMAP" "$want" "$("$CLI" "$G" check 1 2>&1)"
  if [ "$pop" != - ]; then try "$name, gateway, POP3" "$want" "$("$CLI" "$G" check 2 2>&1)"; else RES="$RES -"; fi
  try "$name, gateway, SMTP" "$want" "$("$CLI" "$G" send 1 "$D/msg.txt" 2>&1)"
  kill $GW 2>/dev/null; wait $GW 2>/dev/null; GW=
  grep -h " -> .*TLS\|refus\|can't" "$D/$name-emailgate.log" | sed 's/^/     | /'
  SUMMARY="$SUMMARY$(printf '%-9s %-5s %-5s %-5s   %-5s %-5s %-5s' "$name" $RES)
"
done <<<"$PROVIDERS"

echo
echo "provider  Direct: IMAP  POP3  SMTP   gateway: IMAP  POP3  SMTP"
printf '%s' "$SUMMARY"
echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
