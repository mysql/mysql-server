CREATE TABLE t1 (a INT AUTO_INCREMENT PRIMARY KEY, b INT) ENGINE = InnoDB;
INSERT INTO t1 VALUES (1,1),(2,2),(3,3);
COMMIT;
SET AUTOCOMMIT = 0;
CREATE TEMPORARY TABLE t1_tmp ( b INT );
INSERT INTO t1_tmp (b) SELECT b FROM t1 WHERE a = 3;
INSERT INTO t1_tmp (b) SELECT b FROM t1 WHERE a = 2;
SET AUTOCOMMIT = 0;
CREATE TEMPORARY TABLE t2_tmp ( a int, new_a int );
INSERT INTO t2_tmp VALUES (1,51),(2,52),(3,53);
UPDATE t1 SET a = (SELECT new_a FROM t2_tmp WHERE t2_tmp.a = t1.a) WHERE a = 1;
UPDATE t1 SET a = (SELECT new_a FROM t2_tmp WHERE t2_tmp.a = t1.a) WHERE a = 2;
INSERT INTO t1_tmp (b) SELECT b FROM t1 WHERE a = 1;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
Reap the server message for connection user2 UPDATE t1 ...
UPDATE t1 SET a = (SELECT new_a FROM t2_tmp WHERE t2_tmp.a = t1.a) WHERE a = 3;
DROP TABLE t1;
