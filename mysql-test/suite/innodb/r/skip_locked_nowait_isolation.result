SET SESSION innodb_lock_wait_timeout=1;
SET SESSION innodb_lock_wait_timeout=1;
#
# Test READ UNCOMMITTED
#
CREATE TABLE t1(
seat_id    INT,
pos        POINT NOT NULL,
state      INT,
PRIMARY KEY(seat_id)
) ENGINE=InnoDB;
INSERT INTO t1 VALUES
(1,ST_PointFromText('POINT(1 0)'),0),
(3,ST_PointFromText('POINT(2 0)'),0),
(4,ST_PointFromText('POINT(1 1)'),0),
(6,ST_PointFromText('POINT(2 1)'),0);
SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
BEGIN;
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND seat_id >= 1 LIMIT 2 FOR SHARE;
seat_id	state	ST_AsText(pos)
1	0	POINT(1 0)
3	0	POINT(2 0)
SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
BEGIN;
INSERT INTO t1 VALUES(2,ST_PointFromText('POINT(2 1)'),0);
SELECT seat_id, state, ST_AsText(pos) FROM t1 WHERE state = 0
FOR UPDATE NOWAIT;
SELECT seat_id, state, ST_AsText(pos) FROM t1 WHERE state = 0
FOR UPDATE SKIP LOCKED;
seat_id	state	ST_AsText(pos)
2	0	POINT(2 1)
4	0	POINT(1 1)
6	0	POINT(2 1)
SET @g = ST_GeomFromText('POLYGON((0 0,0 3,3 3,0 3,0 0))');
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND MBRWithin(pos, @g) LIMIT 2 FOR UPDATE NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND MBRWithin(pos, @g) LIMIT 2 FOR UPDATE SKIP LOCKED;
seat_id	state	ST_AsText(pos)
2	0	POINT(2 1)
4	0	POINT(1 1)
COMMIT;
COMMIT;
DROP TABLE t1;
#
# Test READ COMMITTED
#
CREATE TABLE t1(
seat_id    INT,
pos        POINT NOT NULL,
state      INT,
PRIMARY KEY(seat_id)
) ENGINE=InnoDB;
INSERT INTO t1 VALUES
(1,ST_PointFromText('POINT(1 0)'),0),
(3,ST_PointFromText('POINT(2 0)'),0),
(4,ST_PointFromText('POINT(1 1)'),0),
(6,ST_PointFromText('POINT(2 1)'),0);
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
BEGIN;
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND seat_id >= 1 LIMIT 2 FOR SHARE;
seat_id	state	ST_AsText(pos)
1	0	POINT(1 0)
3	0	POINT(2 0)
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
BEGIN;
INSERT INTO t1 VALUES(2,ST_PointFromText('POINT(2 1)'),0);
SELECT seat_id, state, ST_AsText(pos) FROM t1 WHERE state = 0
FOR UPDATE NOWAIT;
SELECT seat_id, state, ST_AsText(pos) FROM t1 WHERE state = 0
FOR UPDATE SKIP LOCKED;
seat_id	state	ST_AsText(pos)
2	0	POINT(2 1)
4	0	POINT(1 1)
6	0	POINT(2 1)
SET @g = ST_GeomFromText('POLYGON((0 0,0 3,3 3,0 3,0 0))');
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND MBRWithin(pos, @g) LIMIT 2 FOR UPDATE NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND MBRWithin(pos, @g) LIMIT 2 FOR UPDATE SKIP LOCKED;
seat_id	state	ST_AsText(pos)
2	0	POINT(2 1)
4	0	POINT(1 1)
COMMIT;
COMMIT;
DROP TABLE t1;
#
# Test REPEATABLE READ
#
CREATE TABLE t1(
seat_id    INT,
pos        POINT NOT NULL,
state      INT,
PRIMARY KEY(seat_id)
) ENGINE=InnoDB;
INSERT INTO t1 VALUES
(1,ST_PointFromText('POINT(1 0)'),0),
(3,ST_PointFromText('POINT(2 0)'),0),
(4,ST_PointFromText('POINT(1 1)'),0),
(6,ST_PointFromText('POINT(2 1)'),0);
SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;
BEGIN;
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND seat_id >= 1 LIMIT 2 FOR SHARE;
seat_id	state	ST_AsText(pos)
1	0	POINT(1 0)
3	0	POINT(2 0)
SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;
BEGIN;
INSERT INTO t1 VALUES(2,ST_PointFromText('POINT(2 1)'),0);
SELECT seat_id, state, ST_AsText(pos) FROM t1 WHERE state = 0
FOR UPDATE NOWAIT;
SELECT seat_id, state, ST_AsText(pos) FROM t1 WHERE state = 0
FOR UPDATE SKIP LOCKED;
seat_id	state	ST_AsText(pos)
4	0	POINT(1 1)
6	0	POINT(2 1)
SET @g = ST_GeomFromText('POLYGON((0 0,0 3,3 3,0 3,0 0))');
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND MBRWithin(pos, @g) LIMIT 2 FOR UPDATE NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND MBRWithin(pos, @g) LIMIT 2 FOR UPDATE SKIP LOCKED;
seat_id	state	ST_AsText(pos)
4	0	POINT(1 1)
6	0	POINT(2 1)
COMMIT;
COMMIT;
DROP TABLE t1;
#
# Test SERIALIZABLE
#
CREATE TABLE t1(
seat_id    INT,
pos        POINT NOT NULL,
state      INT,
PRIMARY KEY(seat_id)
) ENGINE=InnoDB;
INSERT INTO t1 VALUES
(1,ST_PointFromText('POINT(1 0)'),0),
(3,ST_PointFromText('POINT(2 0)'),0),
(4,ST_PointFromText('POINT(1 1)'),0),
(6,ST_PointFromText('POINT(2 1)'),0);
SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN;
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND seat_id >= 1 LIMIT 2 ;
seat_id	state	ST_AsText(pos)
1	0	POINT(1 0)
3	0	POINT(2 0)
SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN;
INSERT INTO t1 VALUES(2,ST_PointFromText('POINT(2 1)'),0);
SELECT seat_id, state, ST_AsText(pos) FROM t1 WHERE state = 0
FOR UPDATE NOWAIT;
SELECT seat_id, state, ST_AsText(pos) FROM t1 WHERE state = 0
FOR UPDATE SKIP LOCKED;
seat_id	state	ST_AsText(pos)
4	0	POINT(1 1)
6	0	POINT(2 1)
SET @g = ST_GeomFromText('POLYGON((0 0,0 3,3 3,0 3,0 0))');
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND MBRWithin(pos, @g) LIMIT 2 FOR UPDATE NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND MBRWithin(pos, @g) LIMIT 2 FOR UPDATE SKIP LOCKED;
seat_id	state	ST_AsText(pos)
4	0	POINT(1 1)
6	0	POINT(2 1)
COMMIT;
COMMIT;
DROP TABLE t1;
