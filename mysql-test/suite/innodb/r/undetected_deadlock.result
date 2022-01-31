#
# Bug #29882690  UNDETECTED DEADLOCK WITH GAP AND INSERT INTENTION LOCKS
#                IS POSSIBLE
#
SET @innodb_lock_wait_timeout_saved = @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout = 20;
CREATE TABLE connections (
trx_mysql_thread_id BIGINT PRIMARY KEY,
name VARCHAR(200)
) ENGINE=InnoDB;
CREATE PROCEDURE show_locks ()
BEGIN
# this is needed because INFORMATION_SCHEMA.INNODB_TRX is refreshed with debounce 100ms
SELECT SLEEP(1);
SELECT
connections.name, object_name, index_name, lock_type, lock_mode, lock_status, lock_data
FROM connections
JOIN INFORMATION_SCHEMA.INNODB_TRX USING (trx_mysql_thread_id)
JOIN performance_schema.data_locks ON (trx_id=engine_transaction_id)
ORDER BY 1,2,3,4,5,6,7;
END //
CREATE PROCEDURE register_connection (name VARCHAR(200))
BEGIN
INSERT INTO connections VALUES (CONNECTION_ID(), name);
END //
CREATE TABLE t1 (id INT NOT NULL PRIMARY KEY, val VARCHAR(200));
INSERT INTO t1 (id,val) VALUES (1, "a"),     (3, "c"), (4, "d");
CALL register_connection("con1");
BEGIN;
CALL register_connection("con2");
BEGIN;
CALL register_connection("con3");
BEGIN;
# 1. con1 obtains a GRANTED LOCK_GAP on gap before row id=3
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
id	val
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IX	GRANTED	NULL
con1	t1	PRIMARY	RECORD	X,GAP	GRANTED	3
# 2. con2 obtains a GRANTED LOCK_X on row id=4
SELECT * FROM t1 WHERE id=4 FOR UPDATE;
id	val
4	d
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IX	GRANTED	NULL
con1	t1	PRIMARY	RECORD	X,GAP	GRANTED	3
con2	t1	NULL	TABLE	IX	GRANTED	NULL
con2	t1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	4
# 3. con2 enqueues a waiting LOCK_INSERT_INTENTION into the gap
#    before id=3
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con2_will_wait';
INSERT INTO t1 (id, val) VALUES (2, "b");
SET DEBUG_SYNC = 'now WAIT_FOR con2_will_wait';
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IX	GRANTED	NULL
con1	t1	PRIMARY	RECORD	X,GAP	GRANTED	3
con2	t1	NULL	TABLE	IX	GRANTED	NULL
con2	t1	PRIMARY	RECORD	X,GAP,INSERT_INTENTION	WAITING	3
con2	t1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	4
# 4. con3 obtains a GRANTED LOCK_GAP on gap before row id=3
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
id	val
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IX	GRANTED	NULL
con1	t1	PRIMARY	RECORD	X,GAP	GRANTED	3
con2	t1	NULL	TABLE	IX	GRANTED	NULL
con2	t1	PRIMARY	RECORD	X,GAP,INSERT_INTENTION	WAITING	3
con2	t1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	4
con3	t1	NULL	TABLE	IX	GRANTED	NULL
con3	t1	PRIMARY	RECORD	X,GAP	GRANTED	3
# 5. con3 enqueues a waiting lock on same row as in step 2.
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con3_will_wait';
SELECT * FROM t1 WHERE id=4 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR con3_will_wait';
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IX	GRANTED	NULL
con1	t1	PRIMARY	RECORD	X,GAP	GRANTED	3
con2	t1	NULL	TABLE	IX	GRANTED	NULL
con2	t1	PRIMARY	RECORD	X,GAP,INSERT_INTENTION	WAITING	3
con2	t1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	4
con3	t1	NULL	TABLE	IX	GRANTED	NULL
con3	t1	PRIMARY	RECORD	X,GAP	GRANTED	3
con3	t1	PRIMARY	RECORD	X,REC_NOT_GAP	WAITING	4
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
DROP PROCEDURE register_connection;
DROP TABLE connections;
SET @@global.innodb_lock_wait_timeout = @innodb_lock_wait_timeout_saved ;
