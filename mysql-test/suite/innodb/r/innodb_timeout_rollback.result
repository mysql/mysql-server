drop table if exists t1;
show variables like 'innodb_rollback_on_timeout';
Variable_name	Value
innodb_rollback_on_timeout	ON
create table t1 (a int unsigned not null primary key) engine = innodb;
insert into t1 values (1);
commit;
begin work;
insert into t1 values (2);
select * from t1;
a
1
2
begin work;
insert into t1 values (5);
select * from t1;
a
1
5
insert into t1 values (2);
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
select * from t1;
a
1
commit;
select * from t1;
a
1
2
commit;
select * from t1;
a
1
2
drop table t1;
End of 5.0 tests
#
# Bug#23753319: !M_THD->TRANSACTION_ROLLBACK_REQUEST' AT
# THD::ATTACHABLE_TRX::INIT IN SQL/SQL_C
#
create table t1 (i int);
insert into t1 values (42);
# Grab locks which will block another con from doing select in RR
BEGIN;
select * from t1 for update;
i
42
# Create competing connection using RR
BEGIN;
set session transaction isolation level repeatable read;
# Will fail and request rollback due to blocking for update
# (prior to fix this would trigger the assert).
create table t2 as select * from t1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
# Additional coverage for WL#7743 "New data dictionary: changes
# to DDL-related parts of SE API". Check how rollback is handled
# by similar CTS for non-transactional table.
BEGIN;
create table t2 engine=myisam as select * from t1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
# Cleanup
COMMIT;
drop table t1;
