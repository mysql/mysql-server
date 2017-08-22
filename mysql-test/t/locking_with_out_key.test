# Test of SKIP LOCKED anfd NOWAIT on table with PRIMARY KEY constraint.
--source include/count_sessions.inc

set @start_read_only= @@global.read_only;
set @start_autocommit= @@global.autocommit;
set @@global.autocommit= 0;
CREATE USER test@localhost;
grant CREATE, SELECT, UPDATE on *.* to test@localhost;
CREATE USER test2@localhost;
grant CREATE, SELECT, UPDATE on *.* to test2@localhost;
--enable_connect_log

connection default;

CREATE TABLE t1 ( a INT, b INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1, 1);
INSERT INTO t1 VALUES (2, 2);
INSERT INTO t1 VALUES (3, 3);

CREATE VIEW v1 AS SELECT * FROM t1;

CREATE TABLE t2 ( a INT) ENGINE=InnoDB;
INSERT INTO t2 VALUES (10);
INSERT INTO t2 VALUES (20);
INSERT INTO t2 VALUES (30);

--echo #######################################################################
--echo # SELECT...FOR UPDATE:sequential search lock all rows as no key exists
connect (con1,localhost,test,,test);
SET SESSION innodb_lock_wait_timeout=1;
BEGIN;
SELECT * FROM t1 WHERE a=2 FOR UPDATE ;

connect (con2,localhost,test2,,test);
BEGIN;
SET SESSION innodb_lock_wait_timeout=1;

--echo #
--echo #  UPDATE ...
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 FOR UPDATE;
--error ER_LOCK_NOWAIT
SELECT * FROM t1 FOR UPDATE NOWAIT;
SELECT * FROM t1 FOR UPDATE SKIP LOCKED;

--echo #
--echo #  SHARE ...
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 FOR SHARE;
--error ER_LOCK_NOWAIT
SELECT * FROM t1 FOR SHARE NOWAIT;
SELECT * FROM t1 FOR SHARE SKIP LOCKED;

--echo #
--echo #  Dual locking clauses
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR UPDATE OF t2;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR SHARE OF t2;
--error ER_LOCK_NOWAIT
SELECT * FROM t1 FOR SHARE OF t1 NOWAIT;
SELECT * FROM t1 FOR SHARE OF t1 SKIP LOCKED;
--error ER_LOCK_NOWAIT
SELECT * FROM t1, t2 FOR SHARE OF t1 NOWAIT FOR SHARE OF t2 NOWAIT;
explain SELECT * FROM t1, t2 FOR SHARE OF t1 SKIP LOCKED 
   FOR UPDATE OF t2 NOWAIT;
COMMIT;

connection con1;
commit;

connection default;
ALTER TABLE t1 ADD PRIMARY KEY (a);

--echo ########################################################################
--echo # SELECT ... FOR UPDATE is only locking row with a=2
connection con1;
BEGIN;
SELECT * FROM t1 WHERE a=2 FOR UPDATE ;

connection con2;
BEGIN;
SET SESSION innodb_lock_wait_timeout=1;

--echo #
--echo #  UPDATE ...
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 FOR UPDATE;
--error ER_LOCK_NOWAIT
SELECT * FROM t1 FOR UPDATE NOWAIT;
SELECT * FROM t1 ORDER BY a FOR UPDATE SKIP LOCKED;
UPDATE t1 SET b=4 WHERE a=3;
UPDATE t1 SET b=5 WHERE a=1;
SELECT * FROM t1 ORDER BY a;

--echo #
--echo #  SHARE ...
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 FOR SHARE;
--error ER_LOCK_NOWAIT
SELECT * FROM t1 FOR SHARE NOWAIT;
SELECT * FROM t1 ORDER BY a FOR SHARE SKIP LOCKED;

--echo #
--echo #  Dual locking clauses
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR UPDATE OF t2;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR SHARE OF t2;
--error ER_LOCK_NOWAIT
SELECT * FROM t1 FOR SHARE OF t1 NOWAIT;
SELECT * FROM t1 FOR SHARE OF t1 SKIP LOCKED;
--error ER_LOCK_NOWAIT
SELECT * FROM t1, t2 FOR SHARE OF t1 NOWAIT FOR SHARE OF t2 NOWAIT;
COMMIT;

connection con1;
COMMIT;

connection default;
disconnect con1;
disconnect con2;

DROP VIEW v1;
DROP TABLE t1, t2;

--disable_connect_log

DROP USER test@localhost;
DROP USER test2@localhost;
set @@global.read_only= @start_read_only;
set @@global.autocommit= @start_autocommit;
--source include/wait_until_count_sessions.inc

