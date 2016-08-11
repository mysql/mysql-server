FLUSH BINARY LOGS;
CREATE TABLE t1 (c1 INT);
INSERT INTO t1 VALUES (1);
DROP TABLE t1;
PURGE BINARY LOGS TO 'BINLOG_FILE';
include/assert_grep.inc [Only two events were read from the binary log file (binlog_gtid_simple_recovery=TRUE).]
