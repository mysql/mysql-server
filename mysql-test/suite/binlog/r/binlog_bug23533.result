SET AUTOCOMMIT=0;
CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT, b TEXT, PRIMARY KEY(a)) ENGINE=InnoDB;
SELECT COUNT(*) FROM t1;
COUNT(*)
1000
SET GLOBAL binlog_cache_size=4096;
SET GLOBAL max_binlog_cache_size=4096;
START TRANSACTION;
CREATE TABLE t2 SELECT * FROM t1;
ERROR HY000: Multi-statement transaction required more than 'max_binlog_cache_size' bytes of storage; increase this mysqld variable and try again
COMMIT;
SHOW TABLES LIKE 't%';
Tables_in_test (t%)
t1
DROP TABLE t1;
