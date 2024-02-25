#
# Bug#24289 Status Variable "Questions" gets wrong values with Stored Routines
#
FLUSH STATUS;
CREATE FUNCTION testQuestion() RETURNS INTEGER
BEGIN
DECLARE foo INTEGER;
DECLARE bar INTEGER;
SET foo=1;
SET bar=2;
RETURN foo;
END $$
CREATE PROCEDURE testQuestion2()
BEGIN
SELECT 1;
END $$
DROP TABLE IF EXISTS t1,t2;
CREATE TABLE t1 (c1 INT);
CREATE TABLE t2 (c1 INT);
CREATE EVENT ev1 ON SCHEDULE EVERY 1 SECOND
DO INSERT INTO t1 VALUES(1);
Assert Questions == 7
SHOW STATUS LIKE 'Questions';
Variable_name	Value
Questions	7
SELECT testQuestion();
testQuestion()
1
Assert Questions == 9
SHOW STATUS LIKE 'Questions';
Variable_name	Value
Questions	9
CALL testQuestion2();
1
1
Assert Questions == 11
SHOW STATUS LIKE 'Questions';
Variable_name	Value
Questions	11
SELECT 1;
1
1
Assert Questions == 13
SHOW STATUS LIKE 'Questions';
Variable_name	Value
Questions	13
SELECT 1;
1
1
Assert Questions == 14
SHOW STATUS LIKE 'Questions';
Variable_name	Value
Questions	14
CREATE TRIGGER trigg1 AFTER INSERT ON t1
FOR EACH ROW BEGIN
INSERT INTO t2 VALUES (1);
END;
$$
Assert Questions == 16
SHOW STATUS LIKE 'Questions';
Variable_name	Value
Questions	16
INSERT INTO t1 VALUES (1);
Assert Questions == 18
SHOW STATUS LIKE 'Questions';
Variable_name	Value
Questions	18
DROP PROCEDURE testQuestion2;
DROP TRIGGER trigg1;
DROP FUNCTION testQuestion;
DROP EVENT ev1;
DROP TABLE t1,t2;
End of 6.0 tests
