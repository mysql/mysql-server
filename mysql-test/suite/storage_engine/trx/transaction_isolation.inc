#
# Basic check for transaction isolation. 
# The results should be different depending on the isolation level.
# For some isolation levels, some statements will end with a timeout.
# If the engine has its own timeout parameters, reduce them to minimum,
# otherwise the test will take very long.
# If the timeout value is greater than the testcase-timeout the test is run with,
# it might fail due to the testcase timeout.
#

--source ../have_engine.inc

--enable_connect_log

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

connect (con1,localhost,root,,);
eval SET SESSION TRANSACTION ISOLATION LEVEL $trx_isolation;
connect (con2,localhost,root,,);
eval SET SESSION TRANSACTION ISOLATION LEVEL $trx_isolation;

connection con1;

let $create_definition = a $int_col;
--source ../create_table.inc

START TRANSACTION; 
--sorted_result
SELECT a FROM t1; # First snapshot

connection con2;

BEGIN;
--let $error_codes = 0,ER_LOCK_WAIT_TIMEOUT
INSERT INTO t1 (a) VALUES(1); 
--source ../strict_check_errors.inc

connection con1;
--sorted_result
SELECT a FROM t1; # Second snapshot

connection con2;
--let $error_codes = 0,ER_LOCK_WAIT_TIMEOUT
INSERT INTO t1 (a) VALUES (2); 
--source ../strict_check_errors.inc

connection con1;
--sorted_result
SELECT a FROM t1; # Third snapshot

--let $error_codes = 0,ER_LOCK_WAIT_TIMEOUT
INSERT INTO t1 (a) SELECT a+100 FROM t1; 
--source ../strict_check_errors.inc

--sorted_result
SELECT a FROM t1;

connection con2;
--sorted_result
SELECT a FROM t1; # Inside the transaction
COMMIT;
--sorted_result
SELECT a FROM t1; # Outside the transaction

connection con1;
--sorted_result
SELECT a FROM t1; # Inside the transaction

# Note: INSERT .. SELECT might be tricky, for example for InnoDB
# even with REPEATABLE-READ it works as if it is executed with READ COMMITTED.
# The test will have a 'logical' result for repeatable read, even although
# we currently don't have an engine which works this way.

--let $error_codes = 0,ER_LOCK_WAIT_TIMEOUT
INSERT INTO t1 (a) SELECT a+200 FROM t1; 
--source ../strict_check_errors.inc

--sorted_result
SELECT a FROM t1;
COMMIT;
--sorted_result
SELECT a FROM t1; # Outside the transaction

connection con2;
--sorted_result
SELECT a FROM t1; # After both transactions have committed

connection default;
disconnect con1;
disconnect con2;
DROP TABLE t1;

--source include/wait_until_count_sessions.inc

