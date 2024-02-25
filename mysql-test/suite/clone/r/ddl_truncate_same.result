INSTALL PLUGIN clone SONAME 'CLONE_PLUGIN';
# Create test schema
CREATE TABLE t1(col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB);
CREATE TABLE t2(col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB)
PARTITION BY KEY(col1) PARTITIONS 5;
CREATE PROCEDURE execute_dml(
p_dml_type	INT,
p_key_min	INT,
p_key_range	INT,
p_loop_count	INT,
p_frequency	INT,
p_is_rand	INT)
BEGIN
DECLARE v_idx		INT DEFAULT 0;
DECLARE v_commit	INT DEFAULT 0;
DECLARE v_key		INT DEFAULT 0;
/* Loop and INSERT data at random position */
WHILE(v_idx < p_loop_count) DO
/* Generate key between 1 to p_loop_count */
IF p_is_rand = 1 THEN
SET v_key = p_key_min + FLOOR(RAND() * p_key_range);
ELSE
SET v_key = p_key_min + (v_idx  % p_key_range);
END IF;
CASE p_dml_type
WHEN 0 THEN
SET @clol3_text = CONCAT('Clone Test Row - ', v_key);
INSERT INTO t1 (col1, col2, col3, col4) VALUES (
v_key, v_key * 10,
@clol3_text, REPEAT('Large Column Data ', 2048))
ON DUPLICATE KEY UPDATE col2 = col2 + 1;
INSERT INTO t2 (col1, col2, col3, col4) VALUES (
v_key, v_key * 10,
@clol3_text, REPEAT('Large Column Data ', 2048))
ON DUPLICATE KEY UPDATE col2 = col2 + 1;
WHEN 1 THEN
UPDATE t1 SET col2 = v_idx + 1 WHERE col1 = v_key;
UPDATE t2 SET col2 = v_idx + 1 WHERE col1 = v_key;
WHEN 2 THEN
DELETE FROM t1 WHERE col1 = v_key;
DELETE FROM t2 WHERE col1 = v_key;
ELSE
DELETE FROM t1;
DELETE FROM t2;
END CASE;
SET v_idx = v_idx + 1;
/* Commit or rollback work at specified frequency. */
IF v_idx % p_frequency = 0 THEN
SET v_commit = FLOOR(RAND() * 2);
IF v_commit = 0 AND p_is_rand = 1 THEN
ROLLBACK;
START TRANSACTION;
ELSE
COMMIT;
START TRANSACTION;
END IF;
END IF;
END WHILE;
COMMIT;
END|
CALL execute_dml(0, 0, 10, 10, 10, 0);
SET GLOBAL DEBUG = '+d,local_release_clone_file_pin';
# 1. Truncate same table in various stages of CLONE
# In connection DEFAULT
SET DEBUG_SYNC = 'clone_before_init_meta SIGNAL start_ddl_0 WAIT_FOR resume_clone_0';
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl_1 WAIT_FOR resume_clone_1';
SET DEBUG_SYNC = 'clone_file_copy_end_before_ack SIGNAL start_ddl_2 WAIT_FOR resume_clone_2';
SET DEBUG_SYNC = 'clone_before_file_ddl_meta SIGNAL start_ddl_3 WAIT_FOR resume_clone_3';
SET DEBUG_SYNC = 'clone_page_copy SIGNAL start_ddl_4 WAIT_FOR resume_clone_4';
SET DEBUG_SYNC = 'clone_before_page_ddl_meta SIGNAL start_ddl_5 WAIT_FOR resume_clone_5';
SET DEBUG_SYNC = 'clone_redo_copy SIGNAL start_ddl_6 WAIT_FOR resume_clone_6';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
CLONE LOCAL DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
# Waiting for clone
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_0';
# Wait finished
TRUNCATE TABLE t1;
TRUNCATE TABLE t2;
CALL execute_dml(0, 0, 10, 10, 10, 0);
SET DEBUG_SYNC = 'now SIGNAL resume_clone_0';
# Waiting for clone
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_1';
# Wait finished
TRUNCATE TABLE t1;
TRUNCATE TABLE t2;
CALL execute_dml(0, 0, 10, 10, 10, 0);
SET DEBUG_SYNC = 'now SIGNAL resume_clone_1';
# Waiting for clone
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_2';
# Wait finished
TRUNCATE TABLE t1;
TRUNCATE TABLE t2;
CALL execute_dml(0, 0, 10, 10, 10, 0);
SET DEBUG_SYNC = 'now SIGNAL resume_clone_2';
# Waiting for clone
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_3';
# Wait finished
TRUNCATE TABLE t1;
TRUNCATE TABLE t2;
CALL execute_dml(0, 0, 10, 10, 10, 0);
SET DEBUG_SYNC = 'now SIGNAL resume_clone_3';
# Waiting for clone
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_4';
# Wait finished
TRUNCATE TABLE t1;
TRUNCATE TABLE t2;
CALL execute_dml(0, 0, 10, 10, 10, 0);
SET DEBUG_SYNC = 'now SIGNAL resume_clone_4';
# Waiting for clone
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_5';
# Wait finished
TRUNCATE TABLE t1;
TRUNCATE TABLE t2;
CALL execute_dml(0, 0, 10, 10, 10, 0);
SET DEBUG_SYNC = 'now SIGNAL resume_clone_5';
# Waiting for clone
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_6';
# Wait finished
TRUNCATE TABLE t1;
TRUNCATE TABLE t2;
CALL execute_dml(0, 0, 10, 10, 10, 0);
SET DEBUG_SYNC = 'now SIGNAL resume_clone_6';
# In connection DEFAULT
# Waiting for clone to complete
# Wait finished
SET GLOBAL DEBUG = '-d,local_release_clone_file_pin';
SET DEBUG_SYNC = 'RESET';
# Restart cloned database
# restart: --datadir=CLONE_DATADIR
select STATE, ERROR_NO, length(BINLOG_FILE) > 0, length(GTID_EXECUTED) > 0,
(BINLOG_POSITION > 0) from performance_schema.clone_status;
STATE	ERROR_NO	length(BINLOG_FILE) > 0	length(GTID_EXECUTED) > 0	(BINLOG_POSITION > 0)
Completed	0	1	0	1
CALL execute_dml(3, 0, 1, 1, 1, 0);
CALL execute_dml(0, 0, 10, 10, 10, 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` varchar(64) DEFAULT NULL,
  `col4` blob,
  PRIMARY KEY (`col1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT count(*) from t1;
count(*)
10
SELECT col1, col2, col3 FROM t1 ORDER BY col1 LIMIT 10;
col1	col2	col3
0	0	Clone Test Row - 0
1	10	Clone Test Row - 1
2	20	Clone Test Row - 2
3	30	Clone Test Row - 3
4	40	Clone Test Row - 4
5	50	Clone Test Row - 5
6	60	Clone Test Row - 6
7	70	Clone Test Row - 7
8	80	Clone Test Row - 8
9	90	Clone Test Row - 9
SELECT col1, col2, col3 FROM t1 ORDER BY col1 DESC LIMIT 10;
col1	col2	col3
9	90	Clone Test Row - 9
8	80	Clone Test Row - 8
7	70	Clone Test Row - 7
6	60	Clone Test Row - 6
5	50	Clone Test Row - 5
4	40	Clone Test Row - 4
3	30	Clone Test Row - 3
2	20	Clone Test Row - 2
1	10	Clone Test Row - 1
0	0	Clone Test Row - 0
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` varchar(64) DEFAULT NULL,
  `col4` blob,
  PRIMARY KEY (`col1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY KEY (col1)
PARTITIONS 5 */
SELECT count(*) from t2;
count(*)
10
SELECT col1, col2, col3 FROM t2 ORDER BY col1 LIMIT 10;
col1	col2	col3
0	0	Clone Test Row - 0
1	10	Clone Test Row - 1
2	20	Clone Test Row - 2
3	30	Clone Test Row - 3
4	40	Clone Test Row - 4
5	50	Clone Test Row - 5
6	60	Clone Test Row - 6
7	70	Clone Test Row - 7
8	80	Clone Test Row - 8
9	90	Clone Test Row - 9
SELECT col1, col2, col3 FROM t2 ORDER BY col1 DESC LIMIT 10;
col1	col2	col3
9	90	Clone Test Row - 9
8	80	Clone Test Row - 8
7	70	Clone Test Row - 7
6	60	Clone Test Row - 6
5	50	Clone Test Row - 5
4	40	Clone Test Row - 4
3	30	Clone Test Row - 3
2	20	Clone Test Row - 2
1	10	Clone Test Row - 1
0	0	Clone Test Row - 0
# Cleanup
# restart:
DROP TABLE t1;
DROP TABLE t2;
DROP PROCEDURE execute_dml;
# Uninstall clone
UNINSTALL PLUGIN clone;
