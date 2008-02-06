#!/bin/bash
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
# a warning on Win64.  Add the cast.  Also define some functions as static.
sed -e '
s/'"$TMPFILE"'/'"$OUTFILE"'/;
s/\(int offset = \)\((yy_c_buf_p) - (yytext_ptr)\);/\1(int)(\2);/;
s/\(void \(yyrestart\|yy_\(switch_to\|delete\|flush\)_buffer\)\)/static \1/;
s/\(void yypush_buffer_state\)/static \1/;
s/\(void yypop_buffer_state\)/static \1/;
s/\(YY_BUFFER_STATE yy_\(create_buffer\|scan_\(buffer\|string\|bytes\)\)\)/static \1/;
s/\(\(int\|void\) yy[gs]et_\)/static \1/;
s/\(void \*\?yy\(\(re\)\?alloc\|free\)\)/static \1/;
s/\(extern \)\?\(int yy\(leng\|lex_destroy\|lineno\|_flex_debug\)\)/static \2/;
s/\(extern \)\?\(int yylex \)/UNIV_INTERN \2/;
s/^\(extern \)\?\(\(FILE\|char\) *\* *yy\)/static \2/;
' < $TMPFILE >> $OUTFILE

rm $TMPFILE
