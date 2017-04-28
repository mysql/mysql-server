set @start_read_only= @@global.read_only;
set @start_autocommit= @@global.autocommit;
set @@global.autocommit= 0;
CREATE USER test@localhost;
grant CREATE, SELECT, UPDATE on *.* to test@localhost;
CREATE USER test2@localhost;
grant CREATE, SELECT, UPDATE on *.* to test2@localhost;
connection default;
CREATE TABLE t1 ( a INT, b INT) ENGINE=MyISAM;
INSERT INTO t1 VALUES (1, 1);
INSERT INTO t1 VALUES (2, 2);
INSERT INTO t1 VALUES (3, 3);
CREATE TABLE t2 ( a INT) ENGINE=InnoDB;
INSERT INTO t2 VALUES (10);
INSERT INTO t2 VALUES (20);
INSERT INTO t2 VALUES (30);
########################################################################
# SELECT...FOR UPDATE:sequential search lock all rows as no key exists
connect  con1,localhost,test,,test;
SET SESSION innodb_lock_wait_timeout=1;
BEGIN;
SELECT * FROM t2 WHERE a=20 FOR UPDATE ;
a
20
connect  con2,localhost,test2,,test;
BEGIN;
SET SESSION innodb_lock_wait_timeout=1;
#
#  UPDATE ...
SELECT * FROM t1 FOR UPDATE;
a	b
1	1
2	2
3	3
SELECT * FROM t1 FOR UPDATE NOWAIT;
a	b
1	1
2	2
3	3
SELECT * FROM t1 FOR UPDATE SKIP LOCKED;
a	b
1	1
2	2
3	3
#
#  SHARE ...
SELECT * FROM t1 FOR SHARE;
a	b
1	1
2	2
3	3
SELECT * FROM t1 FOR SHARE NOWAIT;
a	b
1	1
2	2
3	3
SELECT * FROM t1 FOR SHARE SKIP LOCKED;
a	b
1	1
2	2
3	3
#
#  Dual locking clauses
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR UPDATE OF t2;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR SHARE OF t2;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1 FOR SHARE OF t1 NOWAIT;
a	b
1	1
2	2
3	3
SELECT * FROM t1 FOR SHARE OF t1 SKIP LOCKED;
a	b
1	1
2	2
3	3
SELECT * FROM t1, t2 FOR SHARE OF t1 NOWAIT FOR SHARE OF t2 NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
COMMIT;
connection con1;
commit;
connection default;
ALTER TABLE t2 ADD PRIMARY KEY (a);
#######################################################################
# SELECT ... FOR UPDATE is only locking row with a=2
connection con1;
BEGIN;
SELECT * FROM t2 WHERE a=20 FOR UPDATE ;
a
20
connection con2;
BEGIN;
SET SESSION innodb_lock_wait_timeout=1;
#
#  UPDATE ...
SELECT * FROM t1 FOR UPDATE;
a	b
1	1
2	2
3	3
SELECT * FROM t1 FOR UPDATE NOWAIT;
a	b
1	1
2	2
3	3
SELECT * FROM t1 FOR UPDATE SKIP LOCKED;
a	b
1	1
2	2
3	3
#
#  SHARE ...
SELECT * FROM t1 FOR SHARE;
a	b
1	1
2	2
3	3
SELECT * FROM t1 FOR SHARE NOWAIT;
a	b
1	1
2	2
3	3
SELECT * FROM t1 FOR SHARE SKIP LOCKED;
a	b
1	1
2	2
3	3
#
#  Dual locking clauses
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR UPDATE OF t2;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR SHARE OF t2;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1 FOR SHARE OF t1 NOWAIT;
a	b
1	1
2	2
3	3
SELECT * FROM t1 FOR SHARE OF t1 SKIP LOCKED;
a	b
1	1
2	2
3	3
SELECT * FROM t1, t2 FOR SHARE OF t1 NOWAIT FOR SHARE OF t2 NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
COMMIT;
connection con1;
COMMIT;
connection default;
disconnect con1;
disconnect con2;
DROP TABLE t1, t2;
DROP USER test@localhost;
DROP USER test2@localhost;
set @@global.read_only= @start_read_only;
set @@global.autocommit= @start_autocommit;
