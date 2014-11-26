#! /bin/bash

d="$MYSQLTEST_VARDIR/tmp/long_temporary_directory_path_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789"
test -d "$d" || mkdir "$d"
rm -f "$d"/*
