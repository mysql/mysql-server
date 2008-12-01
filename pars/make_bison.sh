#!/bin/bash
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
