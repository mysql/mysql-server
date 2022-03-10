INSTALL PLUGIN clone SONAME 'CLONE_PLUGIN';
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
call execute_dml(0, 0, 100, 100, 10, 0);
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
100
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 LIMIT 10;
col1	col2	col3	SUBSTRING(col4, 1000, 32)
0	0	Clone Test Row - 0	umn Data Large Column Data Large
1	10	Clone Test Row - 1	umn Data Large Column Data Large
2	20	Clone Test Row - 2	umn Data Large Column Data Large
3	30	Clone Test Row - 3	umn Data Large Column Data Large
4	40	Clone Test Row - 4	umn Data Large Column Data Large
5	50	Clone Test Row - 5	umn Data Large Column Data Large
6	60	Clone Test Row - 6	umn Data Large Column Data Large
7	70	Clone Test Row - 7	umn Data Large Column Data Large
8	80	Clone Test Row - 8	umn Data Large Column Data Large
9	90	Clone Test Row - 9	umn Data Large Column Data Large
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 DESC LIMIT 10;
col1	col2	col3	SUBSTRING(col4, 1000, 32)
99	990	Clone Test Row - 99	umn Data Large Column Data Large
98	980	Clone Test Row - 98	umn Data Large Column Data Large
97	970	Clone Test Row - 97	umn Data Large Column Data Large
96	960	Clone Test Row - 96	umn Data Large Column Data Large
95	950	Clone Test Row - 95	umn Data Large Column Data Large
94	940	Clone Test Row - 94	umn Data Large Column Data Large
93	930	Clone Test Row - 93	umn Data Large Column Data Large
92	920	Clone Test Row - 92	umn Data Large Column Data Large
91	910	Clone Test Row - 91	umn Data Large Column Data Large
90	900	Clone Test Row - 90	umn Data Large Column Data Large
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
100
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 LIMIT 10;
col1	col2	col3	SUBSTRING(col4, 1000, 32)
0	0	Clone Test Row - 0	umn Data Large Column Data Large
1	10	Clone Test Row - 1	umn Data Large Column Data Large
2	20	Clone Test Row - 2	umn Data Large Column Data Large
3	30	Clone Test Row - 3	umn Data Large Column Data Large
4	40	Clone Test Row - 4	umn Data Large Column Data Large
5	50	Clone Test Row - 5	umn Data Large Column Data Large
6	60	Clone Test Row - 6	umn Data Large Column Data Large
7	70	Clone Test Row - 7	umn Data Large Column Data Large
8	80	Clone Test Row - 8	umn Data Large Column Data Large
9	90	Clone Test Row - 9	umn Data Large Column Data Large
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 DESC LIMIT 10;
col1	col2	col3	SUBSTRING(col4, 1000, 32)
99	990	Clone Test Row - 99	umn Data Large Column Data Large
98	980	Clone Test Row - 98	umn Data Large Column Data Large
97	970	Clone Test Row - 97	umn Data Large Column Data Large
96	960	Clone Test Row - 96	umn Data Large Column Data Large
95	950	Clone Test Row - 95	umn Data Large Column Data Large
94	940	Clone Test Row - 94	umn Data Large Column Data Large
93	930	Clone Test Row - 93	umn Data Large Column Data Large
92	920	Clone Test Row - 92	umn Data Large Column Data Large
91	910	Clone Test Row - 91	umn Data Large Column Data Large
90	900	Clone Test Row - 90	umn Data Large Column Data Large
# In connection default - Cloning database
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_dml1 WAIT_FOR resume_clone1';
SET DEBUG_SYNC = 'clone_page_copy SIGNAL start_dml2 WAIT_FOR resume_clone2';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection con1 - Running Update Random [0 - 100 Key Range]
SET DEBUG_SYNC = 'now WAIT_FOR start_dml1';
START TRANSACTION;
CALL execute_dml(1, 0, 100, 500, 50, 1);
COMMIT;
# Flush all dirty buffers
SET GLOBAL innodb_buf_flush_list_now = 1;
SET DEBUG_SYNC = 'now SIGNAL resume_clone1';
SET DEBUG_SYNC = 'now WAIT_FOR start_dml2';
START TRANSACTION;
CALL execute_dml(1, 0, 100, 300, 50, 1);
COMMIT;
SET DEBUG_SYNC = 'now SIGNAL resume_clone2';
# In connection default - Cloning database
# Restart cloned database
# restart: --datadir=CLONE_DATADIR
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
100
SELECT col1, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 LIMIT 10;
col1	col3	SUBSTRING(col4, 1000, 32)
0	Clone Test Row - 0	umn Data Large Column Data Large
1	Clone Test Row - 1	umn Data Large Column Data Large
2	Clone Test Row - 2	umn Data Large Column Data Large
3	Clone Test Row - 3	umn Data Large Column Data Large
4	Clone Test Row - 4	umn Data Large Column Data Large
5	Clone Test Row - 5	umn Data Large Column Data Large
6	Clone Test Row - 6	umn Data Large Column Data Large
7	Clone Test Row - 7	umn Data Large Column Data Large
8	Clone Test Row - 8	umn Data Large Column Data Large
9	Clone Test Row - 9	umn Data Large Column Data Large
SELECT col1, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 DESC LIMIT 10;
col1	col3	SUBSTRING(col4, 1000, 32)
99	Clone Test Row - 99	umn Data Large Column Data Large
98	Clone Test Row - 98	umn Data Large Column Data Large
97	Clone Test Row - 97	umn Data Large Column Data Large
96	Clone Test Row - 96	umn Data Large Column Data Large
95	Clone Test Row - 95	umn Data Large Column Data Large
94	Clone Test Row - 94	umn Data Large Column Data Large
93	Clone Test Row - 93	umn Data Large Column Data Large
92	Clone Test Row - 92	umn Data Large Column Data Large
91	Clone Test Row - 91	umn Data Large Column Data Large
90	Clone Test Row - 90	umn Data Large Column Data Large
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
100
SELECT col1, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 LIMIT 10;
col1	col3	SUBSTRING(col4, 1000, 32)
0	Clone Test Row - 0	umn Data Large Column Data Large
1	Clone Test Row - 1	umn Data Large Column Data Large
2	Clone Test Row - 2	umn Data Large Column Data Large
3	Clone Test Row - 3	umn Data Large Column Data Large
4	Clone Test Row - 4	umn Data Large Column Data Large
5	Clone Test Row - 5	umn Data Large Column Data Large
6	Clone Test Row - 6	umn Data Large Column Data Large
7	Clone Test Row - 7	umn Data Large Column Data Large
8	Clone Test Row - 8	umn Data Large Column Data Large
9	Clone Test Row - 9	umn Data Large Column Data Large
SELECT col1, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 DESC LIMIT 10;
col1	col3	SUBSTRING(col4, 1000, 32)
99	Clone Test Row - 99	umn Data Large Column Data Large
98	Clone Test Row - 98	umn Data Large Column Data Large
97	Clone Test Row - 97	umn Data Large Column Data Large
96	Clone Test Row - 96	umn Data Large Column Data Large
95	Clone Test Row - 95	umn Data Large Column Data Large
94	Clone Test Row - 94	umn Data Large Column Data Large
93	Clone Test Row - 93	umn Data Large Column Data Large
92	Clone Test Row - 92	umn Data Large Column Data Large
91	Clone Test Row - 91	umn Data Large Column Data Large
90	Clone Test Row - 90	umn Data Large Column Data Large
call execute_dml(3, 0, 1, 1, 1, 0);
call execute_dml(0, 0, 100, 100, 10, 0);
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 LIMIT 10;
col1	col2	col3	SUBSTRING(col4, 1000, 32)
0	0	Clone Test Row - 0	umn Data Large Column Data Large
1	10	Clone Test Row - 1	umn Data Large Column Data Large
2	20	Clone Test Row - 2	umn Data Large Column Data Large
3	30	Clone Test Row - 3	umn Data Large Column Data Large
4	40	Clone Test Row - 4	umn Data Large Column Data Large
5	50	Clone Test Row - 5	umn Data Large Column Data Large
6	60	Clone Test Row - 6	umn Data Large Column Data Large
7	70	Clone Test Row - 7	umn Data Large Column Data Large
8	80	Clone Test Row - 8	umn Data Large Column Data Large
9	90	Clone Test Row - 9	umn Data Large Column Data Large
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 DESC LIMIT 10;
col1	col2	col3	SUBSTRING(col4, 1000, 32)
99	990	Clone Test Row - 99	umn Data Large Column Data Large
98	980	Clone Test Row - 98	umn Data Large Column Data Large
97	970	Clone Test Row - 97	umn Data Large Column Data Large
96	960	Clone Test Row - 96	umn Data Large Column Data Large
95	950	Clone Test Row - 95	umn Data Large Column Data Large
94	940	Clone Test Row - 94	umn Data Large Column Data Large
93	930	Clone Test Row - 93	umn Data Large Column Data Large
92	920	Clone Test Row - 92	umn Data Large Column Data Large
91	910	Clone Test Row - 91	umn Data Large Column Data Large
90	900	Clone Test Row - 90	umn Data Large Column Data Large
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 LIMIT 10;
col1	col2	col3	SUBSTRING(col4, 1000, 32)
0	0	Clone Test Row - 0	umn Data Large Column Data Large
1	10	Clone Test Row - 1	umn Data Large Column Data Large
2	20	Clone Test Row - 2	umn Data Large Column Data Large
3	30	Clone Test Row - 3	umn Data Large Column Data Large
4	40	Clone Test Row - 4	umn Data Large Column Data Large
5	50	Clone Test Row - 5	umn Data Large Column Data Large
6	60	Clone Test Row - 6	umn Data Large Column Data Large
7	70	Clone Test Row - 7	umn Data Large Column Data Large
8	80	Clone Test Row - 8	umn Data Large Column Data Large
9	90	Clone Test Row - 9	umn Data Large Column Data Large
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 DESC LIMIT 10;
col1	col2	col3	SUBSTRING(col4, 1000, 32)
99	990	Clone Test Row - 99	umn Data Large Column Data Large
98	980	Clone Test Row - 98	umn Data Large Column Data Large
97	970	Clone Test Row - 97	umn Data Large Column Data Large
96	960	Clone Test Row - 96	umn Data Large Column Data Large
95	950	Clone Test Row - 95	umn Data Large Column Data Large
94	940	Clone Test Row - 94	umn Data Large Column Data Large
93	930	Clone Test Row - 93	umn Data Large Column Data Large
92	920	Clone Test Row - 92	umn Data Large Column Data Large
91	910	Clone Test Row - 91	umn Data Large Column Data Large
90	900	Clone Test Row - 90	umn Data Large Column Data Large
# restart:
DROP TABLE t1;
DROP TABLE t2;
DROP PROCEDURE execute_dml;
UNINSTALL PLUGIN clone;
SET DEBUG_SYNC = 'RESET';
