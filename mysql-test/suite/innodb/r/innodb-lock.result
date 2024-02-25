set global innodb_table_locks=1;
select @@innodb_table_locks;
@@innodb_table_locks
1
drop table if exists t1;
set @@innodb_table_locks=1;
create table t1 (id integer, x integer) engine=INNODB;
insert into t1 values(0, 0);
set autocommit=0;
SELECT * from t1 where id = 0 FOR UPDATE;
id	x
0	0
set autocommit=0;
lock table t1 write;
update t1 set x=1 where id = 0;
select * from t1;
id	x
0	1
commit;
update t1 set x=2 where id = 0;
commit;
unlock tables;
select * from t1;
id	x
0	2
commit;
drop table t1;
#
# Old lock method (where LOCK TABLE was ignored by InnoDB) no longer
# works when LOCK TABLE ... WRITE is used due to fix for bugs #46272
# "MySQL 5.4.4, new MDL: unnecessary and bug #37346 "innodb does not
# detect deadlock between update and alter table".
# After WL#6671 "Improve scalability by not using thr_lock.c locks
# for InnoDB tables" was implemented it no longer works for LOCK TABLES
# ,,, READ as well.
# LOCK TABLES locks are now completely handled by MDL subsystem.
#
set @@innodb_table_locks=0;
create table t1 (id integer primary key, x integer) engine=INNODB;
insert into t1 values(0, 0),(1,1),(2,2);
commit;
SELECT * from t1 where id = 0 FOR UPDATE;
id	x
0	0
# Connection 'con2'.
set autocommit=0;
set @@innodb_table_locks=0;
# The following statement should block because SQL-level lock
# is taken on t1 which will wait until concurrent transaction
# is commited.
# Sending:
lock table t1 write;;
# Connection 'con1'.
# Wait until LOCK TABLE is blocked on SQL-level lock.
# We should be able to do UPDATEs and SELECTs within transaction.
update t1 set x=1 where id = 0;
select * from t1;
id	x
0	1
1	1
2	2
# Unblock LOCK TABLE.
commit;
# Connection 'con2'.
# Reap LOCK TABLE.
unlock tables;
# Connection 'con1'.
select * from t1 where id = 0 for update;
id	x
0	1
# Connection 'con2'.
# The following statement should block because SQL-level lock
# is taken on t1 which will wait until concurrent transaction
# is commited.
# Sending:
lock table t1 read;;
# Connection 'con1'.
# Wait until LOCK TABLE is blocked on SQL-level lock.
# We should be able to do UPDATEs and SELECTs within transaction.
update t1 set x=2 where id = 0;
select * from t1;
id	x
0	2
1	1
2	2
# Unblock LOCK TABLE.
commit;
# Connection 'con2'.
# Reap LOCK TABLE.
unlock tables;
# Connection 'default'.
drop table t1;
#
#Bug#12842206 INNODB LOCKING REGRESSION FOR INSERT IGNORE
#fixed by re-fixing Bug#7975
#aka Bug#11759688 52020: InnoDB can still deadlock on just INSERT...
#
CREATE TABLE t1 (a INT PRIMARY KEY, b INT NOT NULL) ENGINE=InnoDB;
INSERT INTO t1 VALUES(3,1);
BEGIN;
INSERT IGNORE INTO t1 VALUES(3,14);
Warnings:
Warning	1062	Duplicate entry '3' for key 't1.PRIMARY'
BEGIN;
INSERT IGNORE INTO t1 VALUES(3,23);
Warnings:
Warning	1062	Duplicate entry '3' for key 't1.PRIMARY'
SELECT * FROM t1 FOR UPDATE;
COMMIT;
a	b
3	1
COMMIT;
DROP TABLE t1;
Bug#19803418   ASSERTION: TABLE->QUIESCE == QUIESCE_NONE
IN ROW0QUIESCE.CC LINE 683
CREATE TABLE t1(C TEXT CHARACTER SET UJIS) ENGINE=INNODB;
LOCK TABLES t1 WRITE,t1 AS t0 READ,t1 AS t2 READ;
FLUSH TABLE t1;
LOCK TABLES t1 READ,t1 AS t0 WRITE,t1 AS t2 READ;
FLUSH TABLE t1;
LOCK TABLES t1 READ,t1 AS t0 READ,t1 AS t2 READ;
FLUSH TABLE t1;
ERROR HY000: Table 't1' was locked with a READ lock and can't be updated
UNLOCK TABLES;
FLUSH TABLE t1;
DROP TABLE t1;
