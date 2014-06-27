#! /bin/bash

# This test requires a non-lowercase tmpdir directory on a case-sensitive
# filesystem.

d="$MYSQLTEST_VARDIR/tmp/MixedCase"
test -d "$d" || mkdir "$d"
rm -f "$d"/*
