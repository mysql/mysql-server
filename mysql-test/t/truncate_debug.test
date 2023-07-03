#
# Test of truncate
#
--source include/have_debug.inc

--disable_warnings
drop table if exists t1, t2;
--enable_warnings

--echo #
--echo # Bug#29368199: SIG 11 IN LOCKED_TABLES_LIST::UNLOCK_LOCKED_TABLES
--echo # Need debug binary

--let $debug_point= simulate_error_in_truncate_ddl
--source include/add_debug_point.inc

CREATE TABLE t1(a INT, b TEXT, KEY (a)) SECONDARY_ENGINE=MOCK;
LOCK TABLES t1 WRITE;

--error ER_SECONDARY_ENGINE
TRUNCATE TABLE t1;

--let $debug_point= simulate_error_in_truncate_ddl
--source include/remove_debug_point.inc

SELECT * FROM t1;
UNLOCK TABLES;
DROP TABLE t1;
