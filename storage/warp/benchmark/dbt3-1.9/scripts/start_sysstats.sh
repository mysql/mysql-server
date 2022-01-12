#!/bin/bash
#
# This file is released under the terms of the Artistic License.  Please see
# the file LICENSE, included in this package, for details.
#
# Copyright (C) 2002 Mark Wong & Open Source Development Lab, Inc.
#
# 17 october 2002

# Set the relative execution prefix

SAMPLE_LENGTH=60
while getopts "o:s:" opt; do
	case $opt in
		o)
			OUTPUT_DIR=$OPTARG
			;;
		s)
			SAMPLE_LENGTH=$OPTARG
			;;
	esac
done

/usr/bin/sar -o $OUTPUT_DIR/sar.out $SAMPLE_LENGTH 0 &

/usr/bin/iostat -d $SAMPLE_LENGTH  >> $OUTPUT_DIR/iostat.out &
/usr/bin/iostat -d -x $SAMPLE_LENGTH  >> $OUTPUT_DIR/iostatx.out &

/usr/bin/vmstat -n $SAMPLE_LENGTH >> $OUTPUT_DIR/vmstat.out &
