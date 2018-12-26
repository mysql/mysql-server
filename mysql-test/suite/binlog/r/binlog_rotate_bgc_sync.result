[connection conn1]
CREATE TABLE t1 (c1 INT) ENGINE = MyISAM;
SET DEBUG_SYNC= 'bgc_between_flush_and_sync SIGNAL
  holding_between_flush_and_sync WAIT_FOR continue_between_flush_and_sync';
INSERT INTO t1 VALUES (1);
[connection conn2]
SET DEBUG_SYNC= 'now WAIT_FOR holding_between_flush_and_sync';
SET DEBUG_SYNC= 'binlog_rotate_between_close_and_open SIGNAL
  holding_between_close_and_open WAIT_FOR continue_rotate_binlog_file';
FLUSH LOGS;
[connection default]
SET DEBUG_SYNC= 'now WAIT_FOR holding_between_close_and_open';
SET DEBUG_SYNC= 'now SIGNAL continue_between_flush_and_sync';
[connection conn1]
[connection default]
SET DEBUG_SYNC = 'now SIGNAL continue_rotate_binlog_file';
[connection conn2]
[connection default]
DROP TABLE t1;
SET DEBUG_SYNC= 'RESET';
