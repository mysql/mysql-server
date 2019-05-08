SET @innodb_lock_wait_timeout_saved = @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout = 100000;
CREATE TABLE t1 (
id INT PRIMARY KEY,
val VARCHAR(50) NOT NULL COLLATE utf8mb4_0900_ai_ci,
KEY(val)
);
INSERT INTO t1 (id,val) VALUES (1,"a"), (2,"a");
BEGIN;
UPDATE t1 SET val="A" WHERE id=2;
SELECT index_name, lock_type, lock_mode, lock_status, lock_data
FROM performance_schema.data_locks
WHERE object_schema="test" AND object_name="t1"
  ORDER BY 1, 2, 3, 4, 5;
index_name	lock_type	lock_mode	lock_status	lock_data
NULL	TABLE	IX	GRANTED	NULL
PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	2
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con2_will_wait';
SELECT COUNT(1) FROM t1 WHERE val="a" AND CONVERT(val,binary)="A" FOR SHARE;
SET DEBUG_SYNC = 'now WAIT_FOR con2_will_wait';
COMMIT;
COUNT(1)
1
COMMIT;
DROP TABLE t1;
SET @@global.innodb_lock_wait_timeout = @innodb_lock_wait_timeout_saved;
