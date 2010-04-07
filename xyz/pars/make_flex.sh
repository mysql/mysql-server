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
# generate lexer files from flex input files.

set -eu

TMPFILE=_flex_tmp.c
OUTFILE=lexyy.c

flex -o $TMPFILE pars0lex.l

# AIX needs its includes done in a certain order, so include "univ.i" first
# to be sure we get it right.
echo '#include "univ.i"' > $OUTFILE

# flex assigns a pointer to an int in one place without a cast, resulting in
# a warning on Win64.  Add the cast.  Also define some symbols as static.
sed -e '
s/'"$TMPFILE"'/'"$OUTFILE"'/;
s/\(int offset = \)\((yy_c_buf_p) - (yytext_ptr)\);/\1(int)(\2);/;
s/\(void yy\(restart\|_\(delete\|flush\)_buffer\)\)/static \1/;
s/\(void yy_switch_to_buffer\)/__attribute__((unused)) static \1/;
s/\(void yy\(push\|pop\)_buffer_state\)/__attribute__((unused)) static \1/;
s/\(YY_BUFFER_STATE yy_create_buffer\)/static \1/;
s/\(\(int\|void\) yy[gs]et_\)/__attribute__((unused)) static \1/;
s/\(void \*\?yy\(\(re\)\?alloc\|free\)\)/static \1/;
s/\(extern \)\?\(int yy\(leng\|lineno\|_flex_debug\)\)/static \2/;
s/\(int yylex_destroy\)/__attribute__((unused)) static \1/;
s/\(extern \)\?\(int yylex \)/UNIV_INTERN \2/;
s/^\(\(FILE\|char\) *\* *yyget\)/__attribute__((unused)) static \1/;
s/^\(extern \)\?\(\(FILE\|char\) *\* *yy\)/static \2/;
' < $TMPFILE >> $OUTFILE

rm $TMPFILE
