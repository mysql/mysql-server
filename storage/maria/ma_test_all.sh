#!/bin/sh
#
# This file is now deprecated and has been replaced by
# unittest/ma_test_all-t
#
#
#
#

PRG='unittest/ma_test_all-t'
UTST='../../unittest/unit.pl'

if [ ! -x $PRG ] ; then
  DIR=`dirname $0`
  PRG="$DIR/unittest/ma_test_all-t"
  UTST="$DIR/../../unittest/unit.pl"
fi

if test -n "$1"; then

  # unit.pl can't pass options to ma_test_all-t, so if anything
  # was passed as an argument, assume the purpose was to pass
  # them to ma_test_all-t and call it directly

  $PRG $@
else
  perl $UTST run $PRG
fi
