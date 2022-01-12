#!/bin/bash

if [ $# -ne 2 ]; then
	echo "usage: db_stats.sh <database_name> <output_dir>"
	exit 1
fi

OUTPUT_DIR=$2
PHASE=$3

SAMPLE_LENGTH=60

DBOUTPUT_DIR=$OUTPUT_DIR/db
mkdir -p $DBOUTPUT_DIR

# put db info into the readme.txt file
/usr/bin/psql --version >> $OUTPUT_DIR/readme.txt
echo "Database statistics taken at $SAMPLE_LENGTH second intervals, $ITERATIONS times." >> $OUTPUT_DIR/readme.txt

# save the database parameters
/usr/bin/psql -d $SID -c "SHOW ALL"  > $OUTPUT_DIR/param.out

# record indexes
/usr/bin/psql -d $SID -c "SELECT * FROM pg_stat_user_indexes;" -o $DBOUTPUT_DIR/indexes.out


while [ 1 ]; do
	# collent ipcs stats
        ipcs >> $OUTPUT_DIR/ipcs.out

	# Column stats for Tom Lane.
	/usr/bin/psql -d $SID -c "SELECT * FROM pg_stats WHERE attname = 'p_partkey';" >> $DBOUTPUT_DIR/p_partkey.out
	/usr/bin/psql -d $SID -c "SELECT * FROM pg_stats WHERE attname = 'l_partkey';" >> $DBOUTPUT_DIR/l_partkey.out
	/usr/bin/psql -d $SID -c "SELECT * FROM pg_stats WHERE attname = 'ps_suppkey';" >> $DBOUTPUT_DIR/ps_suppkey.out
	/usr/bin/psql -d $SID -c "SELECT * FROM pg_stats WHERE attname = 'l_suppkey';" >> $DBOUTPUT_DIR/l_suppkey.out

	# check lock statistics
	/usr/bin/psql -d $SID -c "SELECT relname,pid, mode, granted FROM pg_locks, pg_class WHERE relfilenode = relation;" >> $DBOUTPUT_DIR/lockstats.out
	/usr/bin/psql -d $SID -c "SELECT * FROM pg_locks WHERE transaction IS NOT NULL;" >> $DBOUTPUT_DIR/tran_lock.out

	# read the database activity table
	/usr/bin/psql -d $SID -c "SELECT * FROM pg_stat_activity;" >> $DBOUTPUT_DIR/db_activity.out
	# table info
	/usr/bin/psql -d $SID -c "SELECT relid, relname, heap_blks_read, heap_blks_hit, idx_blks_read, idx_blks_hit FROM pg_statio_user_tables WHERE relname <> 'time_statistics' ORDER BY relname;" >> $DBOUTPUT_DIR/table_info.out
	/usr/bin/psql -d $SID -c "SELECT relid, indexrelid, relname, indexrelname, idx_blks_read, idx_blks_hit FROM pg_statio_user_indexes ORDER BY indexrelname;" >> $DBOUTPUT_DIR/index_info.out
	# scans 
	/usr/bin/psql -d $SID -c "SELECT * FROM pg_stat_user_tables WHERE relname <> 'time_statistics' ORDER BY relname;" >> $DBOUTPUT_DIR/table_scan.out
	/usr/bin/psql -d $SID -c "SELECT * FROM pg_stat_user_indexes ORDER BY indexrelname;" >> $DBOUTPUT_DIR/indexes_scan.out
	sleep $SAMPLE_LENGTH
done
