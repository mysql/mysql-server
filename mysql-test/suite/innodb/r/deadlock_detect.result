SET GLOBAL innodb_deadlock_detect=OFF;
SET GLOBAL innodb_lock_wait_timeout=2;
CREATE TABLE t1(
id	INT,
PRIMARY KEY(id)
) ENGINE=InnoDB;
INSERT INTO t1 VALUES(1), (2), (3);
BEGIN;
SELECT * FROM t1 WHERE id = 1 FOR UPDATE;
id
1
BEGIN;
SELECT * FROM t1 WHERE id = 2 FOR UPDATE;
id
2
SELECT * FROM t1 WHERE id = 1 FOR UPDATE;;
SELECT * FROM t1 WHERE id = 2 FOR UPDATE;;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
ROLLBACK;
id
2
ROLLBACK;
DROP TABLE t1;
SET GLOBAL innodb_lock_wait_timeout=default;
SET GLOBAL innodb_deadlock_detect=default;
