#!/bin/sh

#
# This file is released under the terms of the Artistic License.
# Please see the file LICENSE, included in this package, for details.
#
# Copyright (C) 2004 Mark Wong & Open Source Development Lab, Inc.
#

SRCDIR=/home/justin/warp/storage/warp/benchmark/dbt3-1.9

while getopts "n:" opt; do
	case $opt in
	n)
		NAME=$OPTARG
		;;
	esac
done

s_time=`$GTIME`
SQL="INSERT INTO time_statistics (task_name, s_time, int_time) VALUES ('$NAME', current_timestamp, $s_time);"
/usr/bin/psql -d $SID -c "$SQL"
