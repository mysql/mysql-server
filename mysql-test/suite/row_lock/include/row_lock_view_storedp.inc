--disable_warnings
DROP TABLE IF EXISTS t1, t2;
DROP VIEW IF EXISTS v1;
DROP PROCEDURE IF EXISTS stp_t;
--enable_warnings
SET autocommit=0;
# Create additional connections used through test
CONNECT (root1, localhost, root,,);
SET autocommit=0;
CONNECT (root2, localhost, root,,);
SET autocommit=0;
--echo connection default;
CONNECTION default;
eval CREATE TABLE t1 (k INT NOT NULL PRIMARY KEY, i INT, j INT, l INT) ENGINE=$engine;
INSERT INTO t1 VALUES (1,123,1,123);
INSERT INTO t1 VALUES (2,124,2,124);
INSERT INTO t1 VALUES (3,125,3,125);
INSERT INTO t1 VALUES (4,126,4,126);
INSERT INTO t1 VALUES (5,127,5,127);
INSERT INTO t1 VALUES (6,128,6,128);
eval $indext1;
eval CREATE TABLE t2 (k INT NOT NULL PRIMARY KEY, i INT, j INT, l INT) ENGINE=$engine;
INSERT INTO t2 VALUES (1,123,1,123);
INSERT INTO t2 VALUES (2,124,2,124);
INSERT INTO t2 VALUES (3,125,3,125);
INSERT INTO t2 VALUES (4,126,4,126);
INSERT INTO t2 VALUES (5,127,5,127);
INSERT INTO t2 VALUES (6,128,6,128);
eval $indext2;
CREATE VIEW v1 AS SELECT t1.i from t1;
DELIMITER |;

CREATE PROCEDURE stp_t (IN p1 int, IN p2 int) MODIFIES SQL DATA
  BEGIN
    UPDATE t2 SET i = p2 WHERE i = p1;
    UPDATE v1 SET i = p2 WHERE i = p1;
    SELECT * FROM v1 ORDER BY i;
    SELECT * FROM t1 ORDER BY t1.k;
    SELECT * FROM t2 ORDER BY t2.k;
  END;
|

DELIMITER ;|

COMMIT;
SELECT @@global.tx_isolation;
eval EXPLAIN $select;
eval $select;
--echo connection root1;
CONNECTION root1;
CALL stp_t (125, 225);

--echo connection root2;
CONNECTION root2;
CALL stp_t (127, 227);

--echo connection default;
CONNECTION default;
CALL stp_t (123, 223);

--echo connection root1;
CONNECTION root1;
CALL stp_t (126, 226);

--echo connection root2;
CONNECTION root2;
CALL stp_t (128, 228);

--echo connection default;
CONNECTION default;
CALL stp_t (124, 224);

--echo connection root1;
CONNECTION root1;
DELETE FROM t1 WHERE t1.i=226;
SELECT * FROM t1 ORDER BY t1.k;
SELECT * FROM t2 ORDER BY t2.k;

--echo connection root2;
CONNECTION root2;
DELETE FROM t1 WHERE t1.i=228;
SELECT * FROM t1 ORDER BY t1.k;
SELECT * FROM t2 ORDER BY t2.k;

--echo connection default;
CONNECTION default;
DELETE FROM t1 WHERE t1.i=224;
SELECT * FROM t1 ORDER BY t1.k;
SELECT * FROM t2 ORDER BY t2.k;
COMMIT;

--echo connection root1;
CONNECTION root1;
ROLLBACK;

--echo connection root1;
CONNECTION root1;
COMMIT;

--echo connection default;
CONNECTION default;
SELECT * FROM v1 ORDER BY i;
SELECT * FROM t1 ORDER BY t1.k;
SELECT * FROM t2 ORDER BY t2.k;

--echo connection root1;
CONNECTION root1;
SELECT * FROM v1 ORDER BY i;
SELECT * FROM t1 ORDER BY t1.k;
SELECT * FROM t2 ORDER BY t2.k;
DISCONNECT root1;

--echo connection root2;
CONNECTION root2;
SELECT * FROM v1 ORDER BY i;
SELECT * FROM t1 ORDER BY t1.k;
SELECT * FROM t2 ORDER BY t2.k;
DISCONNECT root2;

--echo connection default;
CONNECTION default;
--disable_warnings
DROP VIEW v1;
DROP PROCEDURE stp_t;
DROP TABLE t1, t2;
--enable_warnings
