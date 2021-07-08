#!/bin/sh -u

# Copyright (c) 2018 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

for f in export.gnu export.llvm export.msvc; do
	if [ ! -f "${f}" ]; then
		exit 1
	fi
done

TMPDIR="$(mktemp -d)"
GNU="${TMPDIR}/gnu"
LLVM="${TMPDIR}/llvm"
MSVC="${TMPDIR}/msvc"

awk '/^[^*{}]+;$/' export.gnu | tr -d '\t;' | sort > "${GNU}"
sed 's/^_//' export.llvm | sort > "${LLVM}"
grep -v '^EXPORTS$' export.msvc | sort > "${MSVC}"
diff -u "${GNU}" "${LLVM}" && diff -u "${MSVC}" "${LLVM}"
ERROR=$?
rm "${GNU}" "${LLVM}" "${MSVC}"
rmdir "${TMPDIR}"

exit ${ERROR}
