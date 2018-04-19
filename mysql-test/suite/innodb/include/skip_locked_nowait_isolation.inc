--echo #
--echo # Test $trx_isolation_level
--echo #

connection default;

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

eval SET SESSION TRANSACTION ISOLATION LEVEL $trx_isolation_level;
BEGIN;

eval SELECT seat_id, state, ST_AsText(pos) FROM t1
   WHERE state = 0 AND seat_id >= 1 LIMIT 2 $for_share;

connection con1;

eval SET SESSION TRANSACTION ISOLATION LEVEL $trx_isolation_level;
BEGIN;

--error 0,ER_LOCK_WAIT_TIMEOUT
INSERT INTO t1 VALUES(2,ST_PointFromText('POINT(2 1)'),0);

--error 0,ER_LOCK_NOWAIT
SELECT seat_id, state, ST_AsText(pos) FROM t1 WHERE state = 0
   FOR UPDATE NOWAIT;

SELECT seat_id, state, ST_AsText(pos) FROM t1 WHERE state = 0
   FOR UPDATE SKIP LOCKED;

SET @g = ST_GeomFromText('POLYGON((0 0,0 3,3 3,0 3,0 0))');
--error ER_LOCK_NOWAIT
SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND MBRWithin(pos, @g) LIMIT 2 FOR UPDATE NOWAIT;

SELECT seat_id, state, ST_AsText(pos) FROM t1
WHERE state = 0 AND MBRWithin(pos, @g) LIMIT 2 FOR UPDATE SKIP LOCKED;

COMMIT;

connection default;
COMMIT;

DROP TABLE t1;
