CREATE TABLE t(s INT);
SET GLOBAL binlog_group_commit_sync_delay = 32;
SET GLOBAL binlog_group_commit_sync_no_delay_count = 0;
BEGIN;
INSERT INTO t VALUES(10);
COMMIT;
SET GLOBAL binlog_group_commit_sync_delay = 0;
SET GLOBAL binlog_group_commit_sync_no_delay_count = 0;
DROP TABLE t;
