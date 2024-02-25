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
CALL register_connection("con1");
CALL register_connection("con2");
CALL register_connection("con3");
CREATE TABLE t1 (id INT PRIMARY KEY);
INSERT INTO t1 (id) VALUES (1);
BEGIN;
SELECT * FROM t1 FOR SHARE;
id
1
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con2_will_wait';
SELECT * FROM t1 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR con2_will_wait';
SELECT * FROM t1 FOR UPDATE;
id
1
COMMIT;
id
1
COMMIT;
BEGIN;
SELECT * FROM t1 WHERE id=1 FOR UPDATE;
id
1
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con2_will_wait';
SELECT * FROM t1 FOR SHARE;
SET DEBUG_SYNC = 'now WAIT_FOR con2_will_wait';
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IX	GRANTED	NULL
con1	t1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	1
con2	t1	NULL	TABLE	IS	GRANTED	NULL
con2	t1	PRIMARY	RECORD	S	WAITING	1
INSERT INTO t1 VALUES (0);
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IX	GRANTED	NULL
con1	t1	PRIMARY	RECORD	X,GAP,INSERT_INTENTION	GRANTED	1
con1	t1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	1
ROLLBACK;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
COMMIT;
BEGIN;
SELECT * FROM t1 FOR SHARE;
id
1
BEGIN;
SELECT * FROM t1 WHERE id=1 FOR SHARE;
id
1
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IS	GRANTED	NULL
con1	t1	PRIMARY	RECORD	S	GRANTED	1
con1	t1	PRIMARY	RECORD	S	GRANTED	supremum pseudo-record
con2	t1	NULL	TABLE	IS	GRANTED	NULL
con2	t1	PRIMARY	RECORD	S,REC_NOT_GAP	GRANTED	1
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con3_will_wait';
SELECT * FROM t1 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR con3_will_wait';
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IS	GRANTED	NULL
con1	t1	PRIMARY	RECORD	S	GRANTED	1
con1	t1	PRIMARY	RECORD	S	GRANTED	supremum pseudo-record
con2	t1	NULL	TABLE	IS	GRANTED	NULL
con2	t1	PRIMARY	RECORD	S,REC_NOT_GAP	GRANTED	1
con3	t1	NULL	TABLE	IX	GRANTED	NULL
con3	t1	PRIMARY	RECORD	X	WAITING	1
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con1_will_wait';
INSERT INTO t1 VALUES (0);
SET DEBUG_SYNC = 'now WAIT_FOR con1_will_wait';
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IS	GRANTED	NULL
con1	t1	NULL	TABLE	IX	GRANTED	NULL
con1	t1	PRIMARY	RECORD	S	GRANTED	1
con1	t1	PRIMARY	RECORD	S	GRANTED	supremum pseudo-record
con1	t1	PRIMARY	RECORD	X,GAP,INSERT_INTENTION	WAITING	1
con2	t1	NULL	TABLE	IS	GRANTED	NULL
con2	t1	PRIMARY	RECORD	S,REC_NOT_GAP	GRANTED	1
con3	t1	NULL	TABLE	IX	GRANTED	NULL
con3	t1	PRIMARY	RECORD	X	WAITING	1
COMMIT;
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IS	GRANTED	NULL
con1	t1	NULL	TABLE	IX	GRANTED	NULL
con1	t1	PRIMARY	RECORD	S	GRANTED	1
con1	t1	PRIMARY	RECORD	S	GRANTED	supremum pseudo-record
con1	t1	PRIMARY	RECORD	S,GAP	GRANTED	0
con1	t1	PRIMARY	RECORD	X,GAP,INSERT_INTENTION	GRANTED	1
ROLLBACK;
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
BEGIN;
SELECT * FROM t1 FOR SHARE;
id
1
BEGIN;
SELECT * FROM t1 WHERE id=1 FOR SHARE;
id
1
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IS	GRANTED	NULL
con1	t1	PRIMARY	RECORD	S	GRANTED	1
con1	t1	PRIMARY	RECORD	S	GRANTED	supremum pseudo-record
con2	t1	NULL	TABLE	IS	GRANTED	NULL
con2	t1	PRIMARY	RECORD	S,REC_NOT_GAP	GRANTED	1
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con3_will_wait';
SELECT * FROM t1 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR con3_will_wait';
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con1_will_wait';
SELECT * FROM t1 WHERE id=1 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR con1_will_wait';
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IS	GRANTED	NULL
con1	t1	NULL	TABLE	IX	GRANTED	NULL
con1	t1	PRIMARY	RECORD	S	GRANTED	1
con1	t1	PRIMARY	RECORD	S	GRANTED	supremum pseudo-record
con1	t1	PRIMARY	RECORD	X,REC_NOT_GAP	WAITING	1
con2	t1	NULL	TABLE	IS	GRANTED	NULL
con2	t1	PRIMARY	RECORD	S,REC_NOT_GAP	GRANTED	1
con3	t1	NULL	TABLE	IX	GRANTED	NULL
con3	t1	PRIMARY	RECORD	X	WAITING	1
COMMIT;
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IS	GRANTED	NULL
con1	t1	NULL	TABLE	IX	GRANTED	NULL
con1	t1	PRIMARY	RECORD	S	GRANTED	1
con1	t1	PRIMARY	RECORD	S	GRANTED	supremum pseudo-record
con1	t1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	1
con3	t1	NULL	TABLE	IX	GRANTED	NULL
con3	t1	PRIMARY	RECORD	X	WAITING	1
id
1
COMMIT;
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
id
1
COMMIT;
BEGIN;
SELECT * FROM t1 FOR SHARE;
id
1
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con2_will_wait';
SELECT * FROM t1 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR con2_will_wait';
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con3_will_wait';
SELECT * FROM t1 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR con3_will_wait';
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IS	GRANTED	NULL
con1	t1	PRIMARY	RECORD	S	GRANTED	1
con1	t1	PRIMARY	RECORD	S	GRANTED	supremum pseudo-record
con2	t1	NULL	TABLE	IX	GRANTED	NULL
con2	t1	PRIMARY	RECORD	X	WAITING	1
con3	t1	NULL	TABLE	IX	GRANTED	NULL
con3	t1	PRIMARY	RECORD	X	WAITING	1
SELECT * FROM t1 WHERE id=1 FOR UPDATE;
id
1
CALL show_locks();
SLEEP(1)
0
name	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
con1	t1	NULL	TABLE	IS	GRANTED	NULL
con1	t1	NULL	TABLE	IX	GRANTED	NULL
con1	t1	PRIMARY	RECORD	S	GRANTED	1
con1	t1	PRIMARY	RECORD	S	GRANTED	supremum pseudo-record
con1	t1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	1
con2	t1	NULL	TABLE	IX	GRANTED	NULL
con2	t1	PRIMARY	RECORD	X	WAITING	1
con3	t1	NULL	TABLE	IX	GRANTED	NULL
con3	t1	PRIMARY	RECORD	X	WAITING	1
COMMIT;
id
1
COMMIT;
id
1
COMMIT;
DROP TABLE t1;
DROP PROCEDURE show_locks;
DROP PROCEDURE register_connection;
DROP TABLE connections;
