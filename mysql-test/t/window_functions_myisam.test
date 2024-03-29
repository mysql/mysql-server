--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo
--echo Bug with constant table: MYISAM makes ot a constant table when it has one row
--echo
CREATE TABLE ot (i int) ENGINE=MYISAM;
CREATE FUNCTION f(a INTEGER) RETURNS INTEGER DETERMINISTIC RETURN a*a;

--echo Empty table did work before
SELECT ROW_NUMBER() OVER () , BIT_AND(i) FROM ot WHERE f(2)<2;
SELECT ROW_NUMBER() OVER () AS RN, BIT_AND(i) AS x FROM ot WHERE f(2) < 2 HAVING x < 2;
SELECT SUM(BIT_AND(i)) OVER (ORDER BY BIT_AND(i)) , BIT_AND(i) FROM ot WHERE f(2)<2;
INSERT INTO ot VALUES (1);

--echo One row in table: used to be wrong before fix:
SELECT ROW_NUMBER() OVER () , BIT_AND(i) FROM ot WHERE f(2)<2;
SELECT ROW_NUMBER() OVER () AS RN, BIT_AND(i) AS x FROM ot WHERE f(2) < 2 HAVING x < 2;
SELECT SUM(BIT_AND(i)) OVER (ORDER BY BIT_AND(i)) , BIT_AND(i) FROM ot WHERE f(2)<2;
INSERT INTO ot VALUES (1);

--echo Two rows in table did work before
SELECT ROW_NUMBER() OVER () , BIT_AND(i) FROM ot WHERE f(2)<2;
SELECT ROW_NUMBER() OVER () AS RN, BIT_AND(i) AS x FROM ot WHERE f(2) < 2 HAVING x < 2;
SELECT SUM(BIT_AND(i)) OVER (ORDER BY BIT_AND(i)) , BIT_AND(i) FROM ot WHERE f(2)<2;

DROP TABLE ot;
DROP FUNCTION f;
