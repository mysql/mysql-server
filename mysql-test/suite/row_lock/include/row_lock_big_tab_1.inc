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
eval $indext1;
eval CREATE TABLE t2 (k INT NOT NULL PRIMARY KEY, i INT, j INT, l INT) ENGINE=$engine;
eval $indext2;
DELIMITER |;
CREATE PROCEDURE fill_t1 (IN upb int)
BEGIN
DECLARE cnt int DEFAULT 0;
WHILE cnt < upb DO
      INSERT INTO t1 VALUES (cnt, cnt+100, cnt, cnt+100);
      SET cnt= cnt+1;
END WHILE; 
END;
|
CREATE FUNCTION half_t1() RETURNS int
BEGIN
    DECLARE res int DEFAULT 0;
    SELECT MOD(k,2) INTO res FROM t1;
    RETURN  res;
END;
|
CREATE PROCEDURE fill_t2 (IN upb int)
BEGIN
DECLARE cnt int DEFAULT 0;
WHILE cnt < upb DO
      INSERT INTO t2 VALUES (cnt, cnt+100, cnt, cnt+100);
      SET cnt= cnt+1;
END WHILE; 
END;
|
CREATE FUNCTION half_t2() RETURNS int
BEGIN
    DECLARE res int DEFAULT 0;
    SELECT MOD(k,2) INTO res FROM t2;
    RETURN  res;
END;
|
DELIMITER ;|
eval CALL fill_t1 ($nbrows);
eval CALL fill_t2 ($nbrows);
COMMIT;
SELECT @@global.tx_isolation;
# With the two separate selects (without join) the differs from
# that select with join.

# Both transaction are able to update the tables
eval EXPLAIN $select;
eval $select;

--echo connection root1;
CONNECTION root1;
SELECT t1.i,t2.i FROM t1,t2 WHERE t1.k % 2= 1 AND t1.k = t2.k LOCK IN SHARE MODE;
UPDATE t1,t2 SET t1.i=1111,t2.i=2222 WHERE t1.k % 2 = 1 AND t1.k = t2.k;
SELECT * FROM t1 WHERE k < 20 ORDER BY t1.k;
SELECT * FROM t2 WHERE k < 20 ORDER BY t2.k;

--echo connection default;
CONNECTION default;
UPDATE t1,t2 SET t1.i=3333,t2.i=4444 WHERE t1.k % 2 = 0 AND t1.k = t2.k;
SELECT * FROM t1 WHERE k < 20 ORDER BY t1.k;
SELECT * FROM t2 WHERE k < 20 ORDER BY t2.k;

COMMIT;

--echo connection root1;
CONNECTION root1;
ROLLBACK;

--echo connection default;
CONNECTION default;
SELECT * FROM t1 WHERE k < 40 ORDER BY t1.k;
SELECT * FROM t2 WHERE k < 40 ORDER BY t2.k;

--echo connection root1;
CONNECTION root1;
SELECT * FROM t1 WHERE k < 40 ORDER BY t1.k;
SELECT * FROM t2 WHERE k < 40 ORDER BY t2.k;
DISCONNECT root1;
--echo connection default;
CONNECTION default;
DROP TABLE t1, t2;


