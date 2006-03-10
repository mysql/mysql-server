#!/bin/bash
#
# regenerate parser from bison input files as documented at the top of
# pars0lex.l.

set -eu

bison -d pars0grm.y
mv pars0grm.tab.c pars0grm.c
mv pars0grm.tab.h pars0grm.h
cp pars0grm.h ../include
