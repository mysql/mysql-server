#!/bin/sh -ex

# Copyright (c) 2021 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

# usage: ./test.sh "$(mktemp -d fido2test-XXXXXXXX)" device

# Please note that this test script:
# - is incomplete;
# - assumes CTAP 2.1-like hmac-secret;
# - should pass as-is on a YubiKey with a PIN set;
# - may otherwise require set +e above;
# - can be executed with UV=1 to run additional UV tests;
# - was last tested on 2021-07-21 with firmware 5.2.7.

cd "$1"
DEV="$2"

make_cred() {
	cat > cred_param << EOF
$(dd if=/dev/urandom bs=32 count=1 2>/dev/null | base64)
$1
some user name
$(dd if=/dev/urandom bs=32 count=1 2>/dev/null | base64)
EOF
	fido2-cred -M $2 "${DEV}" > "$3" < cred_param
}

verify_cred() {
	fido2-cred -V $1 > cred_out < "$2"
	head -1 cred_out > "$3"
	tail -n +2 cred_out > "$4"
}

get_assert() {
	cat > assert_param << EOF
$(dd if=/dev/urandom bs=32 count=1 2>/dev/null | base64)
$1
$(cat $3)
$(cat $4)
EOF
	fido2-assert -G $2 "${DEV}" > "$5" < assert_param
}

verify_assert() {
	fido2-assert -V $1 "$2" < "$3"
}

dd if=/dev/urandom bs=32 count=1 | base64 > hmac-salt

# u2f
make_cred no.tld "-u" u2f
! make_cred no.tld "-ru" /dev/null
! make_cred no.tld "-uc1" /dev/null
! make_cred no.tld "-uc2" /dev/null
verify_cred "--"  u2f u2f-cred u2f-pubkey
! verify_cred "-h" u2f /dev/null /dev/null
! verify_cred "-v" u2f /dev/null /dev/null
verify_cred "-c0" u2f /dev/null /dev/null
! verify_cred "-c1" u2f /dev/null /dev/null
! verify_cred "-c2" u2f /dev/null /dev/null
! verify_cred "-c3" u2f /dev/null /dev/null

# wrap (non-resident)
make_cred no.tld "--" wrap
verify_cred "--" wrap wrap-cred	wrap-pubkey
! verify_cred "-h" wrap	/dev/null /dev/null
! verify_cred "-v" wrap	/dev/null /dev/null
verify_cred "-c0" wrap /dev/null /dev/null
! verify_cred "-c1" wrap /dev/null /dev/null
! verify_cred "-c2" wrap /dev/null /dev/null
! verify_cred "-c3" wrap /dev/null /dev/null

# wrap (non-resident) + hmac-secret
make_cred no.tld "-h" wrap-hs
! verify_cred "--" wrap-hs /dev/null /dev/null
verify_cred "-h" wrap-hs wrap-hs-cred wrap-hs-pubkey
! verify_cred "-v" wrap-hs /dev/null /dev/null
verify_cred "-hc0" wrap-hs /dev/null /dev/null
! verify_cred "-c0" wrap-hs /dev/null /dev/null
! verify_cred "-c1" wrap-hs /dev/null /dev/null
! verify_cred "-c2" wrap-hs /dev/null /dev/null
! verify_cred "-c3" wrap-hs /dev/null /dev/null

# resident
make_cred no.tld "-r" rk
verify_cred "--" rk rk-cred rk-pubkey
! verify_cred "-h" rk /dev/null /dev/null
! verify_cred "-v" rk /dev/null /dev/null
verify_cred "-c0" rk /dev/null /dev/null
! verify_cred "-c1" rk /dev/null /dev/null
! verify_cred "-c2" rk /dev/null /dev/null
! verify_cred "-c3" rk /dev/null /dev/null

# resident + hmac-secret
make_cred no.tld "-hr" rk-hs
! verify_cred  "--" rk-hs rk-hs-cred rk-hs-pubkey
verify_cred "-h" rk-hs /dev/null /dev/null
! verify_cred "-v" rk-hs /dev/null /dev/null
verify_cred "-hc0" rk-hs /dev/null /dev/null
! verify_cred "-c0" rk-hs /dev/null /dev/null
! verify_cred "-c1" rk-hs /dev/null /dev/null
! verify_cred "-c2" rk-hs /dev/null /dev/null
! verify_cred "-c3" rk-hs /dev/null /dev/null

# u2f
get_assert no.tld "-u" u2f-cred /dev/null u2f-assert
! get_assert no.tld "-u -t up=false" u2f-cred /dev/null /dev/null
verify_assert "--"  u2f-pubkey u2f-assert
verify_assert "-p"  u2f-pubkey u2f-assert

# wrap (non-resident)
get_assert no.tld "--" wrap-cred /dev/null wrap-assert
verify_assert "--" wrap-pubkey wrap-assert
get_assert no.tld "-t pin=true" wrap-cred /dev/null wrap-assert
verify_assert "--" wrap-pubkey wrap-assert
verify_assert "-v" wrap-pubkey wrap-assert
get_assert no.tld "-t pin=false" wrap-cred /dev/null wrap-assert
verify_assert "--" wrap-pubkey wrap-assert
get_assert no.tld "-t up=true" wrap-cred /dev/null wrap-assert
verify_assert "-p" wrap-pubkey wrap-assert
get_assert no.tld "-t up=true -t pin=true" wrap-cred /dev/null wrap-assert
verify_assert "--" wrap-pubkey wrap-assert
verify_assert "-p" wrap-pubkey wrap-assert
verify_assert "-v" wrap-pubkey wrap-assert
verify_assert "-pv" wrap-pubkey wrap-assert
get_assert no.tld "-t up=true -t pin=false" wrap-cred /dev/null wrap-assert
verify_assert "--" wrap-pubkey wrap-assert
verify_assert "-p" wrap-pubkey wrap-assert
get_assert no.tld "-t up=false" wrap-cred /dev/null wrap-assert
verify_assert "--" wrap-pubkey wrap-assert
! verify_assert "-p" wrap-pubkey wrap-assert
get_assert no.tld "-t up=false -t pin=true" wrap-cred /dev/null wrap-assert
! verify_assert "-p" wrap-pubkey wrap-assert
verify_assert "-v" wrap-pubkey wrap-assert
! verify_assert "-pv" wrap-pubkey wrap-assert
get_assert no.tld "-t up=false -t pin=false" wrap-cred /dev/null wrap-assert
! verify_assert "-p" wrap-pubkey wrap-assert
get_assert no.tld "-h" wrap-cred hmac-salt wrap-assert
! verify_assert "--" wrap-pubkey wrap-assert
verify_assert "-h" wrap-pubkey wrap-assert
get_assert no.tld "-h -t pin=true" wrap-cred hmac-salt wrap-assert
! verify_assert "--" wrap-pubkey wrap-assert
verify_assert "-h" wrap-pubkey wrap-assert
verify_assert "-hv" wrap-pubkey wrap-assert
get_assert no.tld "-h -t pin=false" wrap-cred hmac-salt wrap-assert
! verify_assert "--" wrap-pubkey wrap-assert
verify_assert "-h" wrap-pubkey wrap-assert
get_assert no.tld "-h -t up=true" wrap-cred hmac-salt wrap-assert
! verify_assert "--" wrap-pubkey wrap-assert
verify_assert "-h" wrap-pubkey wrap-assert
verify_assert "-hp" wrap-pubkey wrap-assert
get_assert no.tld "-h -t up=true -t pin=true" wrap-cred hmac-salt wrap-assert
! verify_assert "--" wrap-pubkey wrap-assert
verify_assert "-h" wrap-pubkey wrap-assert
verify_assert "-hp" wrap-pubkey wrap-assert
verify_assert "-hv" wrap-pubkey wrap-assert
verify_assert "-hpv" wrap-pubkey wrap-assert
get_assert no.tld "-h -t up=true -t pin=false" wrap-cred hmac-salt wrap-assert
! verify_assert "--" wrap-pubkey wrap-assert
verify_assert "-h" wrap-pubkey wrap-assert
verify_assert "-hp" wrap-pubkey wrap-assert
! get_assert no.tld "-h -t up=false" wrap-cred hmac-salt wrap-assert
! get_assert no.tld "-h -t up=false -t pin=true" wrap-cred hmac-salt wrap-assert
! get_assert no.tld "-h -t up=false -t pin=false" wrap-cred hmac-salt wrap-assert

if [ "x${UV}" != "x" ]; then
	get_assert no.tld "-t uv=true" wrap-cred /dev/null wrap-assert
	verify_assert "-v" wrap-pubkey wrap-assert
	get_assert no.tld "-t uv=true -t pin=true" wrap-cred /dev/null wrap-assert
	verify_assert "-v" wrap-pubkey wrap-assert
	get_assert no.tld "-t uv=true -t pin=false" wrap-cred /dev/null wrap-assert
	verify_assert "-v" wrap-pubkey wrap-assert
	get_assert no.tld "-t uv=false" wrap-cred /dev/null wrap-assert
	verify_assert "--" wrap-pubkey wrap-assert
	get_assert no.tld "-t uv=false -t pin=true" wrap-cred /dev/null wrap-assert
	verify_assert "-v" wrap-pubkey wrap-assert
	get_assert no.tld "-t uv=false -t pin=false" wrap-cred /dev/null wrap-assert
	verify_assert "--" wrap-pubkey wrap-assert
	get_assert no.tld "-t up=true -t uv=true" wrap-cred /dev/null wrap-assert
	verify_assert "-pv" wrap-pubkey wrap-assert
	get_assert no.tld "-t up=true -t uv=true -t pin=true" wrap-cred /dev/null wrap-assert
	verify_assert "-pv" wrap-pubkey wrap-assert
	get_assert no.tld "-t up=true -t uv=true -t pin=false" wrap-cred /dev/null wrap-assert
	verify_assert "-pv" wrap-pubkey wrap-assert
	get_assert no.tld "-t up=true -t uv=false" wrap-cred /dev/null wrap-assert
	verify_assert "-p" wrap-pubkey wrap-assert
	get_assert no.tld "-t up=true -t uv=false -t pin=true" wrap-cred /dev/null wrap-assert
	verify_assert "-pv" wrap-pubkey wrap-assert
	get_assert no.tld "-t up=true -t uv=false -t pin=false" wrap-cred /dev/null wrap-assert
	verify_assert "-p" wrap-pubkey wrap-assert
	get_assert no.tld "-t up=false -t uv=true" wrap-cred /dev/null wrap-assert
	verify_assert "-v" wrap-pubkey wrap-assert
	get_assert no.tld "-t up=false -t uv=true -t pin=true" wrap-cred /dev/null wrap-assert
	verify_assert "-v" wrap-pubkey wrap-assert
	get_assert no.tld "-t up=false -t uv=true -t pin=false" wrap-cred /dev/null wrap-assert
	verify_assert "-v" wrap-pubkey wrap-assert
	get_assert no.tld "-t up=false -t uv=false" wrap-cred /dev/null wrap-assert
	! verify_assert "--" wrap-pubkey wrap-assert
	get_assert no.tld "-t up=false -t uv=false -t pin=true" wrap-cred /dev/null wrap-assert
	verify_assert "-v" wrap-pubkey wrap-assert
	get_assert no.tld "-t up=false -t uv=false -t pin=false" wrap-cred /dev/null wrap-assert
	! verify_assert "--" wrap-pubkey wrap-assert
	get_assert no.tld "-h -t uv=true" wrap-cred hmac-salt wrap-assert
	verify_assert "-hv" wrap-pubkey wrap-assert
	get_assert no.tld "-h -t uv=true -t pin=true" wrap-cred hmac-salt wrap-assert
	verify_assert "-hv" wrap-pubkey wrap-assert
	get_assert no.tld "-h -t uv=true -t pin=false" wrap-cred hmac-salt wrap-assert
	verify_assert "-hv" wrap-pubkey wrap-assert
	get_assert no.tld "-h -t uv=false" wrap-cred hmac-salt wrap-assert
	verify_assert "-h" wrap-pubkey wrap-assert
	get_assert no.tld "-h -t uv=false -t pin=true" wrap-cred hmac-salt wrap-assert
	verify_assert "-hv" wrap-pubkey wrap-assert
	get_assert no.tld "-h -t uv=false -t pin=false" wrap-cred hmac-salt wrap-assert
	verify_assert "-h" wrap-pubkey wrap-assert
	get_assert no.tld "-h -t up=true -t uv=true" wrap-cred hmac-salt wrap-assert
	verify_assert "-hpv" wrap-pubkey wrap-assert
	get_assert no.tld "-h -t up=true -t uv=true -t pin=true" wrap-cred hmac-salt wrap-assert
	verify_assert "-hpv" wrap-pubkey wrap-assert
	get_assert no.tld "-h -t up=true -t uv=true -t pin=false" wrap-cred hmac-salt wrap-assert
	verify_assert "-hpv" wrap-pubkey wrap-assert
	get_assert no.tld "-h -t up=true -t uv=false" wrap-cred hmac-salt wrap-assert
	verify_assert "-hp" wrap-pubkey wrap-assert
	get_assert no.tld "-h -t up=true -t uv=false -t pin=true" wrap-cred hmac-salt wrap-assert
	verify_assert "-hpv" wrap-pubkey wrap-assert
	get_assert no.tld "-h -t up=true -t uv=false -t pin=false" wrap-cred hmac-salt wrap-assert
	verify_assert "-hp" wrap-pubkey wrap-assert
	! get_assert no.tld "-h -t up=false -t uv=true" wrap-cred hmac-salt wrap-assert
	! get_assert no.tld "-h -t up=false -t uv=true -t pin=true" wrap-cred hmac-salt wrap-assert
	! get_assert no.tld "-h -t up=false -t uv=true -t pin=false" wrap-cred hmac-salt wrap-assert
	! get_assert no.tld "-h -t up=false -t uv=false" wrap-cred hmac-salt wrap-assert
	! get_assert no.tld "-h -t up=false -t uv=false -t pin=true" wrap-cred hmac-salt wrap-assert
	! get_assert no.tld "-h -t up=false -t uv=false -t pin=false" wrap-cred hmac-salt wrap-assert
fi

# resident
get_assert no.tld "-r" /dev/null /dev/null wrap-assert
get_assert no.tld "-r -t pin=true" /dev/null /dev/null wrap-assert
get_assert no.tld "-r -t pin=false" /dev/null /dev/null wrap-assert
get_assert no.tld "-r -t up=true" /dev/null /dev/null wrap-assert
get_assert no.tld "-r -t up=true -t pin=true" /dev/null /dev/null wrap-assert
get_assert no.tld "-r -t up=true -t pin=false" /dev/null /dev/null wrap-assert
get_assert no.tld "-r -t up=false" /dev/null /dev/null wrap-assert
get_assert no.tld "-r -t up=false -t pin=true" /dev/null /dev/null wrap-assert
get_assert no.tld "-r -t up=false -t pin=false" /dev/null /dev/null wrap-assert
get_assert no.tld "-r -h" /dev/null hmac-salt wrap-assert
get_assert no.tld "-r -h -t pin=true" /dev/null hmac-salt wrap-assert
get_assert no.tld "-r -h -t pin=false" /dev/null hmac-salt wrap-assert
get_assert no.tld "-r -h -t up=true" /dev/null hmac-salt wrap-assert
get_assert no.tld "-r -h -t up=true -t pin=true" /dev/null hmac-salt wrap-assert
get_assert no.tld "-r -h -t up=true -t pin=false" /dev/null hmac-salt wrap-assert
! get_assert no.tld "-r -h -t up=false" /dev/null hmac-salt wrap-assert
! get_assert no.tld "-r -h -t up=false -t pin=true" /dev/null hmac-salt wrap-assert
! get_assert no.tld "-r -h -t up=false -t pin=false" /dev/null hmac-salt wrap-assert

if [ "x${UV}" != "x" ]; then
	get_assert no.tld "-r -t uv=true" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t uv=true -t pin=true" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t uv=true -t pin=false" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t uv=false" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t uv=false -t pin=true" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t uv=false -t pin=false" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t up=true -t uv=true" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t up=true -t uv=true -t pin=true" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t up=true -t uv=true -t pin=false" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t up=true -t uv=false" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t up=true -t uv=false -t pin=true" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t up=true -t uv=false -t pin=false" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t up=false -t uv=true" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t up=false -t uv=true -t pin=true" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t up=false -t uv=true -t pin=false" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t up=false -t uv=false" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t up=false -t uv=false -t pin=true" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -t up=false -t uv=false -t pin=false" /dev/null /dev/null wrap-assert
	get_assert no.tld "-r -h -t uv=true" /dev/null hmac-salt wrap-assert
	get_assert no.tld "-r -h -t uv=true -t pin=true" /dev/null hmac-salt wrap-assert
	get_assert no.tld "-r -h -t uv=true -t pin=false" /dev/null hmac-salt wrap-assert
	get_assert no.tld "-r -h -t uv=false" /dev/null hmac-salt wrap-assert
	get_assert no.tld "-r -h -t uv=false -t pin=true" /dev/null hmac-salt wrap-assert
	get_assert no.tld "-r -h -t uv=false -t pin=false" /dev/null hmac-salt wrap-assert
	get_assert no.tld "-r -h -t up=true -t uv=true" /dev/null hmac-salt wrap-assert
	get_assert no.tld "-r -h -t up=true -t uv=true -t pin=true" /dev/null hmac-salt wrap-assert
	get_assert no.tld "-r -h -t up=true -t uv=true -t pin=false" /dev/null hmac-salt wrap-assert
	get_assert no.tld "-r -h -t up=true -t uv=false" /dev/null hmac-salt wrap-assert
	get_assert no.tld "-r -h -t up=true -t uv=false -t pin=true" /dev/null hmac-salt wrap-assert
	get_assert no.tld "-r -h -t up=true -t uv=false -t pin=false" /dev/null hmac-salt wrap-assert
	! get_assert no.tld "-r -h -t up=false -t uv=true" /dev/null hmac-salt wrap-assert
	! get_assert no.tld "-r -h -t up=false -t uv=true -t pin=true" /dev/null hmac-salt wrap-assert
	! get_assert no.tld "-r -h -t up=false -t uv=true -t pin=false" /dev/null hmac-salt wrap-assert
	! get_assert no.tld "-r -h -t up=false -t uv=false" /dev/null hmac-salt wrap-assert
	! get_assert no.tld "-r -h -t up=false -t uv=false -t pin=true" /dev/null hmac-salt wrap-assert
	! get_assert no.tld "-r -h -t up=false -t uv=false -t pin=false" /dev/null hmac-salt wrap-assert
fi

exit 0
