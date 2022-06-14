#
# Bug#25966845 INSERT ON DUPLICATE KEY GENERATE A DEADLOCK
#
CREATE TABLE t1(f1 int primary key,
f2 int, f3 int, unique key(f2))engine=innodb;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  `f3` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2` (`f2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
INSERT INTO t1(f1, f2, f3) VALUES(1, 10, 100);
# Connection default
BEGIN;
INSERT INTO t1 VALUES(2, 20, 300) ON DUPLICATE KEY UPDATE f3 = 500;
# Connection other:
BEGIN;
SET DEBUG_SYNC = 'ib_after_row_insert_step SIGNAL default_commit';
INSERT INTO t1 VALUES(2, 10, 200) ON DUPLICATE KEY UPDATE f3 = 120;
# Connection default
SET DEBUG_SYNC = 'now WAIT_FOR default_commit';
COMMIT;
# Connection other
COMMIT;
# Verify Results:
SELECT * FROM t1;
f1	f2	f3
1	10	100
2	20	120
SET DEBUG_SYNC ='RESET';
DROP TABLE t1;
#
# Bug #29195848	ASSERTION "!OTHER_LOCK" IN LOCK_REC_ADD_TO_QUEUE
#
SET GLOBAL innodb_purge_stop_now = ON;
CREATE TABLE t1 (id INT NOT NULL PRIMARY KEY, b INT, UNIQUE KEY(b));
INSERT INTO t1 VALUES(2, 300);
DELETE FROM t1;
INSERT INTO t1 VALUES(3, 300);
SELECT * FROM t1;
id	b
3	300
SET DEBUG_SYNC='ib_after_row_insert_step SIGNAL after_insert WAIT_FOR
rollback';
INSERT INTO t1 VALUES(2, 300);;
SET DEBUG_SYNC='now WAIT_FOR after_insert';
START TRANSACTION;
INSERT INTO t1 VALUES(1, 300);
ERROR 23000: Duplicate entry '300' for key 't1.b'
SET GLOBAL innodb_purge_run_now=ON;
SET DEBUG_SYNC='now SIGNAL rollback';
ERROR 23000: Duplicate entry '300' for key 't1.b'
COMMIT;
DROP TABLE t1;
SET GLOBAL innodb_purge_stop_now = ON;
CREATE TABLE t1 (a INT PRIMARY KEY, b INT, UNIQUE KEY(b));
INSERT INTO t1 VALUES(1,10);
DELETE FROM t1;
INSERT INTO t1 VALUES(2,10);
SET DEBUG_SYNC='ib_undo_mod_before_remove_clust SIGNAL during_rollback
WAIT_FOR rollback';
INSERT INTO t1 VALUES(1, 10) ;;
SET DEBUG_SYNC='now WAIT_FOR during_rollback';
SELECT * FROM t1 WHERE a = 1 LOCK IN SHARE MODE;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET GLOBAL innodb_purge_run_now=ON;
SET DEBUG_SYNC='now SIGNAL rollback';
ERROR 23000: Duplicate entry '10' for key 't1.b'
SELECT * FROM t1;
a	b
2	10
DROP TABLE t1;
#
# Bug #29718243	MYQL SERVER CRASHING
#
CREATE TEMPORARY TABLE tmpTest(tmpField INT , UNIQUE KEY uq_tmpField (tmpField));
CREATE TEMPORARY TABLE tmpTest1(tmpField INT , UNIQUE KEY uq_tmpField (tmpField));
CREATE FUNCTION ZZtest() RETURNS int(11)
BEGIN
DECLARE l_total INTEGER;
SET l_total = 0;
INSERT INTO tmpTest SET tmpField = 40;
INSERT IGNORE INTO tmpTest SET tmpField = 40;
INSERT IGNORE INTO tmpTest1 SET tmpField = 40;
DROP TEMPORARY TABLE IF EXISTS tmpTest;
DROP TEMPORARY TABLE IF EXISTS tmpTest1;
RETURN l_total;
END|
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
SELECT ZZtest() AS test;
test
0
DROP FUNCTION ZZtest;
