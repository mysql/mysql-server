################################################################
#                                                              #
# Bug #27944920: INNODB: ASSERTION FAILURE:                    #
#                LOCK_GET_TYPE_LOW(LOCK) == 32 IN LOCK0PRIV.IC #
#                                                              #
################################################################
SHOW VARIABLES LIKE 'innodb_autoinc_lock_mode';
Variable_name	Value
innodb_autoinc_lock_mode	0
CREATE TABLE t1 (
id INT PRIMARY KEY AUTO_INCREMENT,
val INT
) Engine=InnoDB;
CREATE TABLE t2 (
id INT PRIMARY KEY
) Engine=InnoDB;
INSERT INTO t1 (id, val) VALUES (1,1);
INSERT INTO t2 (id) VALUES (1),(2),(3);
SET @innodb_lock_wait_timeout_saved = @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout = 100000;
BEGIN;
SELECT * FROM t2 WHERE id=2 FOR UPDATE;
id
2
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL C4_will_wait';
INSERT INTO t1 (val) SELECT id FROM t2;
BEGIN;
SELECT * FROM t1 WHERE id=1 FOR SHARE;
id	val
1	1
SET DEBUG_SYNC = 'now WAIT_FOR C4_will_wait';
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL C1_will_wait';
INSERT INTO t1 (val) VALUES (7);
BEGIN;
SET DEBUG_SYNC = 'now WAIT_FOR C1_will_wait';
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL C2_will_wait';
SELECT * FROM t1 WHERE id=1 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR C2_will_wait';
ROLLBACK;
ROLLBACK;
ROLLBACK;
id	val
1	1
ROLLBACK;
DROP TABLE t2;
DROP TABLE t1;
SET @@global.innodb_lock_wait_timeout = @innodb_lock_wait_timeout_saved;
########################
#                      #
# End of Bug #27944920 #
#                      #
########################
CREATE TABLE t1 (
id INT PRIMARY KEY AUTO_INCREMENT,
val INT
) Engine=InnoDB;
CREATE TABLE t2 (
id INT PRIMARY KEY
) Engine=InnoDB;
CREATE TABLE t3 (
id INT PRIMARY KEY,
val INT
) Engine=InnoDB;
INSERT INTO t1 (id, val) VALUES (1,1);
INSERT INTO t2 (id) VALUES (1),(2),(3);
INSERT INTO t3 (id, val) VALUES (1,1),(2,2),(3,3),(4,4),(5,5),(6,6);
SET @innodb_lock_wait_timeout_saved = @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout = 100000;
BEGIN;
SELECT * FROM t2 WHERE id=2 FOR UPDATE;
id
2
BEGIN;
SELECT * FROM t2 WHERE id=3 FOR UPDATE;
id
3
BEGIN;
UPDATE t3 SET val = 13;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL C2_will_wait';
INSERT INTO t1 (val) SELECT id FROM t2;
SET DEBUG_SYNC = 'now WAIT_FOR C2_will_wait';
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL C3_will_wait';
INSERT INTO t1 (val) VALUES (13);;
SET DEBUG_SYNC = 'now WAIT_FOR C3_will_wait';
ROLLBACK;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
ROLLBACK;
ROLLBACK;
DROP TABLES t1,t2,t3;
SET @@global.innodb_lock_wait_timeout = @innodb_lock_wait_timeout_saved;
