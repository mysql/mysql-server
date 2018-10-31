## Bug#12713 (Error in a stored function called from a SELECT doesn't cause
##    ROLLBACK of statem)

##
## Pre-Requisites :
## - $engine_type should be set
##

set sql_mode=no_engine_substitution;
eval set default_storage_engine = $engine_type;
set autocommit=1;

--disable_warnings
drop table if exists t1;
drop table if exists t2;
drop table if exists t3;
drop function if exists f2;
drop procedure if exists bug12713_call;
drop procedure if exists bug12713_dump_spvars;
drop procedure if exists dummy;
--enable_warnings

create table t1 (a int);
create table t2 (a int unique);
create table t3 (a int);

## Create a trigger so that the SHOW TRIGGERS command below
## executes WHERE clause. This is required after WL#9494, as
## SHOW TRIGGERS is nothing but execution of a SELECT query
## over data dictionary tables. And a SELECT query does not
## evaluate the WHERE clause unless there are rows.
create trigger trg before insert on t1 for each row set @a:=1;

# a workaround for Bug#32633: Can not create any routine if
# SQL_MODE=no_engine_substitution

set sql_mode=default;

insert into t1 (a) values (1), (2);
insert into t3 (a) values (1), (2);

delimiter |;

## Cause a failure every time
create function f2(x int) returns int
begin
  insert into t2 (a) values (x);
  insert into t2 (a) values (x);
  return x;
end|

delimiter ;|

set autocommit=0;

flush status;
##============================================================================
## Design notes
##
## In each case, statement rollback is expected.
## for transactional engines, the rollback should be properly executed
## for non transactional engines, the rollback may cause warnings.
##
## The test pattern is as follows
## - insert 1000+N
## - statement with a side effect, that fails to insert N twice
## - a statement rollback is expected (expecting 1 row 1000+N only) in t2
## - a rollback is performed
## - expecting a clean table t2.
##============================================================================

insert into t2 (a) values (1001);
--error ER_DUP_ENTRY
insert into t1 (a) values (f2(1));
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1002);
--error ER_DUP_ENTRY
insert into t3 (a) select f2(2) from t1;
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1003);
--error ER_DUP_ENTRY
update t1 set a= a + f2(3);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1004);
--error ER_DUP_ENTRY
update t1, t3 set t1.a = 0, t3.a = 0 where (f2(4) = 4) and (t1.a = t3.a);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1005);
--error ER_DUP_ENTRY
delete from t1 where (a = f2(5));
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1006);
--error ER_DUP_ENTRY
delete from t1, t3 using t1, t3 where (f2(6) = 6) ;
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1007);
--error ER_DUP_ENTRY
replace t1 values (f2(7));
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1008);
--error ER_DUP_ENTRY
replace into t3 (a) select f2(8) from t1;
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1009);
--error ER_DUP_ENTRY
select f2(9) from t1 ;
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1010);
--error ER_DUP_ENTRY
show databases where (f2(10) = 10);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1011);
--error ER_DUP_ENTRY
show tables where (f2(11) = 11);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1012);
--error ER_DUP_ENTRY
show triggers where (f2(12) = 12);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1013);
--error ER_DUP_ENTRY
show table status where (f2(13) = 13);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1014);
--error ER_DUP_ENTRY
show open tables where (f2(14) = 14);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1015);
--error ER_DUP_ENTRY
show columns in information_schema.routines where (f2(15) = 15);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1016);
--error ER_DUP_ENTRY
show status where (f2(16) = 16);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1017);
--error ER_DUP_ENTRY
show variables where (f2(17) = 17);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1018);
--error ER_DUP_ENTRY
show charset where (f2(18) = 18);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1019);
--error ER_DUP_ENTRY
show collation where (f2(19) = 19);
select * from t2;
rollback;
select * from t2;

--echo # We need at least one procedure to make sure the WHERE clause is
--echo # evaluated
create procedure dummy() begin end;
insert into t2 (a) values (1020);
--error ER_DUP_ENTRY
show procedure status where (f2(20) = 20);
select * from t2;
rollback;
select * from t2;
drop procedure dummy;

insert into t2 (a) values (1021);
--error ER_DUP_ENTRY
show function status where (f2(21) = 21);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1022);
prepare stmt from "insert into t1 (a) values (f2(22))";
--error ER_DUP_ENTRY
execute stmt;
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1023);
--error ER_DUP_ENTRY
do (f2(23));
select * from t2;
rollback;
select * from t2;

## Please note :
## This will insert a record 1024 in t1 (statement commit)
## This will insert a record 24 in t1 (statement commit)
## then will rollback the second insert only (24) (statement rollback)
## then will rollback the complete transaction (transaction rollback)

delimiter |;

create procedure bug12713_call ()
begin
  insert into t2 (a) values (24);
  insert into t2 (a) values (24);
end|

delimiter ;|

insert into t2 (a) values (1024);
--error ER_DUP_ENTRY
call bug12713_call();
select * from t2;
rollback;
select * from t2;

--echo =======================================================================
--echo Testing select_to_file
--echo =======================================================================

insert into t2 (a) values (1025);

--replace_result $MYSQLTEST_VARDIR ..
--error ER_DUP_ENTRY
eval select f2(25) into outfile "$MYSQLTEST_VARDIR/tmp/dml.out" from t1;
select * from t2;
rollback;
select * from t2;
--remove_file $MYSQLTEST_VARDIR/tmp/dml.out

insert into t2 (a) values (1026);
--replace_result $MYSQLTEST_VARDIR ..
--error ER_DUP_ENTRY
eval load data infile "../../std_data/words.dat" ignore into table t1 (a) set a:=f2(26);

select * from t2;
rollback;
select * from t2;

--echo =======================================================================
--echo Testing select_dumpvar
--echo =======================================================================

insert into t2 (a) values (1027);
--error ER_DUP_ENTRY
select f2(27) into @foo;
select * from t2;
rollback;
select * from t2;

--echo =======================================================================
--echo Testing Select_fetch_into_spvars 
--echo =======================================================================

delimiter |;

create procedure bug12713_dump_spvars ()
begin
  declare foo int;

  declare continue handler for sqlexception
  begin
    select "Exception trapped";
  end;

  select f2(28) into foo;
  select * from t2;
end|

delimiter ;|

insert into t2 (a) values (1028);
call bug12713_dump_spvars ();
rollback;
select * from t2;

--echo =======================================================================
--echo Cleanup
--echo =======================================================================

set autocommit=default;

drop table t1;
drop table t2;
drop table t3;
drop function f2;
drop procedure bug12713_call;
drop procedure bug12713_dump_spvars;
--echo #
--echo # Bug#12713 Error in a stored function called from a SELECT doesn't
--echo # cause ROLLBACK of statem
--echo #
--echo # Verify that two-phase commit is not issued for read-only
--echo # transactions.
--echo #
--echo # Verify that two-phase commit is issued for read-write transactions,
--echo # even if the change is done inside a stored function called from
--echo # SELECT or SHOW statement.
--echo #
set autocommit=0;
--disable_warnings
drop table if exists t1;
drop table if exists t2;
drop function if exists f1;
drop procedure if exists p_verify_status_increment;
--enable_warnings

# Save binlog_format in a user variable. References to system
# variables are "unsafe", meaning they are written as rows instead of
# as statements to the binlog, if the loggging mode is 'mixed'.  But
# we don't want p_verify_status_increment to affect the logging mode.
# Hence, we save binlog_format in a user variable (which is not
# unsafe) and use that inside p_verify_status_increment.
set @binlog_format=@@global.binlog_format;

set sql_mode=no_engine_substitution;
create table t1 (a int unique); 
create table t2 (a int) engine=myisam;
set sql_mode=default;
--echo #
--echo # An auxiliary procedure to track Handler_prepare and Handler_commit
--echo # statistics.
--echo #
delimiter |;
create procedure
p_verify_status_increment(commit_inc_mixed int, prepare_inc_mixed int,
                          commit_inc_row int, prepare_inc_row int)
begin
  declare commit_inc int;
  declare prepare_inc int;
  declare old_commit_count int default ifnull(@commit_count, 0);
  declare old_prepare_count int default ifnull(@prepare_count, 0);
  declare c_res int;
# Use a cursor to have just one access to I_S instead of 2, it is very slow
# and amounts for over 90% of test CPU time
  declare c cursor for
     select variable_value
     from performance_schema.session_status
     where variable_name='Handler_commit' or variable_name='Handler_prepare'
     order by variable_name;

  if @binlog_format = 'ROW' then
    set commit_inc= commit_inc_row;
    set prepare_inc= prepare_inc_row;
  else
    set commit_inc= commit_inc_mixed;
    set prepare_inc= prepare_inc_mixed;
  end if;

  open c;
  fetch c into c_res;
  set @commit_count=c_res;
  fetch c into c_res;
  set @prepare_count=c_res;
  close c;

  if old_commit_count + commit_inc <> @commit_count then
    select concat("Expected commit increment: ", commit_inc,
        " actual: ", @commit_count - old_commit_count)
    as 'ERROR';
  elseif old_prepare_count + prepare_inc <> @prepare_count then
    select concat("Expected prepare increment: ", prepare_inc,
        " actual: ", @prepare_count - old_prepare_count)
    as 'ERROR';
  else
    select '' as 'SUCCESS';
  end if;
end|
delimiter ;|
--enable_query_log
--enable_result_log
--echo # Warm-up data-dictionary cache.
--disable_result_log
--disable_query_log
show create table t1;
show create table t2;
show create procedure p_verify_status_increment;
--enable_query_log
--enable_result_log
--echo # Reset Handler_commit and Handler_prepare counters
flush status;
--echo #
--echo # 1. Read-only statement: SELECT
--echo #
select * from t1;
--disable_warnings
call p_verify_status_increment(1, 0, 2, 0);
commit;
call p_verify_status_increment(1, 0, 1, 0);
--enable_warnings

--echo # 2. Read-write statement: INSERT, insert 1 row. 
--echo #
insert into t1 (a) values (1);
--disable_warnings
call p_verify_status_increment(2, 2, 2, 2);
commit;
call p_verify_status_increment(2, 2, 2, 2);
--enable_warnings

--echo # 3. Read-write statement: UPDATE, update 1 row. 
--echo #
update t1 set a=2;
--disable_warnings
call p_verify_status_increment(2, 2, 2, 2);
commit;
call p_verify_status_increment(2, 2, 2, 2);
--enable_warnings

--echo # 4. Read-write statement: UPDATE, update 0 rows, 1 row matches WHERE 
--echo #
update t1 set a=2;
--disable_warnings
call p_verify_status_increment(2, 2, 1, 0);
commit;
call p_verify_status_increment(2, 2, 1, 0);
--enable_warnings

--echo # 5. Read-write statement: UPDATE, update 0 rows, 0 rows match WHERE 
--echo #
--echo # In mixed replication mode, there is a read-only transaction
--echo # in InnoDB and also the statement is written to the binary log.
--echo # So we have two commits but no 2pc, since the first engine's
--echo # transaction is read-only.
--echo # In the row level replication mode, we only have the read-only
--echo # transaction in InnoDB and nothing is written to the binary log.
--echo #
update t1 set a=3 where a=1;
--disable_warnings
call p_verify_status_increment(2, 0, 1, 0);
commit;
call p_verify_status_increment(2, 0, 1, 0);
--enable_warnings

--echo # 6. Read-write statement: DELETE, delete 0 rows. 
--echo #
delete from t1 where a=1;
--disable_warnings
call p_verify_status_increment(2, 0, 1, 0);
commit;
call p_verify_status_increment(2, 0, 1, 0);
--enable_warnings

--echo # 7. Read-write statement: DELETE, delete 1 row. 
--echo #
delete from t1 where a=2;
--disable_warnings
call p_verify_status_increment(2, 2, 2, 2);
commit;
call p_verify_status_increment(2, 2, 2, 2);
--enable_warnings

--echo # 8. Read-write statement: unqualified DELETE
--echo #
--echo # In statement or mixed replication mode, we call
--echo # handler::ha_delete_all_rows() and write statement text
--echo # to the binary log. This results in two read-write transactions.
--echo # In row level replication mode, we do not call
--echo # handler::ha_delete_all_rows(), but delete rows one by one.
--echo # Since there are no rows, nothing is written to the binary log.
--echo # Thus we have just one read-only transaction in InnoDB.
delete from t1;
--disable_warnings
call p_verify_status_increment(2, 2, 1, 0);
commit;
call p_verify_status_increment(2, 2, 1, 0);
--enable_warnings

--echo # 9. Read-write statement: REPLACE, change 1 row. 
--echo #
replace t1 set a=1;
--disable_warnings
call p_verify_status_increment(2, 2, 2, 2);
commit;
call p_verify_status_increment(2, 2, 2, 2);
--enable_warnings

--echo # 10. Read-write statement: REPLACE, change 0 rows. 
--echo #
replace t1 set a=1;
--disable_warnings
call p_verify_status_increment(2, 2, 1, 0);
commit;
call p_verify_status_increment(2, 2, 1, 0);
--enable_warnings

--echo # 11. Read-write statement: IODKU, change 1 row. 
--echo #
insert t1 set a=1 on duplicate key update a=a+1;
--disable_warnings
call p_verify_status_increment(2, 2, 2, 2);
select * from t1;
call p_verify_status_increment(1, 0, 1, 0);
commit;
call p_verify_status_increment(2, 2, 2, 2);
--enable_warnings

--echo # 12. Read-write statement: IODKU, change 0 rows. 
--echo #
insert t1 set a=2 on duplicate key update a=2;
--disable_warnings
call p_verify_status_increment(2, 2, 1, 0);
commit;
call p_verify_status_increment(2, 2, 1, 0);
--enable_warnings

--echo # 13. Read-write statement: INSERT IGNORE, change 0 rows. 
--echo #
insert ignore t1 set a=2;
--disable_warnings
call p_verify_status_increment(2, 2, 1, 0);
commit;
call p_verify_status_increment(2, 2, 1, 0);
--enable_warnings

--echo # 14. Read-write statement: INSERT IGNORE, change 1 row. 
--echo #
insert ignore t1 set a=1;
--disable_warnings
call p_verify_status_increment(2, 2, 2, 2);
commit;
call p_verify_status_increment(2, 2, 2, 2);
--enable_warnings
--echo # 15. Read-write statement: UPDATE IGNORE, change 0 rows. 
--echo #
--disable_warnings
update ignore t1 set a=2 where a=1;
--enable_warnings
if (`select @@binlog_format = 'STATEMENT'`)
{
  --disable_query_log
  --disable_warnings
  call p_verify_status_increment(2, 2, 1, 0);
  --enable_warnings
  --enable_query_log
}
if (`select @@binlog_format != 'STATEMENT'`)
{
  --disable_query_log
  --disable_warnings
  call p_verify_status_increment(1, 0, 1, 0);
  --enable_warnings
  --enable_query_log
}
commit;
if (`select @@binlog_format = 'STATEMENT'`)
{
  --disable_query_log
  --disable_warnings
  call p_verify_status_increment(2, 2, 1, 0);
  --enable_warnings
  --enable_query_log
}
if (`select @@binlog_format != 'STATEMENT'`)
{
  --disable_query_log
  --disable_warnings
  call p_verify_status_increment(1, 0, 1, 0);
  --enable_warnings
  --enable_query_log
}
--echo #
--echo # Create a stored function that modifies a
--echo # non-transactional table. Demonstrate that changes in
--echo # non-transactional tables do not affect the two phase commit
--echo # algorithm.
--echo #
delimiter |;
create function f1() returns int
begin
  insert t2 set a=2;
  return 2;
end|
delimiter ;|
--disable_warnings
call p_verify_status_increment(6, 4, 2, 0);
--enable_warnings

--echo # 16. A function changes non-trans-table.
--echo #
--echo # For row-based logging, there is an extra commit for the
--echo # non-transactional changes saved in the transaction cache to
--echo # the binary log. 
--echo #
select f1();
--disable_warnings
--echo # Extra commit comes from load of f1 into data-dictionary cache,
call p_verify_status_increment(2, 0, 2, 0);
commit;
call p_verify_status_increment(1, 0, 2, 0);
--enable_warnings

--echo # 17. Read-only statement, a function changes non-trans-table.
--echo #
--echo # For row-based logging, there is an extra commit for the
--echo # non-transactional changes saved in the transaction cache to
--echo # the binary log. 
--echo #
--disable_warnings
select f1() from t1;
--enable_warnings
--disable_warnings
call p_verify_status_increment(2, 0, 2, 0);
commit;
call p_verify_status_increment(2, 0, 2, 0);
--enable_warnings

--echo # 18. Read-write statement: UPDATE, change 0 (transactional) rows. 
--echo #
select count(*) from t2;
--disable_warnings
update t1 set a=2 where a=f1()+10;
--enable_warnings
select count(*) from t2;
--disable_warnings
call p_verify_status_increment(2, 0, 2, 0);
commit;
call p_verify_status_increment(2, 0, 2, 0);
--enable_warnings
--echo #
--echo # Replace the non-transactional table with a temporary
--echo # transactional table. Demonstrate that a change to a temporary
--echo # transactional table does not provoke 2-phase commit, although
--echo # does trigger a commit and a binlog write (in statement mode).
--echo #
drop table t2;
set sql_mode=no_engine_substitution;
create temporary table t2 (a int);
--disable_warnings

let $have_debug = `SELECT VERSION() LIKE '%debug%'`;
if (!$have_debug)
{
  --replace_regex /8/X/
  call p_verify_status_increment(8, 0, 0, 0);
}

if ($have_debug)
{
  --replace_regex /9/X/
  call p_verify_status_increment(9, 0, 0, 0);
}
--enable_warnings
set sql_mode=default;

--echo # 19. A function changes temp-trans-table.
--echo #
select f1();
--echo # Two commits because a binary log record is written
--disable_warnings
call p_verify_status_increment(2, 0, 1, 0);
commit;
call p_verify_status_increment(2, 0, 1, 0);
--enable_warnings

--echo # 20. Read-only statement, a function changes non-trans-table.
--echo #
select f1() from t1;
--echo # Two commits because a binary log record is written
--disable_warnings
call p_verify_status_increment(2, 0, 1, 0);
commit;
call p_verify_status_increment(2, 0, 1, 0);
--enable_warnings

--echo # 21. Read-write statement: UPDATE, change 0 (transactional) rows. 
--echo #
--disable_warnings
update t1 set a=2 where a=f1()+10;
--enable_warnings
--disable_warnings
call p_verify_status_increment(2, 0, 1, 0);
commit;
call p_verify_status_increment(2, 0, 1, 0);
--enable_warnings

--echo # 22. DDL: ALTER TEMPORARY TABLE, should not cause a 2pc
--echo #
alter table t2 add column b int default 5;
--echo # A commit is done internally by ALTER. 
--disable_warnings
call p_verify_status_increment(4, 0, 3, 0);
commit;
--echo # There is nothing left to commit
call p_verify_status_increment(0, 0, 0, 0);
--enable_warnings

--echo # 23. DDL: RENAME TEMPORARY TABLE, does not start a transaction
--echo
--echo # No test because of Bug#8729 "rename table fails on temporary table"

--echo # 24. DDL: TRUNCATE TEMPORARY TABLE
--echo
truncate table t2;
--disable_warnings
call p_verify_status_increment(6, 0, 4, 0);
commit;
--echo # There is nothing left to commit
call p_verify_status_increment(0, 0, 0, 0);
--enable_warnings

--echo # 25. Read-write statement: unqualified DELETE 
--echo
delete from t2;
--disable_warnings
call p_verify_status_increment(2, 0, 1, 0);
commit;
--echo # There is nothing left to commit
call p_verify_status_increment(2, 0, 1, 0);
--enable_warnings

--echo # 25. DDL: DROP TEMPORARY TABLE, does not start a transaction
--echo #
drop temporary table t2;
--disable_warnings
call p_verify_status_increment(1, 0, 1, 0);
commit;
call p_verify_status_increment(1, 0, 1, 0);
--enable_warnings

--echo # 26. Verify that SET AUTOCOMMIT issues an implicit commit
--echo #
insert t1 set a=3;
--disable_warnings
call p_verify_status_increment(2, 2, 2, 2);
set autocommit=1;
call p_verify_status_increment(2, 2, 2, 2);
--enable_warnings
rollback;
select a from t1 where a=3;
--disable_warnings
call p_verify_status_increment(1, 0, 1, 0);
--enable_warnings
delete from t1 where a=3;
--disable_warnings
call p_verify_status_increment(2, 2, 2, 2);
commit;
call p_verify_status_increment(0, 0, 0, 0);
--enable_warnings
set autocommit=0;
--disable_warnings
call p_verify_status_increment(0, 0, 0, 0);
insert t1 set a=3;
call p_verify_status_increment(2, 2, 2, 2);
--enable_warnings
--echo # Sic: not actually changing the value of autocommit
set autocommit=0;
--disable_warnings
call p_verify_status_increment(0, 0, 0, 0);
rollback;
select a from t1 where a=3;
call p_verify_status_increment(1, 0, 1, 0);
--enable_warnings

--echo # 27. Savepoint management
--echo #
insert t1 set a=3;
--disable_warnings
call p_verify_status_increment(2, 2, 2, 2);
savepoint a;
call p_verify_status_increment(1, 0, 1, 0);
insert t1 set a=4;
call p_verify_status_increment(2, 2, 2, 2);
release savepoint a;
rollback;
call p_verify_status_increment(0, 0, 0, 0);
select a from t1 where a=3;
call p_verify_status_increment(1, 0, 1, 0);
commit;
call p_verify_status_increment(1, 0, 1, 0);
--enable_warnings

--echo # 28. Read-write statement: DO
--echo #
create table t2 (a int);
--disable_warnings
# Under ps protocol commit count is increased because of extra commits during
# COM_PREPARE.
if (`SELECT $PS_PROTOCOL = 0`)
{
  --replace_regex /11/<commit_count>/
  call p_verify_status_increment(11, 4, 6, 0);
}
if (`SELECT $PS_PROTOCOL > 0`)
{
  --replace_regex /12/<commit_count>/
  call p_verify_status_increment(12, 4, 6, 0);
}
--echo # One extra transaction to load table 't2' into data-dictionary cache.
do (select f1() from t1 where a=2);
call p_verify_status_increment(4, 2, 3, 2);
commit;
call p_verify_status_increment(2, 2, 2, 2);
--enable_warnings

--echo # 29. Read-write statement: MULTI-DELETE
--echo # 
delete t1, t2 from t1 join t2 on (t1.a=t2.a) where t1.a=2;
commit;
--disable_warnings
call p_verify_status_increment(4, 4, 4, 4);
--enable_warnings

--echo # 30. Read-write statement: INSERT-SELECT, MULTI-UPDATE, REPLACE-SELECT
--echo # 
insert into t2 select a from t1;
commit;
--disable_warnings
replace into t2 select a from t1;
--enable_warnings
commit;
--disable_warnings
call p_verify_status_increment(8, 8, 8, 8);
--enable_warnings
#
# Multi-update is one of the few remaining statements that still
# locks the tables at prepare step (and hence starts the transaction.
# Disable the PS protocol, since in this protocol we get a different
# number of commmits (there is an extra commit after prepare
#
--disable_ps_protocol
update t1, t2 set t1.a=4, t2.a=8 where t1.a=t2.a and t1.a=1;
--enable_ps_protocol
commit;
--disable_warnings
call p_verify_status_increment(4, 4, 4, 4);
--enable_warnings

--echo # 31. DDL: various DDL with transactional tables
--echo #
--echo # Sic: no table is created.
create table if not exists t2 (a int) select 6 union select 7;
--echo # No table are locked before existing t2 is found, so nothing to commit.
--disable_warnings
call p_verify_status_increment(0, 0, 0, 0);
create table t3 select a from t2;
# Under ps protocol commit count is increased because of extra commits during
# COM_PREPARE.
if (`SELECT $PS_PROTOCOL = 0`)
{
 --replace_regex /12/<commit_count>/
  call p_verify_status_increment(12, 4, 4, 4);
}
if (`SELECT $PS_PROTOCOL > 0`)
{
  --replace_regex /13/<commit_count>/
  call p_verify_status_increment(13, 4, 4, 4);
}
alter table t3 add column (b int);

if (!$have_debug)
{
  --replace_regex /19/X/
  call p_verify_status_increment(19, 4, 2, 0);
}

if ($have_debug)
{
  --replace_regex /20/X/
  call p_verify_status_increment(20, 4, 2, 0);
}

alter table t3 rename t4;
call p_verify_status_increment(20, 4, 3, 0);
rename table t4 to t3;
call p_verify_status_increment(24, 4, 5, 0);
truncate table t3;

if (!$have_debug)
{
  --replace_regex /9/X/
  call p_verify_status_increment(9, 4, 4, 0);
}

if ($have_debug)
{
  --replace_regex /10/X/
  call p_verify_status_increment(10, 4, 4, 0);
}

create view v1 as select * from t2;
call p_verify_status_increment(7, 4, 3, 0);
check table t1;
call p_verify_status_increment(2, 0, 2, 0);
--echo # Sic: after this bug is fixed, CHECK leaves no pending transaction
commit;
call p_verify_status_increment(0, 0, 0, 0);
check table t1, t2, t3;
--echo # Extra transactions to load t3 into data-dictionary and table caches
call p_verify_status_increment(9, 0, 6, 0);
commit;
call p_verify_status_increment(0, 0, 0, 0);
drop view v1;

--echo # One extra transaction to load v1 into data-dictionary cache.
if (!$have_debug)
{
  --replace_regex /\(6/(X/
  call p_verify_status_increment(6, 4, 3, 0);
}

if ($have_debug)
{
  --replace_regex /7/X/
  call p_verify_status_increment(7, 4, 3, 0);
}

--enable_warnings

--echo #
--echo # Cleanup
--echo #
drop table t1, t2, t3;
drop procedure p_verify_status_increment;
drop function f1;
