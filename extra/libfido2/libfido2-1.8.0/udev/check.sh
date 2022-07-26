#!/bin/sh -u

# Copyright (c) 2020 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

sort_by_id() {
	awk '{ printf "%d\n", $3 }' | sort -Cnu
}

if ! grep '^vendor' "$1" | sort_by_id; then
	echo unsorted vendor section 1>&2
	exit 1
fi

VENDORS=$(grep '^vendor' "$1" | awk '{ print $2 }')
PRODUCTS=$(grep '^product' "$1" | awk '{ print $2 }' | uniq)

if [ "${VENDORS}" != "${PRODUCTS}" ]; then
	echo vendors: "$(echo "${VENDORS}" | tr '\n' ',')" 1>&2
	echo products: "$(echo "${PRODUCTS}" | tr '\n' ',')" 1>&2
	echo vendors and products in different order 1>&2
	exit 2
fi

for v in ${VENDORS}; do
	if ! grep "^product ${v}" "$1" | sort_by_id; then
		echo "${v}": unsorted product section 1>&2
		exit 3
	fi
done
