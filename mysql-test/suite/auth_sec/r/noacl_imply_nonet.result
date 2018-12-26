SHOW VARIABLES LIKE 'skip_networking';
Variable_name	Value
skip_networking	ON
SET PASSWORD = 'foo';
ERROR HY000: The MySQL server is running with the --skip-grant-tables option so it cannot execute this statement
# Attempting to connect using network. Must fail.
# Verify that we can operate the system
CREATE DATABASE db1;
use db1;
CREATE TABLE t1(c1 INT);
INSERT INTO t1 VALUES (1),(2),(3);
SELECT * FROM t1;
c1
1
2
3
UPDATE t1 SET c1=0;
DROP TABLE t1;
use mysql;
DROP DATABASE db1;
