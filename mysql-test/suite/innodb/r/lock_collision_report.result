CREATE TABLE t (
id INT PRIMARY KEY
) ENGINE=InnoDB;
INSERT INTO t VALUES (10), (20), (30), (40), (50);
# Adding debug point 'syncpoint_report_lock_collision' to @@GLOBAL.debug
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
SELECT id FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR SHARE;
id
20
BEGIN;
INSERT INTO t VALUES (60), (70), (80), (90), (100), (110), (120);
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id = 20 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
SELECT * FROM t FORCE INDEX (PRIMARY) WHERE id=100 FOR SHARE;
# expect 25 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 24 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 23 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 22 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 21 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 20 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 19 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 18 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 17 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 16 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 15 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 14 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 13 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 12 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 11 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 10 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 9 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 8 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 7 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 6 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 5 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 4 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 3 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 2 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# expect 1 more edges to be reported
SET DEBUG_SYNC = 'now WAIT_FOR reached_report_lock_collision';
SET DEBUG_SYNC = 'now SIGNAL continue_report_lock_collision';
# excpecting that connection 13 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
# excpecting that connection 12 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
# excpecting that connection 11 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
# excpecting that connection 10 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
# excpecting that connection 9 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
# excpecting that connection 8 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
# excpecting that connection 7 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
# excpecting that connection 6 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
# excpecting that connection 5 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
# excpecting that connection 4 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
# excpecting that connection 3 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
# excpecting that connection 2 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
# excpecting that connection 1 was rolled back as a deadlock victim
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
id
20
ROLLBACK;
# Removing debug point 'syncpoint_report_lock_collision' from @@GLOBAL.debug
DROP TABLE t;
