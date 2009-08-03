#!/bin/bash
#
# Copyright (c) 2006, 2009, Innobase Oy. All Rights Reserved.
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
# this program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place, Suite 330, Boston, MA 02111-1307 USA
#
# generate parser files from bison input files.

set -eu
TMPFILE=pars0grm.tab.c
OUTFILE=pars0grm.c

bison -d pars0grm.y
mv pars0grm.tab.h ../include/pars0grm.h

sed -e '
s/'"$TMPFILE"'/'"$OUTFILE"'/;
s/^\(\(YYSTYPE\|int\) yy\(char\|nerrs\)\)/static \1/;
s/\(\(YYSTYPE\|int\) yy\(lval\|parse\)\)/UNIV_INTERN \1/;
' < "$TMPFILE" > "$OUTFILE"

rm "$TMPFILE"
