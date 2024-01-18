# This file is created to add test cases for mysqlbinlog tool
#  which can be executed only against debug compiled mysqlbinlog
#  tool.
--source include/have_log_bin.inc
--source include/have_mysqlbinlog_debug.inc
--source include/rpl/force_binlog_format_statement.inc

--echo #
--echo # Bug#18742916 : MYSQLBINLOG --RAW DOES NOT CHECK FOR ERRORS
--echo #

--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--error 1
--exec $MYSQL_BINLOG -#d,simulate_result_file_write_error_for_FD_event --raw --read-from-remote-server --user=root --host=127.0.0.1 --port=$MASTER_MYPORT -r$MYSQLTEST_VARDIR/tmp/ binlog.000001 2>&1

--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--error 1
--exec $MYSQL_BINLOG -#d,simulate_result_file_write_error --raw --read-from-remote-server --user=root --host=127.0.0.1 --port=$MASTER_MYPORT -r$MYSQLTEST_VARDIR/tmp/ binlog.000001 2>&1


# mysqlbinlog when executed with options --raw and --read-from-remote-server would leak memory,
# so the requirement for this test is that there shouldn't be any ASAN failures.
--echo #
--echo # Bug#24323288 : MAIN.MYSQLBINLOG_DEBUG FAILS WITH A LEAKSANITIZER ERROR
--echo #

--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--error 1
--exec $MYSQL_BINLOG -#d,simulate_create_log_file_error_for_FD_event --raw --read-from-remote-server --user=root --host=127.0.0.1 --port=$MASTER_MYPORT -r$MYSQLTEST_VARDIR/tmp/ binlog.000001 2>&1

--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR

# For events like User_var_log_event, Rand_log_event and Intvar_log_event, the event object will be deleted when
# the Query_log_event associated with them will be deleted, so this test is added just to ensure that with the new implemenation
# also this behavior is maintained.
CREATE TABLE T1(s INT);
SET @a= 10;
INSERT INTO T1 VALUES(@a);

--exec $MYSQL_BINLOG  --read-from-remote-server --user=root --host=127.0.0.1 --port=$MASTER_MYPORT  binlog.000001 > /dev/null 2>&1
# Cleanup
--remove_file $MYSQLTEST_VARDIR/tmp/binlog.000001

DROP TABLE T1;
--echo
--echo End of tests

--source include/rpl/restore_default_binlog_format.inc
