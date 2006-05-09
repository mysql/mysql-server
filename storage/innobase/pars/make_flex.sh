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
# a warning on Win64. this adds the cast.
sed -e 's/int offset = (yy_c_buf_p) - (yytext_ptr);/int offset = (int)((yy_c_buf_p) - (yytext_ptr));/;' < $TMPFILE >> $OUTFILE

rm $TMPFILE
