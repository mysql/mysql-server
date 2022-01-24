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
DROP TABLE t1;
DROP TABLE t2;
CREATE TABLE t1(col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB)
ROW_FORMAT = COMPRESSED;
CREATE TABLESPACE tbs1 ADD DATAFILE 'tbs1_data1.ibd';
CREATE TABLE t2(col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB)
TABLESPACE = tbs1;
call execute_dml(0, 0, 20, 20, 10, 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` varchar(64) DEFAULT NULL,
  `col4` blob,
  PRIMARY KEY (`col1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=COMPRESSED
SELECT count(*) from t1;
count(*)
20
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
19	190	Clone Test Row - 19	umn Data Large Column Data Large
18	180	Clone Test Row - 18	umn Data Large Column Data Large
17	170	Clone Test Row - 17	umn Data Large Column Data Large
16	160	Clone Test Row - 16	umn Data Large Column Data Large
15	150	Clone Test Row - 15	umn Data Large Column Data Large
14	140	Clone Test Row - 14	umn Data Large Column Data Large
13	130	Clone Test Row - 13	umn Data Large Column Data Large
12	120	Clone Test Row - 12	umn Data Large Column Data Large
11	110	Clone Test Row - 11	umn Data Large Column Data Large
10	100	Clone Test Row - 10	umn Data Large Column Data Large
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` varchar(64) DEFAULT NULL,
  `col4` blob,
  PRIMARY KEY (`col1`)
) /*!50100 TABLESPACE `tbs1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT count(*) from t2;
count(*)
20
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
19	190	Clone Test Row - 19	umn Data Large Column Data Large
18	180	Clone Test Row - 18	umn Data Large Column Data Large
17	170	Clone Test Row - 17	umn Data Large Column Data Large
16	160	Clone Test Row - 16	umn Data Large Column Data Large
15	150	Clone Test Row - 15	umn Data Large Column Data Large
14	140	Clone Test Row - 14	umn Data Large Column Data Large
13	130	Clone Test Row - 13	umn Data Large Column Data Large
12	120	Clone Test Row - 12	umn Data Large Column Data Large
11	110	Clone Test Row - 11	umn Data Large Column Data Large
10	100	Clone Test Row - 10	umn Data Large Column Data Large
# In connection default - Cloning database
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_insert1 WAIT_FOR resume_clone1';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection con1 - Insert [20 Rows - No commit]
SET DEBUG_SYNC = 'now WAIT_FOR start_insert1';
START TRANSACTION;
call execute_dml(0, 50, 20, 20, 500, 0);
# Flush all dirty buffers
SET GLOBAL innodb_buf_flush_list_now = 1;
SET DEBUG_SYNC = 'now SIGNAL resume_clone1';
# In connection default - Cloning database
# In connection con1
ROLLBACK;
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=COMPRESSED
SELECT count(*) from t1;
count(*)
40
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
69	Clone Test Row - 69	umn Data Large Column Data Large
68	Clone Test Row - 68	umn Data Large Column Data Large
67	Clone Test Row - 67	umn Data Large Column Data Large
66	Clone Test Row - 66	umn Data Large Column Data Large
65	Clone Test Row - 65	umn Data Large Column Data Large
64	Clone Test Row - 64	umn Data Large Column Data Large
63	Clone Test Row - 63	umn Data Large Column Data Large
62	Clone Test Row - 62	umn Data Large Column Data Large
61	Clone Test Row - 61	umn Data Large Column Data Large
60	Clone Test Row - 60	umn Data Large Column Data Large
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` varchar(64) DEFAULT NULL,
  `col4` blob,
  PRIMARY KEY (`col1`)
) /*!50100 TABLESPACE `tbs1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT count(*) from t2;
count(*)
40
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
69	Clone Test Row - 69	umn Data Large Column Data Large
68	Clone Test Row - 68	umn Data Large Column Data Large
67	Clone Test Row - 67	umn Data Large Column Data Large
66	Clone Test Row - 66	umn Data Large Column Data Large
65	Clone Test Row - 65	umn Data Large Column Data Large
64	Clone Test Row - 64	umn Data Large Column Data Large
63	Clone Test Row - 63	umn Data Large Column Data Large
62	Clone Test Row - 62	umn Data Large Column Data Large
61	Clone Test Row - 61	umn Data Large Column Data Large
60	Clone Test Row - 60	umn Data Large Column Data Large
call execute_dml(3, 0, 1, 1, 1, 0);
call execute_dml(0, 0, 10, 10, 2, 0);
commit;
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
9	90	Clone Test Row - 9	umn Data Large Column Data Large
8	80	Clone Test Row - 8	umn Data Large Column Data Large
7	70	Clone Test Row - 7	umn Data Large Column Data Large
6	60	Clone Test Row - 6	umn Data Large Column Data Large
5	50	Clone Test Row - 5	umn Data Large Column Data Large
4	40	Clone Test Row - 4	umn Data Large Column Data Large
3	30	Clone Test Row - 3	umn Data Large Column Data Large
2	20	Clone Test Row - 2	umn Data Large Column Data Large
1	10	Clone Test Row - 1	umn Data Large Column Data Large
0	0	Clone Test Row - 0	umn Data Large Column Data Large
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
9	90	Clone Test Row - 9	umn Data Large Column Data Large
8	80	Clone Test Row - 8	umn Data Large Column Data Large
7	70	Clone Test Row - 7	umn Data Large Column Data Large
6	60	Clone Test Row - 6	umn Data Large Column Data Large
5	50	Clone Test Row - 5	umn Data Large Column Data Large
4	40	Clone Test Row - 4	umn Data Large Column Data Large
3	30	Clone Test Row - 3	umn Data Large Column Data Large
2	20	Clone Test Row - 2	umn Data Large Column Data Large
1	10	Clone Test Row - 1	umn Data Large Column Data Large
0	0	Clone Test Row - 0	umn Data Large Column Data Large
# restart:
# In connection default - Cloning database
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_dml WAIT_FOR resume_clone2';
SET DEBUG_SYNC = 'clone_page_copy SIGNAL start_insert2 WAIT_FOR resume_clone3';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
SET GLOBAL clone_valid_donor_list = 'HOST:PORT';
CLONE INSTANCE FROM USER@HOST:PORT IDENTIFIED BY '' DATA DIRECTORY = 'CLONE_DATADIR';
# In connection con1 - Insert [20 Rows - No commit]
SET DEBUG_SYNC = 'now WAIT_FOR start_dml';
START TRANSACTION;
call execute_dml(1, 0, 20, 20, 10, 1);
COMMIT;
# Flush all dirty buffers
SET GLOBAL innodb_buf_flush_list_now = 1;
SET DEBUG_SYNC = 'now SIGNAL resume_clone2';
SET DEBUG_SYNC = 'now WAIT_FOR start_insert2';
START TRANSACTION;
call execute_dml(0, 50, 20, 20, 500, 0);
# Flush all dirty buffers
SET GLOBAL innodb_buf_flush_list_now = 1;
SET DEBUG_SYNC = 'now SIGNAL resume_clone3';
# In connection default - Cloning database
# In connection con1
ROLLBACK;
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=COMPRESSED
SELECT count(*) from t1;
count(*)
40
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
69	Clone Test Row - 69	umn Data Large Column Data Large
68	Clone Test Row - 68	umn Data Large Column Data Large
67	Clone Test Row - 67	umn Data Large Column Data Large
66	Clone Test Row - 66	umn Data Large Column Data Large
65	Clone Test Row - 65	umn Data Large Column Data Large
64	Clone Test Row - 64	umn Data Large Column Data Large
63	Clone Test Row - 63	umn Data Large Column Data Large
62	Clone Test Row - 62	umn Data Large Column Data Large
61	Clone Test Row - 61	umn Data Large Column Data Large
60	Clone Test Row - 60	umn Data Large Column Data Large
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `col1` int NOT NULL,
  `col2` int DEFAULT NULL,
  `col3` varchar(64) DEFAULT NULL,
  `col4` blob,
  PRIMARY KEY (`col1`)
) /*!50100 TABLESPACE `tbs1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT count(*) from t2;
count(*)
40
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
69	Clone Test Row - 69	umn Data Large Column Data Large
68	Clone Test Row - 68	umn Data Large Column Data Large
67	Clone Test Row - 67	umn Data Large Column Data Large
66	Clone Test Row - 66	umn Data Large Column Data Large
65	Clone Test Row - 65	umn Data Large Column Data Large
64	Clone Test Row - 64	umn Data Large Column Data Large
63	Clone Test Row - 63	umn Data Large Column Data Large
62	Clone Test Row - 62	umn Data Large Column Data Large
61	Clone Test Row - 61	umn Data Large Column Data Large
60	Clone Test Row - 60	umn Data Large Column Data Large
call execute_dml(3, 0, 1, 1, 1, 0);
call execute_dml(0, 0, 10, 10, 2, 0);
commit;
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
9	90	Clone Test Row - 9	umn Data Large Column Data Large
8	80	Clone Test Row - 8	umn Data Large Column Data Large
7	70	Clone Test Row - 7	umn Data Large Column Data Large
6	60	Clone Test Row - 6	umn Data Large Column Data Large
5	50	Clone Test Row - 5	umn Data Large Column Data Large
4	40	Clone Test Row - 4	umn Data Large Column Data Large
3	30	Clone Test Row - 3	umn Data Large Column Data Large
2	20	Clone Test Row - 2	umn Data Large Column Data Large
1	10	Clone Test Row - 1	umn Data Large Column Data Large
0	0	Clone Test Row - 0	umn Data Large Column Data Large
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
9	90	Clone Test Row - 9	umn Data Large Column Data Large
8	80	Clone Test Row - 8	umn Data Large Column Data Large
7	70	Clone Test Row - 7	umn Data Large Column Data Large
6	60	Clone Test Row - 6	umn Data Large Column Data Large
5	50	Clone Test Row - 5	umn Data Large Column Data Large
4	40	Clone Test Row - 4	umn Data Large Column Data Large
3	30	Clone Test Row - 3	umn Data Large Column Data Large
2	20	Clone Test Row - 2	umn Data Large Column Data Large
1	10	Clone Test Row - 1	umn Data Large Column Data Large
0	0	Clone Test Row - 0	umn Data Large Column Data Large
# restart:
DROP TABLE t1;
DROP TABLE t2;
DROP PROCEDURE execute_dml;
DROP TABLESPACE tbs1;
UNINSTALL PLUGIN clone;
SET DEBUG_SYNC = 'RESET';
