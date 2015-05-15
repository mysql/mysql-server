#
# Start of 5.7 tests
#
#
# BUG#19792203 - FAILING ASSERTION: TRX->IS_DD_TRX == FALSE
#                WHEN KILLING QUERY WITH CONVERT_TZ
#
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1);
COMMIT;
SET SESSION debug="+d,kill_query_on_open_table_from_tz_find";
SELECT CONVERT_TZ(a, a, a) FROM t1;
ERROR 70100: Query execution was interrupted
SET SESSION debug="-d,kill_query_on_open_table_from_tz_find";
DROP TABLE t1;
#
# BUG#20507804 "FAILING ASSERTION: TRX->READ_ONLY && TRX->AUTO_COMMIT
#               && TRX->ISOLATION_LEVEL==1".
#
# New connection which is closed after the test is needed to reproduce
# the original problem.
SET DEBUG='+d,mysql_lock_tables_kill_query';
SELECT CONVERT_TZ('2003-10-26 01:00:00', 'There-is-no-such-time-zone-1', 'UTC');
ERROR 70100: Query execution was interrupted
SET DEBUG='-d,mysql_lock_tables_kill_query';
START TRANSACTION WITH CONSISTENT SNAPSHOT;
COMMIT;
#
# End of 5.7 tests
#
