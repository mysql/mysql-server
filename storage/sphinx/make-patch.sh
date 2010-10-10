#!/bin/sh

OUT=$1
ORIG=$2
NEW=$3

if [ ! \( "$1" -a "$2" -a "$3" \) ]; then
	echo "$0 <patch> <original> <new>"
	exit 1
fi

FILES='
/config/ac-macros/ha_sphinx.m4
/configure.in
/libmysqld/Makefile.am
/sql/handler.cc
/sql/handler.h
/sql/Makefile.am
/sql/mysqld.cc
/sql/mysql_priv.h
/sql/set_var.cc
/sql/sql_lex.h
/sql/sql_parse.cc
/sql/sql_yacc.yy
/sql/structs.h
/sql/sql_show.cc
'

rm -f $OUT
if [ -e $OUT ]; then
	exit 1
fi

for name in $FILES; do
	diff -BNru "$ORIG$name" "$NEW$name" >> $OUT
done
