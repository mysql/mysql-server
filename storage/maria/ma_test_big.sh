#!/bin/sh
#
# This tests is good to find bugs in the redo/undo handling and in
# finding bugs in blob handling
#

set -e
a=15
while test $a -le 5000
do
  echo $a
  rm -f maria_log*
  ma_test2 -s -L -K -W -P -M -T -c -b32768 -t4 -A1 -m$a > /dev/null
  maria_read_log -a -s >& /dev/null
  maria_chk -es test2
  maria_read_log -a -s >& /dev/null
  maria_chk -es test2
  rm test2.MA?
  maria_read_log -a -s >& /dev/null
  maria_chk -es test2
  a=$((a+1))
done
