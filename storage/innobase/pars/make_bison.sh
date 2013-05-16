#!/bin/bash
#
# Copyright (c) 1994, 2013, Oracle and/or its affiliates. All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA
#
# generate parser files from bison input files.

set -eu
TMPFILE=pars0grm.tab.c
OUTFILE=pars0grm.cc

bison -d pars0grm.y
mv pars0grm.tab.h ../include/pars0grm.h

sed -e '
s/'"$TMPFILE"'/'"$OUTFILE"'/;
s/^\(\(YYSTYPE\|int\) yy\(char\|nerrs\)\)/static \1/;
' < "$TMPFILE" > "$OUTFILE"

rm "$TMPFILE"
