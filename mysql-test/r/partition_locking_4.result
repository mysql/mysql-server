SET @old_autocommit = @@global.autocommit;
SET @@global.autocommit = 0;
SET @old_lock_wait_timeout= @@global.lock_wait_timeout;
SET @@global.lock_wait_timeout = 1;
SET @old_innodb_lock_wait_timeout= @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout = 1;
USE test;
CREATE USER 'mysqltest1'@'localhost';
CREATE USER 'mysqltest2'@'localhost';
GRANT ALL PRIVILEGES ON test.* TO 'mysqltest1'@'localhost';
GRANT ALL PRIVILEGES ON test.* TO 'mysqltest2'@'localhost';
CREATE TABLE t1 (a int PRIMARY KEY, b varchar(128), KEY (b))
ENGINE = InnoDB
PARTITION BY HASH (a) PARTITIONS 13;
INSERT INTO t1 VALUES (11, 'First row, p11');
INSERT INTO t1 VALUES (12, 'First row, p12');
connect  test1,localhost,mysqltest1,,test;
connect  test2,localhost,mysqltest2,,test;
connection test1;
INSERT INTO t1 VALUES (13+11, 'Second row, p11');
INSERT INTO t1 VALUES (13+12, 'Second row, p12');
SELECT * FROM t1 ORDER BY a;
a	b
11	First row, p11
12	First row, p12
24	Second row, p11
25	Second row, p12
connection test2;
INSERT INTO t1 VALUES (13+11, 'Second row, p11');
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
INSERT INTO t1 VALUES (13+12, 'Second row, p12');
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1 ORDER BY a;
a	b
11	First row, p11
12	First row, p12
COMMIT;
connection test1;
SELECT * FROM t1 ORDER BY a;
a	b
11	First row, p11
12	First row, p12
24	Second row, p11
25	Second row, p12
COMMIT;
connection default;
DROP TABLE t1;
DROP USER 'mysqltest1'@'localhost';
DROP USER 'mysqltest2'@'localhost';
SET @@global.autocommit = @old_autocommit;
SET @@global.lock_wait_timeout= @old_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout= @old_innodb_lock_wait_timeout;
