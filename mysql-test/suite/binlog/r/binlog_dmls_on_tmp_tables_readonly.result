DROP TABLE IF EXISTS t1 ;
# READ_ONLY does nothing to SUPER users
# so we use a non-SUPER one:
CREATE USER test@localhost;
GRANT CREATE, SELECT, DROP ON *.* TO test@localhost;
connect  con1,localhost,test,,test;
connection default;
SET GLOBAL READ_ONLY=1;
connection con1;
CREATE TEMPORARY TABLE t1 (a INT) ENGINE=INNODB;
# Test INSERTS with autocommit being off and on.
BEGIN;
INSERT INTO t1 VALUES (10);
COMMIT;
INSERT INTO t1 VALUES (20);
# Test UPDATES with autocommit being off and on.
BEGIN;
UPDATE t1 SET a=30 WHERE a=10;
COMMIT;
UPDATE t1 SET a=40 WHERE a=20;
connection default;
SET GLOBAL READ_ONLY=0;
# Test scenario where global read_only is enabled in the middle of transaction.
# Test INSERT operations on temporary tables, INSERTs should be successful even
# when global read_only is enabled.
connection con1;
BEGIN;
INSERT INTO t1 VALUES(50);
connection default;
SET GLOBAL READ_ONLY=1;
connection con1;
SELECT @@GLOBAL.READ_ONLY;
@@GLOBAL.READ_ONLY
1
COMMIT;
connection default;
SET GLOBAL READ_ONLY=0;
# Test UPDATE operations on temporary tables, UPDATEs should be successful even
# when global read_only is enabled.
connection con1;
BEGIN;
UPDATE t1 SET a=60 WHERE a=50;
connection default;
SET GLOBAL READ_ONLY=1;
connection con1;
SELECT @@GLOBAL.READ_ONLY;
@@GLOBAL.READ_ONLY
1
COMMIT;
SELECT * FROM t1;
a
30
40
60
# Clean up
connection default;
SET GLOBAL READ_ONLY=0;
disconnect con1;
DROP USER test@localhost;
