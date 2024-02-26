CREATE TABLE t1(c1 INT) ENGINE= InnoDB;
# Crash right after flushing InnoDB redo log
SET SESSION DEBUG="+d,crash_after_flush_engine_log";
BEGIN;
INSERT INTO t1 VALUES(1);
COMMIT;
ERROR HY000: Lost connection to MySQL server during query
# Restart the master server
#
# Verify that a transaction can not be recovered during server
# recovery from a crash, which happened after flushing it to
# InnoDB redo log and before flushing it to binary log.
#
include/assert.inc [Table t1 must not contain 1]
# Crash right after flushing binary log
SET SESSION DEBUG="+d,crash_after_flush_binlog";
BEGIN;
INSERT INTO t1 VALUES(2);
COMMIT;
ERROR HY000: Lost connection to MySQL server during query
# Restart the master server
#
# Verify that a transaction can be recovered during server
# recovery from a crash, which happened after flushing it
# to binary log.
#
include/assert.inc [Table t1 must contain 2]
DROP TABLE t1;
