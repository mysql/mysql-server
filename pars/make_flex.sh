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
