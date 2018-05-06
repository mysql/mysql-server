# This file is a collection of regression and coverage tests
# for WL#4897: Add EXPLAIN INSERT/UPDATE/DELETE.

-- disable_query_log
-- disable_result_log
SET GLOBAL innodb_stats_persistent=0;
-- enable_result_log
-- enable_query_log

set end_markers_in_json=on;

--echo #1
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
--let $query =  UPDATE t1 SET a = 10 WHERE a < 10
--let $select = SELECT * FROM t1     WHERE a < 10
--source include/explain_utils.inc
DROP TABLE t1;

--echo #2
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
--let $query =  DELETE   FROM t1 WHERE a < 10
--let $select = SELECT * FROM t1 WHERE a < 10
--source include/explain_utils.inc
DROP TABLE t1;

--echo #3
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
--let $query =  DELETE   FROM t1 USING t1 WHERE a = 1
--let $select = SELECT * FROM t1          WHERE a = 1
--source include/explain_utils.inc
DROP TABLE t1;

--echo #4
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (b INT);
INSERT INTO t2 VALUES (1), (2), (3);
--let $query =  UPDATE        t1, t2 SET t1.a = 10 WHERE t1.a = 1
--let $select = SELECT * FROM t1, t2               WHERE t1.a = 1
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #5
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (b INT);
INSERT INTO t2 VALUES (1), (2), (3);
--let $query =  UPDATE        t1 t11, (SELECT * FROM t2) t12 SET t11.a = 10 WHERE t11.a = 1
--let $select = SELECT * FROM t1 t11, (SELECT * FROM t2) t12                WHERE t11.a = 1
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #6
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (b INT);
INSERT INTO t2 VALUES (1), (2), (3);
--let $query =  UPDATE        t1 SET a = 10 WHERE 1 IN (SELECT 1 FROM t2 WHERE t2.b < 3)
--let $select = SELECT * FROM t1            WHERE 1 IN (SELECT 1 FROM t2 WHERE t2.b < 3)
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #7
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (b INT);
INSERT INTO t2 VALUES (1), (2), (3);
--let $query =  UPDATE        t1 SET a = 10 WHERE a IN (SELECT b FROM t2 WHERE t1.a < 3)
--let $select = SELECT * FROM t1            WHERE a IN (SELECT b FROM t2 WHERE t1.a < 3)
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #7
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (b INT);
INSERT INTO t2 VALUES (1), (2), (3);
--let $query =  UPDATE        t1, t2 SET a = 10 WHERE a IN (SELECT b FROM t2 WHERE t2.b < 3)
--let $select = SELECT * FROM t1, t2            WHERE a IN (SELECT b FROM t2 WHERE t2.b < 3)
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #8
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (b INT);
INSERT INTO t2 VALUES (1), (2), (3);
--let $query =  UPDATE        t1 t11, (SELECT * FROM t2) t12 SET t11.a = t11.a + 10
--let $select = SELECT * FROM t1 t11, (SELECT * FROM t2) t12
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #9
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (b INT);
INSERT INTO t2 VALUES (1), (2), (3);
--let $query =  UPDATE        t1 t11, (SELECT 1 FROM DUAL) t12 SET t11.a = t11.a + 10
--let $select = SELECT * FROM t1 t11, (SELECT 1 FROM DUAL) t12
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #10
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (b INT);
INSERT INTO t2 VALUES (1), (2), (3);
--let $query =  UPDATE        t1 t11, (SELECT * FROM t2) t12 SET t11.a = 10 WHERE t11.a > 1
--let $select = SELECT * FROM t1 t11, (SELECT * FROM t2) t12                WHERE t11.a > 1
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #11
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
--let $query =  DELETE   FROM t1 WHERE a > 1 LIMIT 1
--let $select = SELECT * FROM t1 WHERE a > 1 LIMIT 1
--source include/explain_utils.inc
DROP TABLE t1;

--echo #12
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
--let $query =  DELETE   FROM t1 WHERE 0
--let $select = SELECT * FROM t1 WHERE 0
--source include/explain_utils.inc
DROP TABLE t1;

--echo #13
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
--let $query =  DELETE   FROM t1 USING t1 WHERE 0
--let $select = SELECT * FROM t1          WHERE 0
--source include/explain_utils.inc
DROP TABLE t1;

--echo #14
CREATE TABLE t1 (a INT, b INT, UNIQUE KEY (a), KEY (b));
INSERT INTO t1 VALUES (3, 3), (7, 7);
--let $query =  DELETE   FROM t1 WHERE a = 3
--let $select = SELECT * FROM t1 WHERE a = 3
--source include/explain_utils.inc
DROP TABLE t1;

--echo #15
CREATE TABLE t1 (a INT, b INT, UNIQUE KEY (a), KEY (b));
INSERT INTO t1 VALUES (3, 3), (7, 7);
--let $query =  DELETE   FROM t1 WHERE a < 3
--let $select = SELECT * FROM t1 WHERE a < 3
--source include/explain_utils.inc
DROP TABLE t1;

--echo #16
CREATE TABLE t1 ( a int PRIMARY KEY );
--let $query =  DELETE   FROM t1 WHERE t1.a > 0 ORDER BY t1.a
--let $select = SELECT * FROM t1 WHERE t1.a > 0 ORDER BY t1.a
--source include/explain_utils.inc
INSERT INTO t1 VALUES (1), (2), (3);
--let $query =  DELETE   FROM t1 WHERE t1.a > 0 ORDER BY t1.a
--let $select = SELECT * FROM t1 WHERE t1.a > 0 ORDER BY t1.a
--source include/explain_utils.inc
DROP TABLE t1;

--echo #17
CREATE TABLE t1(a INT PRIMARY KEY);
INSERT INTO t1 VALUES (4),(3),(1),(2);
--let $query =  DELETE   FROM t1 WHERE (@a:= a) ORDER BY a LIMIT 1
--let $select = SELECT * FROM t1 WHERE (@a:= a) ORDER BY a LIMIT 1
--source include/explain_utils.inc
DROP TABLE t1;

--echo #18
CREATE TABLE t1 (a DATE, b TIME, c INT, KEY c(c), KEY b(b), KEY a(a));
INSERT INTO t1 VALUES (), (), (), (), (), (), (), (), (), ();
UPDATE t1 SET a = c, b = c;
--let $query =  DELETE   FROM t1 ORDER BY a ASC, b ASC LIMIT 1
--let $select = SELECT * FROM t1 ORDER BY a ASC, b ASC LIMIT 1
--source include/explain_utils.inc
DROP TABLE t1;

--echo #19
CREATE TABLE t1 (a1 INT NOT NULL, b1 INT NOT NULL);
CREATE TABLE t2 (a2 INT NOT NULL, b2 INT NOT NULL, PRIMARY KEY (a2,b2));
CREATE TABLE t3 (a3 INT NOT NULL, b3 INT NOT NULL, PRIMARY KEY (a3,b3));
INSERT INTO t1 VALUES (1,1), (2,1), (1,3);
INSERT INTO t2 VALUES (1,1), (2,2), (3,3);
INSERT INTO t3 VALUES (1,1), (2,1), (1,3);
# Need to run ANALYZE to get correct index statistics
--disable_query_log
--disable_result_log
ANALYZE TABLE t1, t2, t3;
--enable_result_log
--enable_query_log
--let $query =  DELETE t1,t2,t3 FROM t1,t2,t3 WHERE a1=a2 AND b2=a3 AND b1=b3
--let $select = SELECT *        FROM t1,t2,t3 WHERE a1=a2 AND b2=a3 AND b1=b3
--source include/explain_utils.inc
DROP TABLE t1, t2, t3;

--echo #20
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (a INT);
INSERT INTO t2 VALUES (1), (2), (3);
--let $query =  UPDATE t1 SET a = 10 WHERE a IN (SELECT a FROM t2)
--let $select = SELECT * FROM t1     WHERE a IN (SELECT a FROM t2)
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #21
CREATE TABLE t1 (a1 INT);
INSERT INTO t1 VALUES (1), (2), (3), (4), (5);
CREATE TABLE t2 (a2 VARCHAR(10)) charset utf8mb4;
INSERT INTO t2 VALUES (1), (2), (3), (4), (5);
SET @save_optimizer_switch= @@optimizer_switch;
--disable_query_log
if (`select locate('semijoin', @@optimizer_switch) > 0`)
{
  SET @@optimizer_switch= 'semijoin=off';
}
--enable_query_log
--let $query =  DELETE   FROM t1 WHERE a1 IN (SELECT a2 FROM t2 WHERE a2 > 2)
--let $select = SELECT * FROM t1 WHERE a1 IN (SELECT a2 FROM t2 WHERE a2 > 2)
--source include/explain_utils.inc
SET @@optimizer_switch= @save_optimizer_switch;
TRUNCATE t1;
INSERT INTO t1 VALUES (1), (2), (3), (4), (5);
--let $query =  DELETE   FROM t1 WHERE a1 IN (SELECT a2 FROM t2 WHERE a2 > 2)
--let $select = SELECT * FROM t1 WHERE a1 IN (SELECT a2 FROM t2 WHERE a2 > 2)
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #22
CREATE TABLE t1 (i INT, j INT);
INSERT INTO t1 VALUES (1, 1), (2, 2), (3, 3), (4, 4), (5, 5);
--let $query =  UPDATE t1 SET i = 10
--let $select = SELECT * FROM t1
--source include/explain_utils.inc
DROP TABLE t1;

--echo #23
CREATE TABLE t1 (i INT, j INT);
INSERT INTO t1 VALUES (1, 1), (2, 2), (3, 3), (4, 4), (5, 5);
--let $query =  DELETE FROM t1
--let $select = SELECT * FROM t1
# Disabling binary logging temporarily as optimized delete is not used when log-bin is enabled in row mode
--disable_query_log
SET @save_log_bin= @@sql_log_bin;
SET SQL_LOG_BIN=0;
--enable_query_log
--source include/explain_utils.inc
--disable_query_log
SET SQL_LOG_BIN=@save_log_bin;
--enable_query_log
DROP TABLE t1;

--echo #24
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                      (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                      (30),(31),(32),(33),(34),(35);
CREATE TABLE t2(a INT, b INT, c INT, d INT, INDEX(a, b, c));
INSERT INTO t2 (a, b, c) SELECT i, i, i FROM t1;
INSERT INTO t2 (a, b, c) SELECT t1.i, t1.i, t1.i FROM t1, t1 x1, t1 x2;
--let $query =  DELETE   FROM t2 WHERE b = 10 ORDER BY a, c LIMIT 5
--let $select = SELECT * FROM t2 WHERE b = 10 ORDER BY a, c LIMIT 5
--let $no_rows = 1
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #25
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (i INT);
--let $query =  INSERT INTO t2 SELECT * FROM t1
--let $select =                SELECT * FROM t1
--source include/explain_utils.inc
--let $query =  INSERT INTO t2 SELECT * FROM t1 JOIN t1 AS tt USING(i)
--let $select =                SELECT * FROM t1 JOIN t1 AS tt USING(i)
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #26
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (i INT);
--let $query =  REPLACE INTO t2 SELECT * FROM t1
--let $select =                 SELECT * FROM t1
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #27
CREATE TABLE t1 (i INT);
--let $query = INSERT INTO t1 SET i = 10
--source include/explain_utils.inc
DROP TABLE t1;

--echo #28
CREATE TABLE t1 (i INT);
--let $query = REPLACE INTO t1 SET i = 10
--source include/explain_utils.inc
DROP TABLE t1;

--echo #29
CREATE TABLE t1 (a INT, i INT PRIMARY KEY);
INSERT INTO t1 (i) VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                          (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                          (30),(31),(32),(33),(34),(35);
--let $query =  DELETE   FROM t1 WHERE i > 10 AND i <= 18 ORDER BY i LIMIT 1
--let $select = SELECT * FROM t1 WHERE i > 10 AND i <= 18 ORDER BY i LIMIT 1
--let $no_rows = 1
--source include/explain_utils.inc
--let $no_rows = 0
DROP TABLE t1;

--echo #30
CREATE TABLE t1(a INT, i CHAR(2), INDEX(i(1))) charset utf8mb4;
INSERT INTO t1 (i) VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                          (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                          (30),(31),(32),(33),(34),(35);
--let $query =  DELETE   FROM t1 WHERE i > 10 AND i <= 18 ORDER BY i LIMIT 5
--let $select = SELECT * FROM t1 WHERE i > 10 AND i <= 18 ORDER BY i LIMIT 5
--source include/explain_utils.inc
DROP TABLE t1;

--echo #31
CREATE TABLE t1 (i INT);
INSERT INTO t1 (i) VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                          (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                          (30),(31),(32),(33),(34),(35);
CREATE TABLE t2(a INT, b INT, c INT, d INT, INDEX(a, b, c));
INSERT INTO t2 (a, b, c) SELECT i, i, i FROM t1;
--let $query =  DELETE   FROM t2 WHERE b = 10 ORDER BY a, c LIMIT 5
--let $select = SELECT * FROM t2 WHERE b = 10 ORDER BY a, c LIMIT 5
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #32
CREATE TABLE t1 (i INT);
INSERT INTO t1 (i) VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                          (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                          (30),(31),(32),(33),(34),(35);
CREATE TABLE t2(a INT, b INT, c INT, d INT, INDEX(a, b, c));
INSERT INTO t2 (a, b, c) SELECT i, i, i FROM t1;
INSERT INTO t2 (a, b, c) SELECT t1.i, t1.i, t1.i FROM t1, t1 x1, t1 x2;
--let $query =  DELETE   FROM t2 WHERE b = 10 ORDER BY a, c LIMIT 5
--let $select = SELECT * FROM t2 WHERE b = 10 ORDER BY a, c LIMIT 5
--let $no_rows = 1
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #33
CREATE TABLE t1 (i INT);
INSERT INTO t1 (i) VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                          (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                          (30),(31),(32),(33),(34),(35);
CREATE TABLE t2 (a CHAR(2), b CHAR(2), c CHAR(2), d CHAR(2), INDEX (a,b(1),c))
charset utf8mb4;
INSERT INTO t2 SELECT i, i, i, i FROM t1;
--let $query =  DELETE   FROM t2 WHERE b = 10 ORDER BY a, c LIMIT 5
--let $select = SELECT * FROM t2 WHERE b = 10 ORDER BY a, c LIMIT 5
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #34
CREATE TABLE t1 (i INT);
INSERT INTO t1 (i) VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                          (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                          (30),(31),(32),(33),(34),(35);
CREATE TABLE t2 (a CHAR(2), b CHAR(2), c CHAR(2), d CHAR(2), INDEX (a,b,c))
charset utf8mb4  ENGINE=HEAP;
INSERT INTO t2 SELECT i, i, i, i FROM t1;
--let $query =  DELETE   FROM t2 WHERE b = 10 ORDER BY a, c LIMIT 5
--let $select = SELECT * FROM t2 WHERE b = 10 ORDER BY a, c LIMIT 5
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #35
CREATE TABLE t1 (i INT);
INSERT INTO t1 (i) VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                          (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                          (30),(31),(32),(33),(34),(35),(36),(37),(38),(39),
                          (40),(41),(42),(43),(44);
CREATE TABLE t2 (i INT, key1 INT, key2 INT, INDEX (key1), INDEX (key2));
INSERT INTO t2 (key1, key2) SELECT i, i FROM t1;
--let $query =  DELETE   FROM t2 WHERE key1 < 13 or key2 < 14 ORDER BY key1
--let $select = SELECT * FROM t2 WHERE key1 < 13 or key2 < 14 ORDER BY key1
--let $no_rows = 1
--source include/explain_utils.inc
--let $no_rows = 0
DROP TABLE t1, t2;

--echo #36
CREATE TABLE t1 (i INT);
INSERT INTO t1 (i) VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                          (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                          (30),(31),(32),(33),(34),(35);
CREATE TABLE t2(a INT, i INT PRIMARY KEY);
INSERT INTO t2 (i) SELECT i FROM t1;
--let $query =  DELETE   FROM t2 WHERE i > 10 AND i <= 18 ORDER BY i DESC LIMIT 1
--let $select = SELECT * FROM t2 WHERE i > 10 AND i <= 18 ORDER BY i DESC LIMIT 1
--let $no_rows = 1
--source include/explain_utils.inc
--let $no_rows = 0
DROP TABLE t1, t2;

--echo #37
CREATE TABLE t1 (i INT);
INSERT INTO t1 (i) VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                          (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                          (30),(31),(32),(33),(34),(35);
CREATE TABLE t2 (a CHAR(2), b CHAR(2), c CHAR(2), INDEX (a, b)) charset utf8mb4;
INSERT INTO t2 SELECT i, i, i FROM t1;
--let $query =  DELETE   FROM t2 ORDER BY a, b DESC LIMIT 5
--let $select = SELECT * FROM t2 ORDER BY a, b DESC LIMIT 5
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #38
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                      (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                      (30),(31),(32),(33),(34),(35);
CREATE TABLE t2 (a CHAR(2), b CHAR(2), c INT, INDEX (a, b)) charset utf8mb4;
INSERT INTO t2 (a, b) SELECT i, i FROM t1;
INSERT INTO t2 (a, b) SELECT t1.i, t1.i FROM t1, t1 x1, t1 x2;
--let $query =  DELETE   FROM t2 ORDER BY a DESC, b DESC LIMIT 5
--let $select = SELECT * FROM t2 ORDER BY a DESC, b DESC LIMIT 5
--let $no_rows = 1
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #39
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                      (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                      (30),(31),(32),(33),(34),(35);
CREATE TABLE t2(a INT, i INT PRIMARY KEY);
INSERT INTO t2 (i) SELECT i FROM t1;
--let $query =  UPDATE t2 SET a = 10 WHERE i > 10 AND i <= 18 ORDER BY i LIMIT 1
--let $select = SELECT * FROM  t2    WHERE i > 10 AND i <= 18 ORDER BY i LIMIT 1
--let $no_rows = 1
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #40
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                      (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                      (30),(31),(32),(33),(34),(35);
CREATE TABLE t2(a INT, i CHAR(2), INDEX(i(1))) charset utf8mb4;
INSERT INTO t2 (i) SELECT i FROM t1;
--let $query =  UPDATE t2 SET a = 10 WHERE i > 10 AND i <= 18 ORDER BY i LIMIT 5
--let $select = SELECT * FROM t2     WHERE i > 10 AND i <= 18 ORDER BY i LIMIT 5
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #41
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                      (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                      (30),(31),(32),(33),(34),(35);
CREATE TABLE t2(a INT, b INT, c INT, d INT, INDEX(a, b, c));
INSERT INTO t2 (a, b, c) SELECT i, i, i FROM t1;
--let $query =  UPDATE t2 SET d = 10 WHERE b = 10 ORDER BY a, c LIMIT 5
--let $select = SELECT * FROM t2     WHERE b = 10 ORDER BY a, c LIMIT 5
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #42
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                      (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                      (30),(31),(32),(33),(34),(35);
CREATE TABLE t2(a INT, b INT, c INT, d INT, INDEX(a, b, c));
INSERT INTO t2 (a, b, c) SELECT i, i, i FROM t1;
INSERT INTO t2 (a, b, c) SELECT t1.i, t1.i, t1.i FROM t1, t1 x1, t1 x2;
--let $query =  UPDATE t2 SET d = 10 WHERE b = 10 ORDER BY a, c LIMIT 5
--let $select = SELECT * FROM t2     WHERE b = 10 ORDER BY a, c LIMIT 5
--let $no_rows = 1
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #43
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                      (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                      (30),(31),(32),(33),(34),(35);
CREATE TABLE t2 (a CHAR(2), b CHAR(2), c CHAR(2), d CHAR(2), INDEX (a,b(1),c))
charset utf8mb4;
INSERT INTO t2 SELECT i, i, i, i FROM t1;
--let $query =  UPDATE t2 SET d = 10 WHERE b = 10 ORDER BY a, c LIMIT 5
--let $select = SELECT * FROM t2     WHERE b = 10 ORDER BY a, c LIMIT 5
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #44
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                      (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                      (30),(31),(32),(33),(34),(35);
CREATE TABLE t2 (a CHAR(2), b CHAR(2), c CHAR(2), d CHAR(2), INDEX (a,b,c))
charset utf8mb4  ENGINE=HEAP;
INSERT INTO t2 SELECT i, i, i, i FROM t1;
--let $query =  UPDATE t2 SET d = 10 WHERE b = 10 ORDER BY a, c LIMIT 5
--let $select = SELECT * FROM t2     WHERE b = 10 ORDER BY a, c LIMIT 5
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #45
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                      (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                      (30),(31),(32),(33),(34),(35),(36),(37),(38),(39),
                      (40),(41),(42),(43),(44);
CREATE TABLE t2 (i INT, key1 INT, key2 INT, INDEX (key1), INDEX (key2));
INSERT INTO t2 (key1, key2) SELECT i, i FROM t1;
--let $query =  UPDATE t2 SET i = 123 WHERE key1 < 13 or key2 < 14 ORDER BY key1
--let $select = SELECT * FROM t2      WHERE key1 < 13 or key2 < 14 ORDER BY key1
--let $no_rows = 1
--source include/explain_utils.inc
--let $no_rows = 0
DROP TABLE t1, t2;

--echo #46
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                      (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                      (30),(31),(32),(33),(34),(35);
CREATE TABLE t2(a INT, i INT PRIMARY KEY);
INSERT INTO t2 (i) SELECT i FROM t1;
--let $query =  UPDATE t2 SET a = 10 WHERE i > 10 AND i <= 18 ORDER BY i DESC LIMIT 1
--let $select = SELECT * FROM t2     WHERE i > 10 AND i <= 18 ORDER BY i DESC LIMIT 1
--let $no_rows = 1
--source include/explain_utils.inc
--let $no_rows = 0
DROP TABLE t1, t2;

--echo #47
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                      (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                      (30),(31),(32),(33),(34),(35);
CREATE TABLE t2 (a CHAR(2), b CHAR(2), c CHAR(2), INDEX (a, b)) charset utf8mb4;
INSERT INTO t2 SELECT i, i, i FROM t1;
--let $query =  UPDATE t2 SET c = 10 ORDER BY a, b DESC LIMIT 5
--let $select = SELECT * FROM t2     ORDER BY a, b DESC LIMIT 5
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #48
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (10),(11),(12),(13),(14),(15),(16),(17),(18),(19),
                      (20),(21),(22),(23),(24),(25),(26),(27),(28),(29),
                      (30),(31),(32),(33),(34),(35);
CREATE TABLE t2 (a CHAR(2), b CHAR(2), c INT, INDEX (a, b)) charset utf8mb4;
INSERT INTO t2 (a, b) SELECT i, i FROM t1;
INSERT INTO t2 (a, b) SELECT t1.i, t1.i FROM t1, t1 x1, t1 x2;
--let $query =  UPDATE t2 SET c = 10 ORDER BY a DESC, b DESC LIMIT 5
--let $select = SELECT * FROM t2     ORDER BY a DESC, b DESC LIMIT 5
--let $no_rows = 1
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #49
CREATE TABLE t1 (
  pk INT NOT NULL AUTO_INCREMENT,
  c1_idx CHAR(1) DEFAULT 'y',
  c2 INT,
  PRIMARY KEY (pk),
  INDEX c1_idx (c1_idx)
) charset utf8mb4;
INSERT INTO t1 VALUES (1,'y',1), (2,'n',2), (3,'y',3), (4,'n',4);
--let $query =  UPDATE t1 SET c2 = 0 WHERE c1_idx = 'y' ORDER BY pk DESC LIMIT 2
--let $select = SELECT * FROM t1     WHERE c1_idx = 'y' ORDER BY pk DESC LIMIT 2
--source include/explain_utils.inc
--let $query =  DELETE   FROM t1 WHERE c1_idx = 'y' ORDER BY pk DESC LIMIT 2
--let $select = SELECT * FROM t1 WHERE c1_idx = 'y' ORDER BY pk DESC LIMIT 2
--source include/explain_utils.inc
DROP TABLE t1;

--echo #50
CREATE TABLE t1 (a INT AUTO_INCREMENT PRIMARY KEY);
INSERT INTO t1 VALUES (),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(),(); 
--let $query =  UPDATE t1 SET a=a+10 WHERE a > 34
--let $select = SELECT * FROM t1     WHERE a > 34
--source include/explain_utils.inc
DROP TABLE t1;

--echo #51
CREATE TABLE t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE t2 (c1 INT, c2 INT);
INSERT INTO t1 VALUES (1, 1, 10), (2, 2, 20);
--let $query =  UPDATE        t1 LEFT JOIN t2 ON t1.c1 = t2.c1 SET t2.c2 = 10
--let $select = SELECT * FROM t1 LEFT JOIN t2 ON t1.c1 = t2.c1
--source include/explain_utils.inc
--let $query =  UPDATE        t1 LEFT JOIN t2 ON t1.c1 = t2.c1 SET t2.c2 = 10 WHERE t1.c3 = 10
--let $select = SELECT * FROM t1 LEFT JOIN t2 ON t1.c1 = t2.c1                WHERE t1.c3 = 10
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #52
CREATE TABLE t1(f1 INT, f2 INT);
CREATE TABLE t2(f3 INT, f4 INT);
CREATE INDEX IDX ON t2(f3);
INSERT INTO t1 VALUES(1,0),(2,0);
INSERT INTO t2 VALUES(1,1),(2,2);
# Need to run ANALYZE to get correct index statistics
-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1,t2;
-- enable_result_log
-- enable_query_log
--let $query =  UPDATE t1 SET t1.f2=(SELECT MAX(t2.f4) FROM t2 WHERE t2.f3=t1.f1)
--let $select = SELECT              (SELECT MAX(t2.f4) FROM t2 WHERE t2.f3=t1.f1) FROM t1
--source include/explain_utils.inc
DROP TABLE t1, t2;

--echo #55
CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES (1);
SET @a = NULL;
EXPLAIN DELETE FROM t1 WHERE (@a:= a);
if (`SELECT @a IS NOT NULL`) {
  die Unexpectedly modified user variable;
}
DROP TABLE t1;

--echo #56
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
--error ER_BAD_FIELD_ERROR
                 DELETE FROM t1 USING t1 WHERE uknown_column = 12345;
--error ER_BAD_FIELD_ERROR
EXPLAIN DELETE FROM t1 USING t1 WHERE uknown_column = 12345;
DROP TABLE t1;

--echo #57
CREATE TABLE t1(f1 INT);
--error ER_BAD_FIELD_ERROR
EXPLAIN UPDATE t1 SET f2=1 ORDER BY f2;
--error ER_BAD_FIELD_ERROR
UPDATE t1 SET f2=1 ORDER BY f2;
DROP TABLE t1;

--echo #59
CREATE TABLE t1 ( a INT, KEY( a ) );
INSERT INTO t1 VALUES (0), (1);
CREATE VIEW v1 AS SELECT t11.a, t12.a AS b FROM t1 t11, t1 t12;
SET SESSION sql_safe_updates = 1;
--error ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE
EXPLAIN UPDATE IGNORE v1 SET a = 1;
--error ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE
UPDATE IGNORE v1 SET a = 1;
SET SESSION sql_safe_updates = DEFAULT;
DROP TABLE t1;
DROP VIEW v1;

--echo #62
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (0), (1);
CREATE VIEW v1 AS SELECT t11.a, t12.a AS b FROM t1 t11, t1 t12;
--let $query =  UPDATE        v1 SET a = 1 WHERE a > 0
--let $select = SELECT * FROM v1           WHERE a > 0
--source include/explain_utils.inc
--let $query =  UPDATE        t1, v1 SET v1.a = 1 WHERE t1.a = v1.a
--let $select = SELECT * FROM t1, v1              WHERE t1.a = v1.a
--source include/explain_utils.inc
DROP TABLE t1;
DROP VIEW v1;

--echo #63
CREATE TABLE t1 (a INT, PRIMARY KEY(a));
INSERT INTO t1 VALUES (1), (2), (3), (4), (5);
CREATE VIEW v1 (a) AS SELECT a FROM t1;
--let $query =  DELETE   FROM v1 WHERE a < 4
--let $select = SELECT * FROM v1 WHERE a < 4
--let $no_rows = 1
--source include/explain_utils.inc
--let $no_rows = 0
DROP TABLE t1;
DROP VIEW v1;

--echo #64
CREATE TABLE t1 (a INT, b INT, PRIMARY KEY(a));
INSERT INTO t1 VALUES (1,2), (2,3), (3,4), (4,5), (5,10);
CREATE TABLE t2 (x INT);
INSERT INTO t2 VALUES (1), (2), (3), (4);
CREATE VIEW v1 (a,c) AS SELECT a, b+1 FROM t1;
--let $query =  DELETE v1 FROM t2, v1 WHERE t2.x = v1.a
--let $select = SELECT *  FROM t2, v1 WHERE t2.x = v1.a
--source include/explain_utils.inc
DROP TABLE t1,t2;
DROP VIEW v1;

--echo #65
CREATE TABLE t1 (a INT, b INT, PRIMARY KEY(a));
INSERT INTO t1 VALUES (1,2), (2,3), (3,4), (4,5), (5,10);
CREATE TABLE t2 (x INT);
INSERT INTO t2 VALUES (1), (2), (3), (4);
CREATE VIEW v1 (a,c) AS SELECT a, b+1 FROM t1;
--let $query =  DELETE v1 FROM t2, v1 WHERE t2.x = v1.a
--let $select = SELECT *  FROM t2, v1 WHERE t2.x = v1.a
--source include/explain_utils.inc
DROP TABLE t1,t2;
DROP VIEW v1;

--echo #66
CREATE TABLE t1 (a INT);
CREATE VIEW v1 (x) AS SELECT a FROM t1;
--let $query =  INSERT INTO v1 VALUES (10)
--let $select = SELECT NULL
--source include/explain_utils.inc
DROP TABLE t1;
DROP VIEW v1;

--echo #67
CREATE TABLE t1 (a INT);
CREATE TABLE t2 (b INT);
INSERT INTO t2 VALUES (1), (2), (3);
CREATE VIEW v1 (x) AS SELECT b FROM t2;
--let $query =  INSERT INTO v1 SELECT * FROM t1
--let $select =                SELECT * FROM t1
--source include/explain_utils.inc
DROP TABLE t1, t2;
DROP VIEW v1;

--echo #69
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (b INT);
INSERT INTO t2 VALUES (1), (2), (3);
--let $query =  UPDATE        t1 SET a = 10 WHERE a IN (SELECT * FROM (SELECT b FROM t2 ORDER BY b LIMIT 2,2) x)
--let $select = SELECT * FROM t1            WHERE a IN (SELECT * FROM (SELECT b FROM t2 ORDER BY b LIMIT 2,2) x)
--source include/explain_utils.inc
--let $query =  UPDATE        t1, t2 SET a = 10 WHERE a IN (SELECT * FROM (SELECT b FROM t2 ORDER BY b LIMIT 2,2) x)
--let $select = SELECT * FROM t1, t2            WHERE a IN (SELECT * FROM (SELECT b FROM t2 ORDER BY b LIMIT 2,2) x)
--source include/explain_utils.inc
--let $query =  UPDATE        t1, (SELECT * FROM t2) y SET a = 10 WHERE a IN (SELECT * FROM (SELECT b FROM t2 ORDER BY b LIMIT 2,2) x)
--let $select = SELECT * FROM t1, (SELECT * FROM t2) y            WHERE a IN (SELECT * FROM (SELECT b FROM t2 ORDER BY b LIMIT 2,2) x)
--source include/explain_utils.inc
DROP TABLE t1,t2;

--echo #70
CREATE TABLE t1 (c1 INT KEY);
CREATE TABLE t2 (c2 INT);
CREATE TABLE t3 (c3 INT);
EXPLAIN UPDATE t3 SET c3 = (
  SELECT COUNT(d1.c1)
    FROM (
      SELECT a11.c1 FROM t1 AS a11 
        STRAIGHT_JOIN t2 AS a21 ON a21.c2 = a11.c1
        JOIN t1 AS a12 ON a12.c1 = a11.c1
    ) d1
); 

DROP TABLE t1, t2, t3;

--echo #71
#
# Bug: after EXPLAIN bulk INSERT...SELECT and bulk INSERT...SELECT
# to a # MyISAM table the SELECT query may fail with the
# "1030: Got error 124 from storage engine" error message.
#
CREATE TABLE t1 (c1 INT NOT NULL, c2 INT NOT NULL, INDEX i1(c1));
INSERT INTO t1 VALUES (1,0),(2,0),(3,0),(4,0),(5,0),(6,0),(7,0),(8,0);
--disable_query_log
let $1=7;
SET @d=8;
while ($1) {
  eval INSERT INTO t1 SELECT c1 + @d, c2 + @d FROM t1;
  eval SET @d = @d*2;
  dec $1;
}
--enable_query_log
CREATE TABLE t2 LIKE t1;

# replace "rows" column for InnoDB
--replace_column 10 X
EXPLAIN INSERT INTO t2 SELECT * FROM t1;
INSERT INTO t2 SELECT * FROM t1;
--disable_result_log
SELECT * FROM t1 LEFT JOIN t2 ON t1.c1 = t2.c1;
--enable_result_log

DROP TABLE t1, t2;

--echo #73

CREATE TABLE t1 (id INT);
CREATE TABLE t2 (id INT);
INSERT INTO t1 VALUES (1), (2);

EXPLAIN SELECT * FROM t1 LEFT JOIN t2 USING(id) GROUP BY t1.id;

DROP TABLE t1,t2;

--echo #74

CREATE TABLE t1(a INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1), (2), (3), (4), (5);

--echo # used key is modified & Using temporary

--let $query = UPDATE t1 SET a=a+1 WHERE a>10
--let $select = SELECT a t1 FROM t1 WHERE a>10
--source include/explain_utils.inc

--echo # used key is modified & Using filesort

--let $query = UPDATE t1 SET a=a+1 WHERE a>10 ORDER BY a+20
--let $select = SELECT a t1 FROM t1 WHERE a>10 ORDER BY a+20
--source include/explain_utils.inc

DROP TABLE t1;

--echo #
--echo # Bug #12949629: CLIENT LOSES CONNECTION AFTER EXECUTING A PROCEDURE WITH
--echo #                EXPLAIN UPDATE/DEL/INS
--echo #

CREATE TABLE t1 (i INT);
CREATE TABLE t2 (i INT);

--delimiter |
CREATE PROCEDURE p1() BEGIN EXPLAIN INSERT INTO t1 VALUES (1);END|
CREATE PROCEDURE p2() BEGIN         INSERT INTO t1 VALUES (1);END|
CREATE PROCEDURE p3() BEGIN EXPLAIN INSERT INTO t1 SELECT 1;END|
CREATE PROCEDURE p4() BEGIN         INSERT INTO t1 SELECT 1;END|
CREATE PROCEDURE p5() BEGIN EXPLAIN REPLACE INTO t1 VALUES (1);END|
CREATE PROCEDURE p6() BEGIN         REPLACE INTO t1 VALUES (1);END|
CREATE PROCEDURE p7() BEGIN EXPLAIN REPLACE INTO t1 SELECT 1;END|
CREATE PROCEDURE p8() BEGIN         REPLACE INTO t1 SELECT 1;END|
CREATE PROCEDURE p9() BEGIN EXPLAIN UPDATE t1 SET i = 10;END|
CREATE PROCEDURE p10() BEGIN         UPDATE t1 SET i = 10;END|
CREATE PROCEDURE p11() BEGIN EXPLAIN UPDATE t1,t2 SET t1.i = 10 WHERE t1.i = t2.i ;END|
CREATE PROCEDURE p12() BEGIN         UPDATE t1,t2 SET t1.i = 10 WHERE t1.i = t2.i ;END|
CREATE PROCEDURE p13() BEGIN
# Disabling binary logging temporarily as optimized delete is not used when log-bin is enabled in row mode
SET @save_log_bin= @@sql_log_bin;SET SQL_LOG_BIN=0;
EXPLAIN DELETE FROM t1;
SET SQL_LOG_BIN=@save_log_bin; END|
CREATE PROCEDURE p14() BEGIN         DELETE FROM t1;END|
CREATE PROCEDURE p15() BEGIN EXPLAIN DELETE FROM t1 USING t1;END|
CREATE PROCEDURE p16() BEGIN         DELETE FROM t1 USING t1;END|
--delimiter ;

let $i=16;
while($i) {
	eval CALL p$i();
	eval DROP PROCEDURE p$i;
	dec $i;
}

DROP TABLE t1, t2;

--echo #

-- disable_query_log
-- disable_result_log
SET GLOBAL innodb_stats_persistent=default;
-- enable_result_log
-- enable_query_log
