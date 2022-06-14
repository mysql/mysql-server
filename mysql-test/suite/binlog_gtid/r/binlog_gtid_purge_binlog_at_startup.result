RESET MASTER;
CREATE TABLE t (a INT);
include/stop_mysqld.inc [server 1]
# restart: --binlog-expire-logs-seconds=1
include/assert.inc [GTID_PURGED is consistent]
include/assert.inc [We have 1 binlog file]
DROP TABLE t;
RESET MASTER;
CREATE TABLE t (a INT);
FLUSH LOGS;
INSERT INTO t VALUES (1), (2), (3);
include/stop_mysqld.inc [server 1]
# restart: --binlog-expire-logs-seconds=5
include/assert.inc [GTID_PURGED is consistent]
include/assert.inc [We have 1 binlog file]
DROP TABLE t;
SET GLOBAL binlog_expire_logs_seconds = SAVED_EXPIRE_SECONDS;
