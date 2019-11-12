# Test for effect of SKIP LOCKED and NOWAIT on table locking.
--source include/count_sessions.inc

set @start_read_only= @@global.read_only;
set @start_autocommit= @@global.autocommit;
set @@global.autocommit= 0;
SET SESSION lock_wait_timeout=1;

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

CREATE TABLE t2 ( a INT) ENGINE=InnoDB;
INSERT INTO t2 VALUES (10);
INSERT INTO t2 VALUES (20);
INSERT INTO t2 VALUES (30);

ALTER TABLE t2 ADD PRIMARY KEY (a);

--echo ########################################################################
--echo # Read locking of table 
connect (con1,localhost,test,,test);
SET SESSION lock_wait_timeout=1;
SET SESSION innodb_lock_wait_timeout=1;
BEGIN;
LOCK TABLES t2 READ;

connect (con2,localhost,test2,,test);
BEGIN;
SET SESSION lock_wait_timeout=1;
SET SESSION innodb_lock_wait_timeout=1;

--echo #
--echo #  UPDATE ...
SELECT * FROM t1 FOR UPDATE;
SELECT * FROM t1 FOR UPDATE SKIP LOCKED;

--echo #
--echo #  SHARE ...
SELECT * FROM t1 FOR SHARE;
#SELECT * FROM t1 FOR SHARE NOWAIT;
#SELECT * FROM t1 FOR SHARE SKIP LOCKED;

--echo #
--echo #  Dual locking clauses
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR UPDATE OF t2;
--sorted_result
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR SHARE OF t2;
SELECT * FROM t1 FOR SHARE OF t1 NOWAIT;
SELECT * FROM t1 FOR SHARE OF t1 SKIP LOCKED;
--sorted_result
SELECT * FROM t1, t2 FOR SHARE OF t1 NOWAIT FOR SHARE OF t2 NOWAIT;
COMMIT;

connection con1;
UNLOCK TABLES;
COMMIT;

--echo ########################################################################
--echo # Write locking of table
connection con1;
BEGIN;
LOCK TABLES t2 WRITE;

connection con2;
BEGIN;
SET SESSION innodb_lock_wait_timeout=1;

--echo #
--echo #  UPDATE ...
SELECT * FROM t1 FOR UPDATE;
SELECT * FROM t1 FOR UPDATE NOWAIT;
SELECT * FROM t1 FOR UPDATE SKIP LOCKED;

--echo #
--echo #  SHARE ...
SELECT * FROM t1 FOR SHARE;
SELECT * FROM t1 FOR SHARE NOWAIT;
SELECT * FROM t1 FOR SHARE SKIP LOCKED;

--echo #
--echo #  Dual locking clauses
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR UPDATE OF t2;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR SHARE OF t2;
SELECT * FROM t1 FOR SHARE OF t1 NOWAIT;
SELECT * FROM t1 FOR SHARE OF t1 SKIP LOCKED;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1, t2 FOR SHARE OF t1 NOWAIT FOR SHARE OF t2 NOWAIT;
COMMIT;

connection con1;
UNLOCK TABLES;
COMMIT;

connection default;
disconnect con1;
disconnect con2;

DROP TABLE t1, t2;

--disable_connect_log

DROP USER test@localhost;
DROP USER test2@localhost;
set @@global.read_only= @start_read_only;
set @@global.autocommit= @start_autocommit;
--source include/wait_until_count_sessions.inc

