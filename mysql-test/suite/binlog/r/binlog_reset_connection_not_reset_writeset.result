SET GLOBAL transaction_write_set_extraction= 'XXHASH64';
SET GLOBAL binlog_transaction_dependency_tracking= 'WRITESET';
CREATE TABLE t1(id INT, PRIMARY KEY (id));
FLUSH LOGS;
INSERT INTO t1(id) VALUES(1);
INSERT INTO t1(id) VALUES(2);
SET GLOBAL binlog_transaction_dependency_tracking= 'WRITESET_SESSION';
INSERT INTO t1(id) VALUES(3);
INSERT INTO t1(id) VALUES(4);
include/save_binlog_position.inc
INSERT INTO t1(id) VALUES(5);
include/include/assert_logical_timestamps.inc [2 5]
SET GLOBAL binlog_transaction_dependency_tracking= COMMIT_ORDER;
SET GLOBAL transaction_write_set_extraction= XXHASH64;
DROP TABLE t1;
