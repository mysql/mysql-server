CREATE TABLE t1 (f1 int);
INSERT INTO t1 VALUES (1);
CREATE TABLE t2 AS SELECT * FROM t1;
CREATE TABLE t3 AS SELECT * FROM t1;
CREATE VIEW v1 AS SELECT * FROM t3;
Case 1: IS query under 'FLUSH TABLES' command.
FLUSH TABLES WITH READ LOCK;
SELECT COUNT(*) FROM information_schema.columns
WHERE table_name='db';
COUNT(*)
22
UNLOCK TABLES;
Case 2: Simple IS query under 'LOCK TABLE' command.
LOCK TABLE t1 READ, v1 READ;
SELECT COUNT(*) FROM t1;
COUNT(*)
1
SELECT COUNT(*) FROM v1;
COUNT(*)
1
Test that IS view will be opened without explicit LOCK TABLE on them.
SELECT COUNT(*) FROM information_schema.columns WHERE table_name='db';
COUNT(*)
22
Test that join between two IS views work fine in LOCK TABLE mode.
SELECT COUNT(*) FROM information_schema.tables m
JOIN information_schema.columns n
ON m.table_name = n.table_name
WHERE m.table_name='db';
COUNT(*)
22
Test that IS view will be opened along with other locked tables.
SELECT COUNT(*) FROM information_schema.columns, t1
WHERE table_name='db';
COUNT(*)
22
SELECT COUNT(*) FROM information_schema.columns, v1
WHERE table_name='db';
COUNT(*)
22
Test that we fail when tring to use user table that is not locked
along with IS view.
SELECT COUNT(*) FROM information_schema.columns, t2
WHERE table_name='db';
ERROR HY000: Table 't2' was not locked with LOCK TABLES
UNLOCK TABLES;
Case 3: IS query inside SP + SF + LOCK TABLE combination.
CREATE FUNCTION func1()
RETURNS INT DETERMINISTIC
BEGIN
DECLARE a int;
SELECT COUNT(*) INTO a
FROM information_schema.columns
WHERE table_name='db';
RETURN a;
END $
CREATE PROCEDURE proc1()
BEGIN
DECLARE i INT;
SELECT (func1() + COUNT(*)) INTO i
FROM information_schema.tables m
JOIN information_schema.columns n
ON m.table_name = n.table_name
WHERE m.table_name='db';
INSERT INTO t1 VALUES (i);
END $
Testing IS view inside SP + SF, not in LOCK TABLE.
CALL proc1();
SELECT * FROM t1;
f1
1
44
Enter LOCK TABLE mode.
LOCK TABLE t1 WRITE, t1 as X READ, t3 READ;
Testing IS view inside SF.
SELECT func1() as COUNT_FROM_SP FROM t3;
COUNT_FROM_SP
22
Testing IS view in outer query and also inside a SF.
SELECT func1() as COUNT_FROM_SP, COUNT(*) FROM information_schema.tables m
JOIN information_schema.columns n
ON m.table_name = n.table_name
WHERE m.table_name='db';
COUNT_FROM_SP	COUNT(*)
22	22
Testing IS view inside SP + SF, in LOCK TABLE mode.
CALL proc1();
SELECT * FROM t1 as X;
f1
1
44
44
UNLOCK TABLES;
Case 3: IS query inside SP + Trigger + LOCK TABLE combination.
CREATE TRIGGER trig1 AFTER INSERT ON t1 FOR EACH ROW
INSERT INTO t2 SELECT COUNT(*)>1 FROM information_schema.columns;
Invoking trigger with IS not in LOCK TABLE mode.
INSERT INTO t1 VALUES(1);
SELECT * FROM t2;
f1
1
1
LOCK TABLE t1 WRITE, t2 WRITE;
Invoking trigger with IS in LOCK TABLE mode.
INSERT INTO t1 VALUES(1);
SELECT * FROM t2;
f1
1
1
1
UNLOCK TABLES;
Case 4: IS query under SERIALIZABLE ISOLATION + AUTO_COMMIT OFF
+ LOCK TABLE + I_S query + DROP TABLE.

DROP TABLE should success. If I_S query doesn't do non-locking read,
then the DROP TABLE would block, waiting for innodb lock to be release
until transaction is committed.
SET TRANSACTION_ISOLATION='SERIALIZABLE';
SET AUTOCOMMIT=0;
LOCK TABLES t1 READ;
SELECT COUNT(*) > 0 FROM information_schema.tables;
COUNT(*) > 0
1
DROP TABLE t2;
UNLOCK TABLES;
SET TRANSACTION_ISOLATION=default;
SET AUTOCOMMIT=default;
DROP TABLE t1, t3;
DROP VIEW v1;
DROP FUNCTION func1;
DROP PROCEDURE proc1;
