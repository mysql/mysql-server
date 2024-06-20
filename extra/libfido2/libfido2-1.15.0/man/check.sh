#!/bin/sh -u

# Copyright (c) 2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

T=$(mktemp -d) || exit 1
find . -maxdepth 1 -type f -name '*.3' -print0 > "$T/files"

xargs -0 awk '/^.Sh NAME/,/^.Nd/' < "$T/files" | \
    awk '/^.Nm/ { print $2 }' | sort -u > "$T/Nm"
xargs -0 awk '/^.Fn/ { print $2 }' < "$T/files" | sort -u > "$T/Fn"
(cd "$T" && diff -u Nm Fn)

cut -c2- ../src/export.llvm | sort > "$T/exports"
(cd "$T" && diff -u Nm exports)

awk '/^list\(APPEND MAN_SOURCES/,/^\)/' CMakeLists.txt | \
    awk '/.3$/ { print $1 }' | sort > "$T/listed_sources"
xargs -0 -n1 basename < "$T/files" | sort > "$T/actual_sources"
(cd "$T" && diff -u listed_sources actual_sources)

awk '/^list\(APPEND MAN_ALIAS/,/^\)/' CMakeLists.txt | \
    sed '1d;$d' | awk '{ print $1, $2 }' | sort > "$T/listed_aliases"
xargs -0 grep -o "^.Fn [A-Za-z0-9_]* \"" < "$T/files" | \
    cut -c3- | sed 's/\.3:\.Fn//;s/ "//' | awk '$1 != $2' | \
    sort > "$T/actual_aliases"
(cd "$T" && diff -u listed_aliases actual_aliases)

xargs -0 grep -hB1 "^.Fn [A-Za-z0-9_]* \"" < "$T/files" | \
    sed -E 's/^.F[tn] //;s/\*[^"\*]+"/\*"/g;s/ [^" \*]+"/"/g;/^--$/d' | \
    paste -d " " - - | sed 's/\* /\*/' | sort > "$T/documented_prototypes"
while read -r f; do
	awk "/\/\*/ { next } /$f\(/,/;/" ../src/fido.h ../src/fido/*.h | \
	    sed -E 's/^[ ]+//;s/[ ]+/ /' | tr '\n' ' ' | \
	    sed 's/(/ "/;s/, /" "/g;s/);/"/;s/ $/\n/'
done < "$T/exports" | sort > "$T/actual_prototypes"
(cd "$T" && diff -u documented_prototypes actual_prototypes)

(cd "$T" && rm files Nm Fn exports listed_sources actual_sources \
    listed_aliases actual_aliases documented_prototypes actual_prototypes)
rmdir -- "$T"
