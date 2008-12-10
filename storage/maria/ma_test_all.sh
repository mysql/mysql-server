#!/bin/sh
#
# This file is now deprecated and has been replaced by
# unittest/ma_test_all-t
#
#
#
#

if test -n "$1"; then

  # unit.pl can't pass options to ma_test_all-t, so if anything
  # was passed as an argument, assume the purpose was to pass
  # them to ma_test_all-t and call it directly

  unittest/ma_test_all-t $@
else
  perl ../../unittest/unit.pl run unittest/ma_test_all-t
fi
