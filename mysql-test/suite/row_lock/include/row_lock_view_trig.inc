--disable_warnings
DROP TABLE IF EXISTS t1, t2;
DROP VIEW IF EXISTS v1;
--enable_warnings
SET autocommit=0;
# Create additional connections used through test
CONNECT (root1, localhost, root,,);
SET autocommit=0;
--echo connection default;
CONNECTION default;
eval CREATE TABLE t1 (k INT NOT NULL PRIMARY KEY, i INT, j INT, l INT) ENGINE=$engine;
INSERT INTO t1 VALUES (1,123,1,123);
INSERT INTO t1 VALUES (2,124,2,124);
INSERT INTO t1 VALUES (3,125,3,125);
INSERT INTO t1 VALUES (4,126,4,126);
eval $indext1;
eval CREATE TABLE t2 (k INT NOT NULL PRIMARY KEY, i INT, j INT, l INT) ENGINE=$engine;
INSERT INTO t2 VALUES (1,123,1,123);
INSERT INTO t2 VALUES (2,124,2,124);
INSERT INTO t2 VALUES (3,125,3,125);
INSERT INTO t2 VALUES (4,126,4,126);
eval $indext2;
CREATE VIEW v1 AS SELECT t1.i from t1;
DELIMITER |;

CREATE TRIGGER trig_t2 AFTER UPDATE ON t2
  FOR EACH ROW BEGIN
    UPDATE v1 SET i = NEW.i WHERE i = OLD.i;
  END;
|

DELIMITER ;|

COMMIT;
SELECT @@global.tx_isolation;
eval EXPLAIN $select;
eval $select;
--echo connection root1;
CONNECTION root1;
UPDATE t2 SET t2.i=225 WHERE t2.i=125;
SELECT * FROM v1 ORDER BY i;
SELECT * FROM t1 ORDER BY t1.k;
SELECT * FROM t2 ORDER BY t2.k;

--echo connection default;
CONNECTION default;
UPDATE t2 SET t2.i=223 WHERE t2.i=123;
SELECT * FROM v1 ORDER BY i;
SELECT * FROM t1 ORDER BY t1.k;
SELECT * FROM t2 ORDER BY t2.k;

--echo connection root1;
CONNECTION root1;
UPDATE t2 SET t2.i=226 WHERE t2.i=126;
SELECT * FROM v1 ORDER BY i;
SELECT * FROM t1 ORDER BY t1.k;
SELECT * FROM t2 ORDER BY t2.k;

--echo connection default;
CONNECTION default;
UPDATE t2 SET t2.i=224 WHERE t2.i=124;
SELECT * FROM v1 ORDER BY i;
SELECT * FROM t1 ORDER BY t1.k;
SELECT * FROM t2 ORDER BY t2.k;

--echo connection root1;
CONNECTION root1;
DELETE FROM t1 WHERE t1.i=226;
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
--echo connection default;
CONNECTION default;
DROP TABLE t1, t2;
DROP VIEW v1;

