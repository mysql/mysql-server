--echo # Test of SKIP LOCKED and NOWAIT on read only database.
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

ALTER TABLE t1 ADD PRIMARY KEY (a);

--echo ########################################################################
--echo # SELECT ... FOR UPDATE is only locking row with a=2
connect (con1,localhost,test,,test);
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
COMMIT;

connection default;
set global read_only=ON;

--echo ########################################################################
--echo # SELECT ... FOR UPDATE on a readonly db (with key)
connection con2;
BEGIN;
--echo #
--echo #  UPDATE ...
--error ER_OPTION_PREVENTS_STATEMENT
SELECT * FROM t1 FOR UPDATE;
--error ER_OPTION_PREVENTS_STATEMENT
SELECT * FROM t1 FOR UPDATE NOWAIT;
--error ER_OPTION_PREVENTS_STATEMENT
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
--error ER_OPTION_PREVENTS_STATEMENT
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
set global read_only=OFF;

connection con1;
--echo ########################################################################
--echo # SELECT ... FOR SHARE is locking (with key)
connection con1;
BEGIN;
SELECT * FROM t1 WHERE a=2 FOR SHARE ;

connection default;
set global read_only=ON;

connection con2;
BEGIN;
SET SESSION innodb_lock_wait_timeout=1;

--echo #
--echo #  UPDATE ...
--error ER_OPTION_PREVENTS_STATEMENT
SELECT * FROM t1 FOR UPDATE;
--error ER_OPTION_PREVENTS_STATEMENT
SELECT * FROM t1 FOR UPDATE NOWAIT;
--error ER_OPTION_PREVENTS_STATEMENT
SELECT * FROM t1 FOR UPDATE SKIP LOCKED;

--echo #
--echo #  SHARE ...
SELECT * FROM t1 FOR SHARE;
SELECT * FROM t1 FOR SHARE NOWAIT;
SELECT * FROM t1 FOR SHARE SKIP LOCKED;

--echo #
--echo #  Dual locking clauses
--error ER_OPTION_PREVENTS_STATEMENT
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR UPDATE OF t2;
--sorted_result
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR SHARE OF t2;
SELECT * FROM t1 FOR SHARE OF t1 NOWAIT;
SELECT * FROM t1 FOR SHARE OF t1 SKIP LOCKED;
--sorted_result
SELECT * FROM t1, t2 FOR SHARE OF t1 NOWAIT FOR SHARE OF t2 NOWAIT;
COMMIT;

connection con1;
COMMIT;

connection default;
set global read_only=OFF;

--echo ########################################################################
--echo # SELECT ... FOR SHARE OF ... FOR UPDATE OF ...
connection con1;
BEGIN;
--sorted_result
SELECT * FROM t1, t2 FOR UPDATE OF t1 FOR SHARE OF t2;

connection default;
set global read_only=ON;

connection con2;
BEGIN;
SET SESSION innodb_lock_wait_timeout=1;
--echo #
--echo #  UPDATE ...
--error ER_OPTION_PREVENTS_STATEMENT
SELECT * FROM t1 FOR UPDATE;
--error ER_OPTION_PREVENTS_STATEMENT
SELECT * FROM t1 FOR UPDATE NOWAIT;
--error ER_OPTION_PREVENTS_STATEMENT
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
--error ER_OPTION_PREVENTS_STATEMENT
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
set global read_only=OFF;

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

