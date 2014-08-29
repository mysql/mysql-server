set global innodb_lock_wait_timeout=42;
select @@innodb_lock_wait_timeout;
@@innodb_lock_wait_timeout
42
set innodb_lock_wait_timeout=1;
select @@innodb_lock_wait_timeout;
@@innodb_lock_wait_timeout
1
select @@innodb_lock_wait_timeout;
@@innodb_lock_wait_timeout
42
set global innodb_lock_wait_timeout=347;
select @@innodb_lock_wait_timeout;
@@innodb_lock_wait_timeout
42
set innodb_lock_wait_timeout=10;
select @@innodb_lock_wait_timeout;
@@innodb_lock_wait_timeout
10
select @@innodb_lock_wait_timeout;
@@innodb_lock_wait_timeout
347
SET @connection_b_id = <connection_b_id>;
create table t1(a int primary key)engine=innodb;
begin;
insert into t1 values(1),(2),(3);
select * from t1 for update;
commit;
a
1
2
3
begin;
insert into t1 values(4);
set innodb_lock_wait_timeout=3;
select * from t1 for update;
commit;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
drop table t1;
set global innodb_lock_wait_timeout=<initial_timeout>;
