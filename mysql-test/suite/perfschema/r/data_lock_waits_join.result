Bug #28733170
PERFORMANCE_SCHEMA.DATA_LOCKS DOES NOT HANDLE JOINS CORRECTLY
CREATE TABLE t1(
id INT PRIMARY KEY
) Engine=InnoDB;
INSERT INTO t1 VALUES (1);
BEGIN;
SELECT * FROM t1 WHERE id=1 FOR SHARE;
id
1
SELECT * FROM t1 WHERE id=1 FOR UPDATE;
id
1
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL C1_will_wait';
SELECT * FROM t1 WHERE id=1 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR C1_will_wait';
CREATE TABLE t2 (
id INT PRIMARY KEY AUTO_INCREMENT,
lock_id VARCHAR(200) NOT NULL
) Engine=InnoDB;
INSERT INTO t2 (lock_id)
SELECT blocking_engine_lock_id
FROM performance_schema.data_lock_waits;
SELECT COUNT(*) FROM t2;
COUNT(*)
2
SELECT COUNT(t2.id)
FROM t2
JOIN performance_schema.data_lock_waits dlw
ON(dlw.blocking_engine_lock_id = t2.lock_id)
ORDER BY t2.id;
COUNT(t2.id)
2
CREATE TABLE t3 (
id INT PRIMARY KEY AUTO_INCREMENT,
lock_id VARCHAR(200) NOT NULL
) Engine=InnoDB;
INSERT INTO t3 (lock_id)
SELECT requesting_engine_lock_id
FROM performance_schema.data_lock_waits
LIMIT 1;
INSERT INTO t3 (lock_id)
SELECT requesting_engine_lock_id
FROM performance_schema.data_lock_waits
LIMIT 1;
SELECT COUNT(DISTINCT t3.id)
FROM t3
JOIN performance_schema.data_lock_waits dlw
ON(dlw.requesting_engine_lock_id = t3.lock_id)
ORDER BY t3.id;
COUNT(DISTINCT t3.id)
2
SELECT COUNT(*) FROM performance_schema.data_lock_waits;
COUNT(*)
2
SELECT COUNT(*)
FROM performance_schema.data_lock_waits
JOIN performance_schema.data_locks r
ON(r.engine_lock_id = requesting_engine_lock_id)
JOIN performance_schema.data_locks b
ON(b.engine_lock_id = blocking_engine_lock_id);
COUNT(*)
2
DROP TABLE t2;
DROP TABLE t3;
ROLLBACK;
DROP TABLE t1;
