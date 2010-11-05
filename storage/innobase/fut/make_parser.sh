#!/bin/sh

TMPF=t.$$

make -f Makefile.query

# This is to avoid compiler warning about unused parameters
sed -e '
s/^\(static.*void.*yy_fatal_error.*msg.*,\)\(.*yyscanner.*\)/\1 __attribute__((unused)) \2/;
s/^\(static.*void.*yy_flex_strncpy.*n.*,\)\(.*yyscanner.*\)/\1 __attribute__((unused)) \2/;
s/^\(static.*int.*yy_flex_strlen.*s.*,\)\(.*yyscanner.*\)/\1 __attribute__((unused)) \2/;
s/^\(\(static\|void\).*fts0[bt]alloc.*,\)\(.*yyscanner.*\)/\1 __attribute__((unused)) \3/;
s/^\(\(static\|void\).*fts0[bt]realloc.*,\)\(.*yyscanner.*\)/\1 __attribute__((unused)) \3/;
s/^\(\(static\|void\).*fts0[bt]free.*,\)\(.*yyscanner.*\)/\1 __attribute__((unused)) \3/;
' < fts0blex.c > $TMPF

mv $TMPF fts0blex.c

sed -e '
s/^\(static.*void.*yy_fatal_error.*msg.*,\)\(.*yyscanner.*\)/\1 __attribute__((unused)) \2/;
s/^\(static.*void.*yy_flex_strncpy.*n.*,\)\(.*yyscanner.*\)/\1 __attribute__((unused)) \2/;
s/^\(static.*int.*yy_flex_strlen.*s.*,\)\(.*yyscanner.*\)/\1 __attribute__((unused)) \2/;
s/^\(\(static\|void\).*fts0[bt]alloc.*,\)\(.*yyscanner.*\)/\1 __attribute__((unused)) \3/;
s/^\(\(static\|void\).*fts0[bt]realloc.*,\)\(.*yyscanner.*\)/\1 __attribute__((unused)) \3/;
s/^\(\(static\|void\).*fts0[bt]free.*,\)\(.*yyscanner.*\)/\1 __attribute__((unused)) \3/;
' < fts0tlex.c > $TMPF

mv $TMPF fts0tlex.c
