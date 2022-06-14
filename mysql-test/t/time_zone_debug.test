--source include/have_debug.inc
# CET is also know as MET, so using this include file for checking CET.
--source include/have_met_timezone.inc

--echo #
--echo # Start of 5.7 tests
--echo #

--echo #
--echo # BUG#19792203 - FAILING ASSERTION: TRX->IS_DD_TRX == FALSE
--echo #                WHEN KILLING QUERY WITH CONVERT_TZ
--echo #
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1);
COMMIT;
SET SESSION debug="+d,kill_query_on_open_table_from_tz_find";
--error ER_QUERY_INTERRUPTED
SELECT CONVERT_TZ(a, a, a) FROM t1;
SET SESSION debug="-d,kill_query_on_open_table_from_tz_find";
DROP TABLE t1;

--echo #
--echo # BUG#20507804 "FAILING ASSERTION: TRX->READ_ONLY && TRX->AUTO_COMMIT
--echo #               && TRX->ISOLATION_LEVEL==1".
--echo #

--echo # New connection which is closed after the test is needed to reproduce
--echo # the original problem.
connect (con1,localhost,root,,);

SET DEBUG='+d,mysql_lock_tables_kill_query';
--error ER_QUERY_INTERRUPTED
SELECT CONVERT_TZ('2003-10-26 01:00:00', 'There-is-no-such-time-zone-1', 'UTC');
SET DEBUG='-d,mysql_lock_tables_kill_query';
START TRANSACTION WITH CONSISTENT SNAPSHOT;
COMMIT;

disconnect con1;
--source include/wait_until_disconnected.inc
connection default;

--echo #
--echo # Bug#29330089: SERVER TIMEZONE STAYS ON CEST AFTER CHANGING
--echo #               SUMMER TIME IS OVER
--echo #

SET DEBUG="+d,set_cet_before_dst";
SELECT @@SYSTEM_TIME_ZONE;
SET DEBUG="-d,set_cet_before_dst";
SET DEBUG="+d,set_cet_after_dst";
SELECT @@SYSTEM_TIME_ZONE;
SET DEBUG="-d,set_cet_after_dst";

--echo #
--echo # End of 5.7 tests
--echo #

