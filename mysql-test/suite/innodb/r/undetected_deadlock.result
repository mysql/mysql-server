#
# Bug #29882690  UNDETECTED DEADLOCK WITH GAP AND INSERT INTENTION LOCKS
#                IS POSSIBLE
#
SET @innodb_lock_wait_timeout_saved = @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout = 20;
CREATE TABLE t0 (id INT NOT NULL PRIMARY KEY);
INSERT INTO t0 VALUES (1), (2), (3);
CREATE PROCEDURE show_locks ()
BEGIN
SELECT engine_transaction_id INTO @con1
FROM performance_schema.data_locks
WHERE object_name = 't0' AND lock_data = '1';
SELECT engine_transaction_id INTO @con2
FROM performance_schema.data_locks
WHERE object_name = 't0' AND lock_data = '2';
SELECT engine_transaction_id INTO @con3
FROM performance_schema.data_locks
WHERE object_name = 't0' AND lock_data = '3';
SELECT
CASE engine_transaction_id
WHEN @con1 THEN "con1"
      WHEN @con2 THEN "con2"
      WHEN @con3 THEN "con3"
      ELSE "unknown"
    END connection,index_name,lock_type,lock_mode,lock_status,lock_data
FROM performance_schema.data_locks
WHERE object_name = 't1'
    ORDER BY 1,2,3,4,5,6;
END //
CREATE TABLE t1 (id INT NOT NULL PRIMARY KEY, val VARCHAR(200));
INSERT INTO t1 (id,val) VALUES (1, "a"),     (3, "c"), (4, "d");
BEGIN;
SELECT * FROM t0 WHERE id=1 FOR UPDATE;
id
1
BEGIN;
SELECT * FROM t0 WHERE id=2 FOR UPDATE;
id
2
BEGIN;
SELECT * FROM t0 WHERE id=3 FOR UPDATE;
id
3
# 1. con1 obtains a GRANTED LOCK_GAP on gap before row id=3
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
id	val
CALL show_locks();
connection	index_name	lock_type	lock_mode	lock_status	lock_data
con1	NULL	TABLE	IX	GRANTED	NULL
con1	PRIMARY	RECORD	X,GAP	GRANTED	3
# 2. con2 obtains a GRANTED LOCK_X on row id=4
SELECT * FROM t1 WHERE id=4 FOR UPDATE;
id	val
4	d
CALL show_locks();
connection	index_name	lock_type	lock_mode	lock_status	lock_data
con1	NULL	TABLE	IX	GRANTED	NULL
con1	PRIMARY	RECORD	X,GAP	GRANTED	3
con2	NULL	TABLE	IX	GRANTED	NULL
con2	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	4
# 3. con2 enqueues a waiting LOCK_INSERT_INTENTION into the gap
#    before id=3
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con2_will_wait';
INSERT INTO t1 (id, val) VALUES (2, "b");
SET DEBUG_SYNC = 'now WAIT_FOR con2_will_wait';
CALL show_locks();
connection	index_name	lock_type	lock_mode	lock_status	lock_data
con1	NULL	TABLE	IX	GRANTED	NULL
con1	PRIMARY	RECORD	X,GAP	GRANTED	3
con2	NULL	TABLE	IX	GRANTED	NULL
con2	PRIMARY	RECORD	X,GAP,INSERT_INTENTION	WAITING	3
con2	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	4
# 4. con3 obtains a GRANTED LOCK_GAP on gap before row id=3
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
id	val
CALL show_locks();
connection	index_name	lock_type	lock_mode	lock_status	lock_data
con1	NULL	TABLE	IX	GRANTED	NULL
con1	PRIMARY	RECORD	X,GAP	GRANTED	3
con2	NULL	TABLE	IX	GRANTED	NULL
con2	PRIMARY	RECORD	X,GAP,INSERT_INTENTION	WAITING	3
con2	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	4
con3	NULL	TABLE	IX	GRANTED	NULL
con3	PRIMARY	RECORD	X,GAP	GRANTED	3
# 5. con3 enqueues a waiting lock on same row as in step 2.
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con3_will_wait';
SELECT * FROM t1 WHERE id=4 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR con3_will_wait';
CALL show_locks();
connection	index_name	lock_type	lock_mode	lock_status	lock_data
con1	NULL	TABLE	IX	GRANTED	NULL
con1	PRIMARY	RECORD	X,GAP	GRANTED	3
con2	NULL	TABLE	IX	GRANTED	NULL
con2	PRIMARY	RECORD	X,GAP,INSERT_INTENTION	WAITING	3
con2	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	4
con3	NULL	TABLE	IX	GRANTED	NULL
con3	PRIMARY	RECORD	X,GAP	GRANTED	3
con3	PRIMARY	RECORD	X,REC_NOT_GAP	WAITING	4
# 6. con1 resizes the row with id=3 ("c"->"cccccccccccccccc") causing
#    locks to be moved
UPDATE t1 SET val="cccccccccccccccc" WHERE id=3;
# 7. it depends on implementation of con3's LOCK_GAP lands is in front
#    of con2's waiting II or not
# 8. con1 commits
COMMIT;
ROLLBACK;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
ROLLBACK;
DROP TABLE t1;
DROP PROCEDURE show_locks;
DROP TABLE t0;
SET @@global.innodb_lock_wait_timeout = @innodb_lock_wait_timeout_saved ;
