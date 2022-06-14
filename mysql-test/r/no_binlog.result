SHOW BINARY LOGS;
ERROR HY000: You are not using binary logging
#
# Verify that The log-replica-updates
# is disabled if binary log is disabled.
#
SELECT @@GLOBAL.log_bin;
@@GLOBAL.log_bin
0
SELECT @@GLOBAL.log_replica_updates;
@@GLOBAL.log_replica_updates
0
#
# Bug#32234194: DDL: DIAGNOSTICS_AREA::SET_OK_STATUS: ASSERTION `!IS_SET()'
#
CREATE TABLE t1(a INT);
ALTER TABLE t1 MODIFY COLUMN a DECIMAL DEFAULT '4648-04-10';
ERROR HY000: Incorrect decimal value: '4648-04-10' for column 'a' at row 1
DROP TABLE t1;
