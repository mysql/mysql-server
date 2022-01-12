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

SQL="UPDATE time_statistics SET e_time = current_timestamp WHERE task_name = '$NAME';"
/usr/bin/psql -d $SID -c "$SQL"
