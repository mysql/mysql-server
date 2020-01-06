set @start_read_only= @@global.read_only;
set @start_autocommit= @@global.autocommit;
set @@global.autocommit= 0;
CREATE USER test@localhost;
grant CREATE, SELECT, UPDATE on *.* to test@localhost;
CREATE USER test2@localhost;
grant CREATE, SELECT, UPDATE on *.* to test2@localhost;
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
#######################################################################
# SELECT...FOR UPDATE:sequential search lock all rows as no key exists
connect  con1,localhost,test,,test;
SET SESSION innodb_lock_wait_timeout=1;
BEGIN;
SELECT * FROM t1 WHERE a=2 FOR UPDATE ;
a	b
2	2
connect  con2,localhost,test2,,test;
BEGIN;
SET SESSION innodb_lock_wait_timeout=1;
#
#  UPDATE ...
SELECT * FROM t1 FOR UPDATE;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1 FOR UPDATE NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
SELECT * FROM t1 FOR UPDATE SKIP LOCKED;
a	b
#
#  SHARE ...
SELECT * FROM t1 FOR SHARE;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1 FOR SHARE NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
SELECT * FROM t1 FOR SHARE SKIP LOCKED;
a	b
#
#  Dual locking clauses
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR UPDATE OF t2;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR SHARE OF t2;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1 FOR SHARE OF t1 NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
SELECT * FROM t1 FOR SHARE OF t1 SKIP LOCKED;
a	b
SELECT * FROM t1, t2 FOR SHARE OF t1 NOWAIT FOR SHARE OF t2 NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
explain SELECT * FROM t1, t2 FOR SHARE OF t1 SKIP LOCKED 
FOR UPDATE OF t2 NOWAIT;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	3	100.00	NULL
1	SIMPLE	t2	NULL	ALL	NULL	NULL	NULL	NULL	3	100.00	Using join buffer (hash join)
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a`,`test`.`t1`.`b` AS `b`,`test`.`t2`.`a` AS `a` from `test`.`t1` join `test`.`t2`
COMMIT;
connection con1;
commit;
connection default;
ALTER TABLE t1 ADD PRIMARY KEY (a);
########################################################################
# SELECT ... FOR UPDATE is only locking row with a=2
connection con1;
BEGIN;
SELECT * FROM t1 WHERE a=2 FOR UPDATE ;
a	b
2	2
connection con2;
BEGIN;
SET SESSION innodb_lock_wait_timeout=1;
#
#  UPDATE ...
SELECT * FROM t1 FOR UPDATE;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1 FOR UPDATE NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
SELECT * FROM t1 ORDER BY a FOR UPDATE SKIP LOCKED;
a	b
1	1
3	3
UPDATE t1 SET b=4 WHERE a=3;
UPDATE t1 SET b=5 WHERE a=1;
SELECT * FROM t1 ORDER BY a;
a	b
1	5
2	2
3	4
#
#  SHARE ...
SELECT * FROM t1 FOR SHARE;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1 FOR SHARE NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
SELECT * FROM t1 ORDER BY a FOR SHARE SKIP LOCKED;
a	b
1	5
3	4
#
#  Dual locking clauses
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR UPDATE OF t2;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1, t2 FOR SHARE OF t1 FOR SHARE OF t2;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SELECT * FROM t1 FOR SHARE OF t1 NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
SELECT * FROM t1 FOR SHARE OF t1 SKIP LOCKED;
a	b
1	5
3	4
SELECT * FROM t1, t2 FOR SHARE OF t1 NOWAIT FOR SHARE OF t2 NOWAIT;
ERROR HY000: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set.
COMMIT;
connection con1;
COMMIT;
connection default;
disconnect con1;
disconnect con2;
DROP VIEW v1;
DROP TABLE t1, t2;
DROP USER test@localhost;
DROP USER test2@localhost;
set @@global.read_only= @start_read_only;
set @@global.autocommit= @start_autocommit;
