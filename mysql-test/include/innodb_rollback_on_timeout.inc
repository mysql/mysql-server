#
# Bug #24200: Provide backwards compatibility mode for 4.x "rollback on
# transaction timeout"
#
--disable_warnings
drop table if exists t1;
--enable_warnings

show variables like 'innodb_rollback_on_timeout';
create table t1 (a int unsigned not null primary key) engine = innodb;
insert into t1 values (1);
commit;
connect (con1,localhost,root,,);
connect (con2,localhost,root,,);

connection con2;
begin work;
insert into t1 values (2);
select * from t1;

connection con1;
begin work;
insert into t1 values (5);
select * from t1;
# Lock wait timeout set to 2 seconds in <THIS TEST>-master.opt; this
# statement will time out; in 5.0.13+, it will not roll back transaction.
--error ER_LOCK_WAIT_TIMEOUT
insert into t1 values (2);
# On 5.0.13+, this should give ==> 1, 5
select * from t1;
commit;

connection con2;
select * from t1;
commit;

connection default;
select * from t1;
drop table t1;
disconnect con1;
disconnect con2;

--echo End of 5.0 tests


--echo #
--echo # Bug#23753319: !M_THD->TRANSACTION_ROLLBACK_REQUEST' AT
--echo # THD::ATTACHABLE_TRX::INIT IN SQL/SQL_C
--echo #

create table t1 (i int);
insert into t1 values (42);

--echo # Grab locks which will block another con from doing select in RR
BEGIN;
select * from t1 for update;

--echo # Create competing connection using RR
connect (rr_con,localhost,root,,);
BEGIN;
set session transaction isolation level repeatable read;

--echo # Will fail and request rollback due to blocking for update
--echo # (prior to fix this would trigger the assert).
--error ER_LOCK_WAIT_TIMEOUT
create table t2 as select * from t1;

--echo # Additional coverage for WL#7743 "New data dictionary: changes
--echo # to DDL-related parts of SE API". Check how rollback is handled
--echo # by similar CTS for non-transactional table.
BEGIN;
--error ER_LOCK_WAIT_TIMEOUT
create table t2 engine=myisam as select * from t1;

--echo # Cleanup
connection default;
disconnect rr_con;

COMMIT;
drop table t1;
