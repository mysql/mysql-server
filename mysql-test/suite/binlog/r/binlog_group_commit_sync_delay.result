SET GLOBAL binlog_group_commit_sync_delay=1000000;
SET GLOBAL sync_binlog = 1;
SET @clock_in = SYSDATE();
CREATE TABLE t1 (c1 INT) Engine=InnoDB;
SET @elapsed = TIMESTAMPDIFF(MICROSECOND, @clock_in, SYSDATE());
include/assert.inc ["Assert that the statement never takes less than the delay set."]
SET GLOBAL sync_binlog=1000;
SET @clock_in = SYSDATE();
SET @elapsed = TIMESTAMPDIFF(MICROSECOND, @clock_in, SYSDATE());
include/assert.inc ["Assert that the above statements should not take more than 1000 seconds"]
SET GLOBAL binlog_group_commit_sync_delay=0;
SET GLOBAL sync_binlog=1;
DROP TABLE t1;
