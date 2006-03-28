#!/bin/bash
#
# generate parser files from bison input files.

set -eu

bison -d pars0grm.y
mv pars0grm.tab.c pars0grm.c
mv pars0grm.tab.h pars0grm.h
cp pars0grm.h ../include
