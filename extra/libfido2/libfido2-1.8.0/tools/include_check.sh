#!/bin/sh

# Copyright (c) 2019 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

check() {
	for f in $(find $1 -maxdepth 1 -name '*.h'); do
		echo "#include \"$f\"" | \
			cc $CFLAGS -Isrc -xc -c - -o /dev/null 2>&1
		echo "$f $CFLAGS $?"
	done
}

check examples
check fuzz
check openbsd-compat
CFLAGS="${CFLAGS} -D_FIDO_INTERNAL" check src
check src/fido.h
check src/fido
check tools
