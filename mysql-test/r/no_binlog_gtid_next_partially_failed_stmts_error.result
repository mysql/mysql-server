RESET MASTER;
SET SESSION GTID_NEXT='MASTER_UUID:1';
CREATE TABLE t1 (a int) ENGINE=MyISAM;
SET SESSION GTID_NEXT='MASTER_UUID:2';
CREATE TABLE t2 (a int) ENGINE=InnoDB;
SET SESSION GTID_NEXT='MASTER_UUID:3';
#
# Execute a partially failed DROP TABLE statement.
#
SET SESSION debug="+d,simulate_err_on_write_gtid_into_table";
SET SESSION debug="+d,rm_table_no_locks_abort_before_atomic_tables";
DROP TABLE t1, t2;
ERROR HY000: Unknown error
SET SESSION debug="-d,rm_table_no_locks_abort_before_atomic_tables";
SET SESSION debug="-d,simulate_err_on_write_gtid_into_table";
#
# The table t1 was dropped, which means DROP TABLE
# can be failed partially.
#
SHOW CREATE TABLE t1;
ERROR 42S02: Table 'test.t1' doesn't exist
#
# When binlog is disabled, verify that the partially failed
# DROP TABLE statement do not commit its gtid into both
# @@GLOBAL.GTID_EXECUTED and mysql.GTID_EXECUTED table if
# it encounters an error when saving its gtid into
# mysql.GTID_EXECUTED table.
#
include/assert.inc [Did not commit gtid MASTER_UUID:3 into @@GLOBAL.GTID_EXECUTED]
include/assert.inc [Did not save gtid MASTER_UUID:3 into mysql.gtid_executed table]
SET SESSION GTID_NEXT='MASTER_UUID:3';
CREATE USER u1@h;
SET SESSION debug="+d,simulate_err_on_write_gtid_into_table";
SET SESSION GTID_NEXT='MASTER_UUID:4';
DROP USER u1@h, u2@h;
ERROR HY000: Operation DROP USER failed for 'u2'@'h'
SET SESSION debug="-d,simulate_err_on_write_gtid_into_table";
#
# When binlog is disabled, verify that the partially failed
# DROP USER statement do not commit its gtid into both
# @@GLOBAL.GTID_EXECUTED and mysql.GTID_EXECUTED table if
# it encounters an error when saving its gtid into
# mysql.GTID_EXECUTED table.
#
include/assert.inc [Did not commit gtid MASTER_UUID:4 into @@GLOBAL.GTID_EXECUTED]
include/assert.inc [Did not save gtid MASTER_UUID:4 into mysql.gtid_executed table]
SET SESSION GTID_NEXT='MASTER_UUID:5';
DROP USER u1@h;
SET SESSION GTID_NEXT='MASTER_UUID:6';
DROP TABLE t2;
