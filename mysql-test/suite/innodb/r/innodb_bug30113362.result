SET GLOBAL innodb_adaptive_hash_index = false;
SET GLOBAL innodb_stats_persistent = false;
SET GLOBAL innodb_flush_log_at_trx_commit = 0;
CREATE TABLE t1 (
a00 CHAR(255) NOT NULL DEFAULT 'a',
a01 CHAR(255) NOT NULL DEFAULT 'a',
a02 CHAR(255) NOT NULL DEFAULT 'a',
b INT NOT NULL DEFAULT 0,
CONSTRAINT pkey PRIMARY KEY(a00, a01, a02)
) charset latin1 ENGINE = InnoDB COMMENT='MERGE_THRESHOLD=45';
SET GLOBAL innodb_limit_optimistic_insert_debug = 3;
DROP PROCEDURE IF EXISTS data_load_t1;
Warnings:
Note	1305	PROCEDURE test.data_load_t1 does not exist
CREATE PROCEDURE data_load_t1()
BEGIN
DECLARE c1 INT DEFAULT 97;
DECLARE c2 INT DEFAULT 97;
DECLARE c3 INT DEFAULT 97;
WHILE c1 < 102 DO
WHILE c2 < 123 DO
WHILE c3 < 123 DO
INSERT INTO t1 (a00) VALUES (CHAR(c1,c2,c3));
SET c3 = c3 + 1;
END WHILE;
SET c3 = 97;
SET c2 = c2 + 1;
END WHILE;
SET c2 = 97;
SET c1 = c1 + 1;
END WHILE;
END |
call data_load_t1();
DROP PROCEDURE data_load_t1;
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT CLUST_INDEX_SIZE FROM information_schema.INNODB_TABLESTATS WHERE NAME = 'test/t1';
CLUST_INDEX_SIZE
1856
SET GLOBAL innodb_limit_optimistic_insert_debug = 0;
DELETE FROM t1 WHERE a00 = 'cnm';
# Test start
SET DEBUG_SYNC = 'RESET';
SET GLOBAL innodb_purge_stop_now = ON;
SET GLOBAL DEBUG = "+d,pessimistic_row_purge_clust";
DELETE FROM t1 WHERE a00 = 'bii';
SET GLOBAL innodb_purge_run_now = ON;
SET DEBUG_SYNC = "now WAIT_FOR pessimistic_row_purge_clust_pause";
SET DEBUG_SYNC = 'rw_s_lock_waiting SIGNAL lockwait1';
SELECT a00 FROM t1 WHERE a00 = 'bcc';
SET DEBUG_SYNC = 'now WAIT_FOR lockwait1';
SET GLOBAL DEBUG = "-d,pessimistic_row_purge_clust";
SET DEBUG_SYNC = "now SIGNAL pessimistic_row_purge_clust_continue";
a00
bcc
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT CLUST_INDEX_SIZE FROM information_schema.INNODB_TABLESTATS WHERE NAME = 'test/t1';
CLUST_INDEX_SIZE
1856
SET DEBUG_SYNC = 'RESET';
DELETE FROM t1 WHERE a00 = 'dpn';
ALTER TABLE t1 COMMENT='MERGE_THRESHOLD=35';
SET GLOBAL innodb_purge_stop_now = ON;
SET GLOBAL DEBUG = "+d,pessimistic_row_purge_clust";
DELETE FROM t1 WHERE a00 = 'cnd';
SET GLOBAL innodb_purge_run_now = ON;
SET DEBUG_SYNC = "now WAIT_FOR pessimistic_row_purge_clust_pause";
SELECT a00 FROM t1 WHERE a00 = 'ass';
a00
ass
SELECT a00 FROM t1 WHERE a00 = 'dyx';
a00
dyx
SET DEBUG_SYNC = 'rw_s_lock_waiting SIGNAL lockwait1';
SELECT a00 FROM t1 WHERE a00 = 'ast';
SET DEBUG_SYNC = 'rw_s_lock_waiting SIGNAL lockwait2';
SELECT a00 FROM t1 WHERE a00 = 'dyw';
SET DEBUG_SYNC = 'now WAIT_FOR lockwait1';
SET DEBUG_SYNC = 'now WAIT_FOR lockwait2';
SET GLOBAL DEBUG = "-d,pessimistic_row_purge_clust";
SET DEBUG_SYNC = "now SIGNAL pessimistic_row_purge_clust_continue";
a00
ast
a00
dyw
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT CLUST_INDEX_SIZE FROM information_schema.INNODB_TABLESTATS WHERE NAME = 'test/t1';
CLUST_INDEX_SIZE
1856
SET DEBUG_SYNC = 'RESET';
DROP TABLE t1;
