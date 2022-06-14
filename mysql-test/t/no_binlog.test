# Should be tested against "binlog disabled" server
-- source include/not_log_bin.inc

# BUG#50780: 'show binary logs' debug assertion when binary logging is disabled

-- error ER_NO_BINARY_LOGGING
SHOW BINARY LOGS;

--echo #
--echo # Verify that The log-replica-updates
--echo # is disabled if binary log is disabled.
--echo #
SELECT @@GLOBAL.log_bin;
SELECT @@GLOBAL.log_replica_updates;

--echo #
--echo # Bug#32234194: DDL: DIAGNOSTICS_AREA::SET_OK_STATUS: ASSERTION `!IS_SET()'
--echo #

CREATE TABLE t1(a INT);
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
ALTER TABLE t1 MODIFY COLUMN a DECIMAL DEFAULT '4648-04-10';
DROP TABLE t1;
