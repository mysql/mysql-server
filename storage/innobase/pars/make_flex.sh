#!/bin/bash
#
# Copyright (c) 1994, 2022, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
#
# generate lexer files from flex input files.

set -eu

TMPFILE=_flex_tmp.cc
OUTFILE=lexyy.cc

flex -o $TMPFILE pars0lex.l

# AIX needs its includes done in a certain order, so include "univ.i" first
# to be sure we get it right.
echo '#include "univ.i"' > $OUTFILE
echo 'static char *yytext;' >> $OUTFILE

# flex assigns a pointer to an int in one place without a cast, resulting in
# a warning on Win64.  Add the cast.  Also define some symbols as static.
sed -e '
s/'"$TMPFILE"'/'"$OUTFILE"'/;
s/\<register\> *//;
s/\(int offset = \)\((yy_c_buf_p) - (yytext_ptr)\);/\1(int)(\2);/;
s/\(void yy\(restart\|_\(delete\|flush\)_buffer\)\)/static \1/;
s/\(void yy_switch_to_buffer\)/[[maybe_unused]] static \1/;
s/\(void yy\(push\|pop\)_buffer_state\)/[[maybe_unused]] static \1/;
s/\(YY_BUFFER_STATE yy_create_buffer\)/static \1/;
s/^void yyset_extra.*;//;
s/\(\(int\|void\) yy[gs]et_\)/[[maybe_unused]] static \1/;
s/\(void \*\?yy\(\(re\)\?alloc\|free\)\)/static \1/;
s/\(extern \)\?\(int yy\(leng\|lineno\|_flex_debug\)\)/static \2/;
s/\(int yylex_destroy\)/[[maybe_unused]] static \1/;
s/^\(\(FILE\|char\) *\* *yyget\)/[[maybe_unused]] static \1/;
s/^\(extern \)\?\(\(FILE\|char\) *\* *yy\)/static \2/;
s/^static FILE \*yyin = .*0, \*yyout = .*0;//;
s/^static int yy\(_flex_debug\|lineno\);//;
s/^static char \*yytext;//;
' < $TMPFILE >> $OUTFILE

rm $TMPFILE
