# ==== Purpose ====
#
# Test the --[start|stop]-[position|datetime] options to mysqlbinlog,
# with/without --read-from-remote-server, with one or two binary logs
# as input.
#
# ==== Implementation ====

# TODO: Need to look at making row based version once new binlog client is complete.

CREATE TABLE t1 (a INT);
RESET MASTER;

--source extra/binlog_tests/binlog_mysqlbinlog_fill.inc

--let $MYSQLD_DATADIR= `select @@datadir`

--echo
--echo ==== Local ====

--let $start_position= $pos_1
--let $start_datetime= $datetime_1

--let $stop_position= $pos_2
--let $stop_datetime= $datetime_2
--let $extra_options= $MYSQLD_DATADIR/$file_1
--source extra/binlog_tests/mysqlbinlog_start_stop_1.inc

--echo
--echo ==== Local with 2 binlogs on command line ====
# This is to verify that some options apply only to first, or last binlog

--let $stop_position= $pos_3
--let $stop_datetime= $datetime_3
--let $extra_options= $MYSQLD_DATADIR/$file_1 $MYSQLD_DATADIR/$file_2
--source extra/binlog_tests/mysqlbinlog_start_stop_1.inc

--echo
--echo ==== Remote ====

--let $stop_position= $pos_2
--let $stop_datetime= $datetime_2
--let $extra_options= --read-from-remote-server --user=root --host=127.0.0.1 --port=$MASTER_MYPORT $file_1
--source extra/binlog_tests/mysqlbinlog_start_stop_1.inc

--echo
--echo ==== Remote with 2 binlogs on command line ====

--let $stop_position= $pos_3
--let $stop_datetime= $datetime_3
--let $extra_options= --read-from-remote-server --user=root --host=127.0.0.1 --port=$MASTER_MYPORT $file_1 $file_2
--source extra/binlog_tests/mysqlbinlog_start_stop_1.inc


# ==== to-last-log ====

--let $extra_options= --read-from-remote-server --user=root --host=127.0.0.1 --port=$MASTER_MYPORT $file_1
--let $options= --to-last-log
--source extra/binlog_tests/mysqlbinlog_start_stop_2.inc


--echo
--echo ==== clean up ====
DROP TABLE t1;
