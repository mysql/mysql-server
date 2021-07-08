#!/bin/bash -e
#
# Copyright (c) 2018 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

if [[ "$#" -ne 1 ]]; then
	echo "usage: test.sh device" 1>&2
	exit 1
fi

read -p "This script will reset the authenticator at $1, permanently erasing "\
"its credentials. Are you *SURE* you want to proceed (yes/no)? "
if [[ "${REPLY}" != "yes" ]]; then
	exit 1
fi

echo "Resetting authenticator... (tap to continue!)"
fido2-token -R $1

CRED_PARAM="$(mktemp /tmp/cred_param.XXXXXXXX)"
ASSERT_PARAM="$(mktemp /tmp/assert_param.XXXXXXXX)"
ASSERT_PUBKEY="$(mktemp /tmp/assert_pubkey.XXXXXXXX)"
ES256_CRED="$(mktemp /tmp/es256_cred.XXXXXXX)"
ES256_CRED_R="$(mktemp /tmp/es256_cred_r.XXXXXXXX)"

cleanup() {
	echo "Cleaning up..."
	[[ "${CRED_PARAM}" != "" ]] && rm "${CRED_PARAM}"
	[[ "${ASSERT_PARAM}" != "" ]] && rm "${ASSERT_PARAM}"
	[[ "${ASSERT_PUBKEY}" != "" ]] && rm "${ASSERT_PUBKEY}"
	[[ "${ES256_CRED}" != "" ]] && rm "${ES256_CRED}"
	[[ "${ES256_CRED_R}" != "" ]] && rm "${ES256_CRED_R}"
}

trap cleanup EXIT

dd if=/dev/urandom bs=1 count=32 2>/dev/null | base64 > "${CRED_PARAM}"
echo "Boring Relying Party" >> "${CRED_PARAM}"
echo "Boring User Name" >> "${CRED_PARAM}"
dd if=/dev/urandom bs=1 count=32 2>/dev/null | base64 >> "${CRED_PARAM}"
echo "Credential parameters:"
cat "${CRED_PARAM}"

echo "Generating non-resident ES256 credential... (tap to continue!)"
fido2-cred -M -i "${CRED_PARAM}" $1 | fido2-cred -V | tee "${ES256_CRED}"
echo "Generating resident ES256 credential... (tap to continue!)"
fido2-cred -M -r -i "${CRED_PARAM}" $1 | fido2-cred -V | tee "${ES256_CRED_R}"

PIN1="$(dd if=/dev/urandom | tr -cd '[:print:]' | fold -w50 | head -1)"
PIN2="$(dd if=/dev/urandom | tr -cd '[:print:]' | fold -w50 | head -1)"

echo "Setting ${PIN1} as the PIN..."
echo -e "${PIN1}\n${PIN1}" | setsid -w fido2-token -S $1
echo "Changing PIN from ${PIN1} to ${PIN2}..."
echo -e "${PIN1}\n${PIN2}\n${PIN2}" | setsid -w fido2-token -C $1
echo ""

echo "Testing non-resident ES256 credential..."
echo "Getting assertion without user presence verification..."
dd if=/dev/urandom bs=1 count=32 2>/dev/null | base64 > "${ASSERT_PARAM}"
echo "Boring Relying Party" >> "${ASSERT_PARAM}"
head -1 "${ES256_CRED}" >> "${ASSERT_PARAM}"
tail -n +2 "${ES256_CRED}" > "${ASSERT_PUBKEY}"
echo "Assertion parameters:"
cat "${ASSERT_PARAM}"
fido2-assert -G -i "${ASSERT_PARAM}" $1 | fido2-assert -V "${ASSERT_PUBKEY}" 
echo "Checking that the user presence bit is observed..."
! fido2-assert -G -i "${ASSERT_PARAM}" $1 | fido2-assert -V -p "${ASSERT_PUBKEY}"
echo "Checking that the user verification bit is observed..."
! fido2-assert -G -i "${ASSERT_PARAM}" $1 | fido2-assert -V -v "${ASSERT_PUBKEY}"
echo "Getting assertion _with_ user presence verification... (tap to continue!)"
fido2-assert -G -p -i "${ASSERT_PARAM}" $1 | fido2-assert -V -p "${ASSERT_PUBKEY}" 
echo "Getting assertion  _with_ user verification..."
echo -e "${PIN2}\n" | setsid -w fido2-assert -G -v -i "${ASSERT_PARAM}" $1 | \
	fido2-assert -V -v "${ASSERT_PUBKEY}" 
echo ""

echo "Testing resident ES256 credential..."
echo "Getting assertion without user presence verification..."
dd if=/dev/urandom bs=1 count=32 2>/dev/null | base64 > "${ASSERT_PARAM}"
echo "Boring Relying Party" >> "${ASSERT_PARAM}"
tail -n +2 "${ES256_CRED_R}" > "${ASSERT_PUBKEY}"
echo "Assertion parameters:"
cat "${ASSERT_PARAM}"
fido2-assert -G -r -i "${ASSERT_PARAM}" $1 | fido2-assert -V "${ASSERT_PUBKEY}" 
echo "Checking that the user presence bit is observed..."
! fido2-assert -G -r -i "${ASSERT_PARAM}" $1 | fido2-assert -V -p "${ASSERT_PUBKEY}"
echo "Checking that the user verification bit is observed..."
! fido2-assert -G -r -i "${ASSERT_PARAM}" $1 | fido2-assert -V -v "${ASSERT_PUBKEY}"
echo "Getting assertion _with_ user presence verification... (tap to continue!)"
fido2-assert -G -r -p -i "${ASSERT_PARAM}" $1 | fido2-assert -V -p "${ASSERT_PUBKEY}" 
echo "Getting assertion _with_ user verification..."
echo -e "${PIN2}\n" | setsid -w fido2-assert -G -v -r -i "${ASSERT_PARAM}" $1 | \
	fido2-assert -V -v "${ASSERT_PUBKEY}" 
echo ""
