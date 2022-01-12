#!/bin/bash
#
# This file is released under the terms of the Artistic License.  Please see
# the file LICENSE, included in this package, for details.
#
# Copyright (C) 2002 Mark Wong & Open Source Development Lab, Inc.
#
# 17 october 2002

OUTPUT_DIR=$1

killall /usr/bin/sar /usr/bin/iostat /usr/bin/vmstat /usr/lib/sa/sadc > /dev/null 2> /dev/null
ps -ef | grep -v grep | grep db_stats | awk '{ print $2}' | xargs kill > /dev/null 2> /dev/null

# We are done collecting stats, so transform the sar output to something
# readable.
/usr/bin/sar -f $OUTPUT_DIR/sar.out -A > $OUTPUT_DIR/sar.txt
