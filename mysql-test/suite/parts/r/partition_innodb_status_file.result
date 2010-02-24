CREATE TABLE t1 (a INT) ENGINE = InnoDB PARTITION BY HASH(a);
INSERT INTO t1 VALUES (0), (1), (2);
START TRANSACTION;
UPDATE t1 SET a = 5 WHERE a = 1;
# Connection con1
# InnoDB lock timeout and monitor thread runs every 15 seconds
SET innodb_lock_wait_timeout = 20;
START TRANSACTION;
UPDATE t1 SET a = 3 WHERE a = 1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
COMMIT;
# Connection default
COMMIT;
DROP TABLE t1;
