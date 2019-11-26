#
# Test that $query correctly checks read_only flag even if this
# flag is set while the query is blocked waiting for protection
# global read lock. Used by test for bug#28438114 in read_only.test.
#

--enable_connect_log
connection default;
FLUSH TABLES WITH READ LOCK;

connection con1;
--echo # DDL statement will block waiting for GRL.
--send_eval $query

connection con2;
--echo # Wait until statement is blocked waiting for GRL.
let $wait_condition=
  SELECT COUNT(*) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE STATE = "Waiting for global read lock";
--source include/wait_condition.inc
SET GLOBAL READ_ONLY= 1;

connection default;
--echo # Unblock the DDL statement.
UNLOCK TABLES;

connection con1;
--echo # Without patch for BUG#28438114, the DDL statement will succeed even
--echo # though read only mode was set.
--error ER_OPTION_PREVENTS_STATEMENT
--reap

--echo # Clean up.
connection default;
SET GLOBAL READ_ONLY= 0;
--disable_connect_log
