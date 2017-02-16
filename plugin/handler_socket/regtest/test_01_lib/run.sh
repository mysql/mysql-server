#!/bin/bash

TESTS="01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23";

source ../common/compat.sh

for i in $TESTS; do
  perl "test$i.pl" > test$i.log 2> test$i.log2
done
for i in $TESTS; do
  if ! $DIFF -u test$i.log test$i.expected; then
    echo "test$i failed";
    exit 1
  fi
  if [ -f "test$i.expect2" ]; then
    lines="`wc -l < test$i.expect2`"
    head -$lines test$i.log2 > test$i.log2h
    if ! $DIFF -u test$i.log2h test$i.expect2 && \
       ! $DIFF -u test$i.log2h test$i.expect2ef; then
      echo "test$i failed";
      exit 1
    fi
  fi
done
echo "OK."
exit 0

