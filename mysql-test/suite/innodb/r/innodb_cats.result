CREATE TABLE t0 (
id INT PRIMARY KEY,
trx_mysql_thread_id BIGINT
) ENGINE=InnoDB;
CREATE TABLE t1 (
id INT PRIMARY KEY
) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1);
CREATE TABLE t2 (
id INT PRIMARY KEY
) ENGINE=InnoDB;
INSERT INTO t2 VALUES (1);
CREATE TABLE t3 (
id INT PRIMARY KEY
) ENGINE=InnoDB;
INSERT INTO t3 VALUES (1);
CREATE TABLE t4 (
id INT PRIMARY KEY
) ENGINE=InnoDB;
INSERT INTO t4 VALUES (1);
INSERT INTO t0 VALUES (1, CONNECTION_ID());
BEGIN;
SELECT * FROM t1 FOR UPDATE;
id
1
INSERT INTO t0 VALUES (2, CONNECTION_ID());
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL will_wait_c2';
SELECT * FROM t1 FOR UPDATE;
INSERT INTO t0 VALUES (3, CONNECTION_ID());
SET DEBUG_SYNC = 'now WAIT_FOR will_wait_c2';
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL will_wait_c3';
SELECT * FROM t1 FOR UPDATE;
INSERT INTO t0 VALUES (4, CONNECTION_ID());
SET DEBUG_SYNC = 'now WAIT_FOR will_wait_c3';
BEGIN;
SELECT * FROM t2 FOR UPDATE;
id
1
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL will_wait_c4';
SELECT * FROM t1 FOR UPDATE;
INSERT INTO t0 VALUES (5, CONNECTION_ID());
SET DEBUG_SYNC = 'now WAIT_FOR will_wait_c4';
BEGIN;
SELECT * FROM t3 FOR UPDATE;
id
1
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL will_wait_c5';
SELECT * FROM t1 FOR UPDATE;
INSERT INTO t0 VALUES (6, CONNECTION_ID());
SET DEBUG_SYNC = 'now WAIT_FOR will_wait_c5';
BEGIN;
SELECT * FROM t4 FOR UPDATE;
id
1
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL will_wait_c6';
SELECT * FROM t1 FOR UPDATE;
INSERT INTO t0 VALUES (7, CONNECTION_ID());
SET DEBUG_SYNC = 'now WAIT_FOR will_wait_c6';
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL will_wait_c7';
SELECT * FROM t2 FOR UPDATE;
INSERT INTO t0 VALUES (8, CONNECTION_ID());
SET DEBUG_SYNC = 'now WAIT_FOR will_wait_c7';
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL will_wait_c8';
SELECT * FROM t3 FOR UPDATE;
INSERT INTO t0 VALUES (9, CONNECTION_ID());
SET DEBUG_SYNC = 'now WAIT_FOR will_wait_c8';
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL will_wait_c9';
SELECT * FROM t4 FOR UPDATE;
INSERT INTO t0 VALUES (10, CONNECTION_ID());
SET DEBUG_SYNC = 'now WAIT_FOR will_wait_c9';
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL will_wait_c10';
SELECT * FROM t4 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR will_wait_c10';
SELECT t0.id, trx_state, trx_schedule_weight
FROM t0
JOIN INFORMATION_SCHEMA.INNODB_TRX USING (trx_mysql_thread_id)
ORDER BY t0.id;
id	trx_state	trx_schedule_weight
1	RUNNING	NULL
2	LOCK WAIT	1
3	LOCK WAIT	1
4	LOCK WAIT	2
5	LOCK WAIT	2
6	LOCK WAIT	3
7	LOCK WAIT	1
8	LOCK WAIT	1
9	LOCK WAIT	1
10	LOCK WAIT	1
COMMIT;
id
1
COMMIT;
id
1
COMMIT;
id
1
COMMIT;
id
1
COMMIT;
id
1
COMMIT;
id
1
COMMIT;
id
1
COMMIT;
id
1
COMMIT;
id
1
COMMIT;
DROP TABLE t0;
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
################################################################
#                                                              #
# Bug #89829: lock_rec_has_to_wait_vats ignores locks held by  #
#             transactions being rolled back                   #
#                                                              #
################################################################
CREATE TABLE t1 (
id INT PRIMARY KEY,
val INT NOT NULL
) ENGINE=InnoDB;
INSERT INTO t1 (id, val) VALUES (1,1), (3,3), (5,5);
SET @innodb_lock_wait_timeout_saved = @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout = 100000;
# too_big_to_fail:
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN;
INSERT INTO t1 (id, val)
VALUES (100, 100), (200, 200), (300, 300), (400, 400), (500, 500);
SELECT * FROM t1 WHERE id = 2 FOR UPDATE;
id	val
# victim:
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN;
SELECT * FROM t1 WHERE id = 4 FOR UPDATE;
id	val
UPDATE t1 SET val = 200 WHERE id = 3;
SET DEBUG_SYNC =
'lock_wait_has_finished_waiting WAIT_FOR victim_can_rollback';
INSERT INTO t1 (id, val) VALUES (2,2);
# seer:
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL seer_will_wait';
UPDATE t1 SET val = val + 10 WHERE id = 3;
# too_big_to_fail:
SET DEBUG_SYNC = 'now WAIT_FOR seer_will_wait';
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL too_big_to_fail_will_wait';
INSERT INTO t1 (id, val) VALUES (4, 4);
# default:
SET DEBUG_SYNC = 'now WAIT_FOR too_big_to_fail_will_wait';
# Number of X locks granted: 1
SET DEBUG_SYNC = 'now SIGNAL victim_can_rollback';
# seer:
SELECT * FROM t1 WHERE id = 3 FOR UPDATE;
id	val
3	13
SET DEBUG_SYNC = 'now SIGNAL victim_can_rollback';
COMMIT;
# victim:
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
ROLLBACK;
# too_big_to_fail:
ROLLBACK;
# default:
SELECT * FROM t1;
id	val
1	1
3	13
5	5
DROP TABLE t1;
SET DEBUG_SYNC = 'RESET';
SET @@global.innodb_lock_wait_timeout = @innodb_lock_wait_timeout_saved;
#####################
#                   #
# End of Bug #89829 #
#                   #
#####################
#################################################################
#                                                               #
# Bug #89932: RecLock::add_to_waitq calls lock_update_age on    #
#             DB_DEADLOCK inflating trx->age transactions being #
#             rolled back                                       #
#                                                               #
#################################################################
##############
#            #
# Scenario 1 #
#            #
##############
CREATE TABLE t1 (
id INT PRIMARY KEY
) ENGINE=InnoDB;
INSERT INTO t1 (id) VALUES (0);
INSERT INTO t1 (id) VALUES (1);
SET @innodb_lock_wait_timeout_saved = @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout = 100000;
# too_big_to_fail:
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN;
INSERT INTO t1 (id) VALUES (-100), (-200), (-300), (-400), (-500);
#connection victim
BEGIN;
SELECT * FROM t1 WHERE id = 0 FOR UPDATE;
id
0
#connection too_big_to_fail
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL too_big_to_fail_waits';
SELECT * FROM t1 WHERE id = 0 FOR UPDATE;
SELECT trx_state, trx_schedule_weight
FROM INFORMATION_SCHEMA.INNODB_TRX
WHERE trx_mysql_thread_id=@too_big_to_fail_id;
trx_state	trx_schedule_weight
LOCK WAIT	1
#connection victim
SET DEBUG_SYNC = 'now WAIT_FOR too_big_to_fail_waits';
SELECT * FROM t1 WHERE id = -100 FOR UPDATE;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
#connection too_big_to_fail
id
0
#connection victim
BEGIN;
SELECT * FROM t1 WHERE id = 1 FOR UPDATE;
id
1
#connection too_big_to_fail
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL too_big_to_fail_waits';
SELECT * FROM t1 WHERE id = 1 FOR UPDATE;
SELECT trx_state, trx_schedule_weight
FROM INFORMATION_SCHEMA.INNODB_TRX
WHERE trx_mysql_thread_id=@too_big_to_fail_id;
trx_state	trx_schedule_weight
LOCK WAIT	1
#connection victim
SET DEBUG_SYNC = 'now WAIT_FOR too_big_to_fail_waits';
SELECT * FROM t1 WHERE id = -100 FOR UPDATE;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
#connection too_big_to_fail
id
1
ROLLBACK;
SET GLOBAL DEBUG = '-d,lock_update_trx_age_check_age_limit';
DROP TABLE t1;
SET DEBUG_SYNC = 'RESET';
SET @@global.innodb_lock_wait_timeout = @innodb_lock_wait_timeout_saved;
##############
#            #
# Scenario 2 #
#            #
##############
CREATE TABLE t1 (
id INT PRIMARY KEY
) ENGINE=InnoDB;
INSERT INTO t1 (id) VALUES (0),(1);
SET @innodb_lock_wait_timeout_saved = @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout = 100000;
# beneficiary:
BEGIN;
SELECT * FROM t1 WHERE id = 0 FOR SHARE;
id
0
# too_big_to_fail:
BEGIN;
INSERT INTO t1 (id) VALUES (-100), (-200), (-300), (-400), (-500);
SELECT * FROM t1 WHERE id = 0 FOR SHARE;
id
0
#connection victim
BEGIN;
SELECT * FROM t1 WHERE id = 1 FOR UPDATE;
id
1
#connection too_big_to_fail
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL too_big_to_fail_waits';
SELECT * FROM t1 WHERE id = 1 FOR UPDATE;
SELECT trx_state, trx_schedule_weight
FROM INFORMATION_SCHEMA.INNODB_TRX
WHERE trx_mysql_thread_id=@too_big_to_fail_id;
trx_state	trx_schedule_weight
LOCK WAIT	1
#connection victim
SET DEBUG_SYNC = 'now WAIT_FOR too_big_to_fail_waits';
SELECT * FROM t1 WHERE id = 0 FOR UPDATE;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
#connection too_big_to_fail
id
1
ROLLBACK;
# too_big_to_fail:
BEGIN;
INSERT INTO t1 (id) VALUES (-100), (-200), (-300), (-400), (-500);
SELECT * FROM t1 WHERE id = 0 FOR SHARE;
id
0
#connection victim
BEGIN;
SELECT * FROM t1 WHERE id = 1 FOR UPDATE;
id
1
#connection too_big_to_fail
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL too_big_to_fail_waits';
SELECT * FROM t1 WHERE id = 1 FOR UPDATE;
SELECT trx_state, trx_schedule_weight
FROM INFORMATION_SCHEMA.INNODB_TRX
WHERE trx_mysql_thread_id=@too_big_to_fail_id;
trx_state	trx_schedule_weight
LOCK WAIT	1
#connection victim
SET DEBUG_SYNC = 'now WAIT_FOR too_big_to_fail_waits';
SELECT * FROM t1 WHERE id = 0 FOR UPDATE;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
#connection too_big_to_fail
id
1
ROLLBACK;
ROLLBACK;
SET GLOBAL DEBUG = '-d,lock_update_trx_age_check_age_limit';
DROP TABLE t1;
SET DEBUG_SYNC = 'RESET';
SET @@global.innodb_lock_wait_timeout = @innodb_lock_wait_timeout_saved;
#####################
#                   #
# End of Bug #89932 #
#                   #
#####################
#####################################################################
#                                                                   #
# Bug #89737: Using VATS with spatial index can lead to transaction #
# never being woken up                                              #
#                                                                   #
#####################################################################
CREATE TABLE t1 (
id INT PRIMARY KEY,
p GEOMETRY NOT NULL SRID 0,
SPATIAL KEY p_idx(p)
) ENGINE=InnoDB;
INSERT INTO t1 (id, p) VALUES
(1,POINT(1,1)),
(2,POINT(2,2)),
(3,POINT(3,3)),
(4,POINT(4,4));
SET @innodb_lock_wait_timeout_saved = @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout = 10;
# Using CATS
# in selecting_thread
USE test;
BEGIN;
SELECT id
FROM t1
WHERE MBRContains(
ST_GeomFromText('Polygon((2 2, 2 4, 4 4, 4 2, 2 2))'),
p
)
FOR UPDATE;
id
3
# in inserting_thread
USE test;
BEGIN;
SET DEBUG_SYNC =
'lock_wait_will_wait SIGNAL inserting_thread_waits';
INSERT INTO t1 (id, p) VALUES (0, POINT (3, 3));;
# in selecting_thread
SET DEBUG_SYNC = 'now WAIT_FOR inserting_thread_waits';
COMMIT;
# in inserting_thread
ROLLBACK;
DROP TABLE t1;
SET DEBUG_SYNC = 'RESET';
SET @@global.innodb_lock_wait_timeout = @innodb_lock_wait_timeout_saved;
#####################
#                   #
# End of Bug #89737 #
#                   #
#####################
