#
# Metadata lock handling for stored procedures and
# functions.
#

# Thread stack overrun in debug mode on sparc
--source include/not_sparc_debug.inc

--echo # 
--echo # Test coverage for changes performed by the fix
--echo # for Bug#30977 "Concurrent statement using stored function
--echo # and DROP FUNCTION breaks SBR.
--echo #

--echo #
--echo # 1) Verify that the preceding transaction is
--echo # (implicitly) committed  before CREATE/ALTER/DROP
--echo # PROCEDURE. Note, that this is already tested
--echo # in implicit_commit.test, but here we use an alternative
--echo # approach.
--echo #

--echo # Start a transaction, create a savepoint, 
--echo # then call a DDL operation on a procedure, and then check
--echo # that the savepoint is no longer present.

--disable_warnings
drop table if exists t1;
drop procedure if exists p1;
drop procedure if exists p2;
drop procedure if exists p3;
drop procedure if exists p4;
drop function if exists f1;
--enable_warnings
create table t1 (a int);
--echo #
--echo # Test 'CREATE PROCEDURE'.
--echo #
begin;
savepoint sv;
create procedure p1() begin end;
--error ER_SP_DOES_NOT_EXIST
rollback to savepoint sv;
--echo #
--echo # Test 'ALTER PROCEDURE'.
--echo #
begin;
savepoint sv;
alter procedure p1 comment 'changed comment';
--error ER_SP_DOES_NOT_EXIST
rollback to savepoint sv;
--echo #
--echo # Test 'DROP PROCEDURE'.
--echo #
begin;
savepoint sv;
drop procedure p1;
--error ER_SP_DOES_NOT_EXIST
rollback to savepoint sv;
--echo #
--echo # Test 'CREATE FUNCTION'.
--echo #
begin;
savepoint sv;
create function f1() returns int return 1; 
--error ER_SP_DOES_NOT_EXIST
rollback to savepoint sv;
--echo #
--echo # Test 'ALTER FUNCTION'.
--echo #
begin;
savepoint sv;
alter function f1 comment 'new comment';
--error ER_SP_DOES_NOT_EXIST
rollback to savepoint sv;
--echo #
--echo # Test 'DROP FUNCTION'.
--echo #
begin;
savepoint sv;
drop function f1;
--error ER_SP_DOES_NOT_EXIST
rollback to savepoint sv;

--echo #
--echo # 2) Verify that procedure DDL operations fail
--echo # under lock tables. 
--echo #
--echo # Auxiliary routines to test ALTER.
create procedure p1() begin end;
create function f1() returns int return 1;

lock table t1 write;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
create procedure p2() begin end;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
alter procedure p1 comment 'changed comment';
--error ER_LOCK_OR_ACTIVE_TRANSACTION
drop procedure p1;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
create function f2() returns int return 1;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
alter function f1 comment 'changed comment';
lock table t1 read;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
create procedure p2() begin end;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
alter procedure p1 comment 'changed comment';
--error ER_LOCK_OR_ACTIVE_TRANSACTION
drop procedure p1;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
create function f2() returns int return 1;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
alter function f1 comment 'changed comment';
unlock tables;
--echo #
--echo # Even if we locked a temporary table.
--echo # Todo: this is a restriction we could possibly lift.
--echo #
drop table t1;
create temporary table t1 (a int);
lock table t1 read;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
create procedure p2() begin end;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
alter procedure p1 comment 'changed comment';
--error ER_LOCK_OR_ACTIVE_TRANSACTION
drop procedure p1;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
create function f2() returns int return 1;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
alter function f1 comment 'changed comment';
unlock tables;

drop function f1;
drop procedure p1;
drop temporary table t1;

--echo #
--echo # 3) Verify that CREATE/ALTER/DROP routine grab an
--echo # exclusive lock.
--echo #
--echo # For that, start a transaction, use a routine. In a concurrent
--echo # connection, try to drop or alter the routine. It should place
--echo # a pending or exclusive lock and block. In another concurrnet
--echo # connection, try to use the routine.
--echo # That should block on the pending exclusive lock.
--echo #
--echo # Establish helper connections.
connect(con1, localhost, root,,);
connect(con2, localhost, root,,);
connect(con3, localhost, root,,);

--echo #
--echo # Test DROP PROCEDURE.
--echo #
--echo # --> connection default
connection default;
create procedure p1() begin end;
delimiter |;
create function f1() returns int 
begin
  call p1();
  return 1;
end|
delimiter ;|
begin;
select f1();
--echo # --> connection con1
connection con1;
--echo # Sending 'drop procedure p1'...
send drop procedure p1;
--echo # --> connection con2 
connection con2;
--echo # Waiting for 'drop procedure t1' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored procedure metadata lock' and
info='drop procedure p1';
--source include/wait_condition.inc
--echo # Demonstrate that there is a pending exclusive lock.
--echo # Sending 'select f1()'...
send select f1();
--echo # --> connection con3
connection con3;
--echo # Waiting for 'select f1()' to get blocked by a pending MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored procedure metadata lock' and info='select f1()';
--echo # --> connection default
connection default;
commit;
--echo # --> connection con1
connection con1;
--echo # Reaping 'drop procedure p1'...
reap;
--echo # --> connection con2
connection con2;
--echo # Reaping 'select f1()'
--error ER_SP_DOES_NOT_EXIST
reap;
--echo # --> connection default
connection default;

--echo #
--echo # Test CREATE PROCEDURE.
--echo #
create procedure p1() begin end;
begin;
select f1();
--echo # --> connection con1
connection con1;
--echo # Sending 'create procedure p1'...
send create procedure p1() begin end;
--echo # --> connection con2 
connection con2;
--echo # Waiting for 'create procedure t1' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored procedure metadata lock' and
info='create procedure p1() begin end';
--source include/wait_condition.inc
--echo # Demonstrate that there is a pending exclusive lock.
--echo # Sending 'select f1()'...
send select f1();
--echo # --> connection con3
connection con3;
--echo # Waiting for 'select f1()' to get blocked by a pending MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored procedure metadata lock' and info='select f1()';
--echo # --> connection default
connection default;
commit;
--echo # --> connection con1
connection con1;
--echo # Reaping 'create procedure p1'...
--error ER_SP_ALREADY_EXISTS
reap;
--echo # --> connection con2
connection con2;
--echo # Reaping 'select f1()'
reap;
connection default;

--echo # 
--echo # Test ALTER PROCEDURE.
--echo #
begin;
select f1();
--echo # --> connection con1
connection con1;
--echo # Sending 'alter procedure p1'...
send alter procedure p1 contains sql;
--echo # --> connection con2 
connection con2;
--echo # Waiting for 'alter procedure t1' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored procedure metadata lock' and
info='alter procedure p1 contains sql';
--source include/wait_condition.inc
--echo # Demonstrate that there is a pending exclusive lock.
--echo # Sending 'select f1()'...
send select f1();
--echo # --> connection con3
connection con3;
--echo # Waiting for 'select f1()' to get blocked by a pending MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored procedure metadata lock' and info='select f1()';
--echo # --> connection default
connection default;
commit;
--echo # --> connection con1
connection con1;
--echo # Reaping 'alter procedure p1'...
reap;
--echo # --> connection con2
connection con2;
--echo # Reaping 'select f1()'
reap;
--echo # --> connection default
connection default;

--echo #
--echo # Test DROP FUNCTION.
--echo #
begin;
select f1();
--echo # --> connection con1
connection con1;
--echo # Sending 'drop function f1'...
send drop function f1;
--echo # --> connection con2 
connection con2;
--echo # Waiting for 'drop function f1' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and
info='drop function f1';
--source include/wait_condition.inc
--echo # Demonstrate that there is a pending exclusive lock.
--echo # Sending 'select f1()'...
send select f1();
--echo # --> connection con3
connection con3;
--echo # Waiting for 'select f1()' to get blocked by a pending MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and info='select f1()';
--echo # --> connection default
connection default;
commit;
--echo # --> connection con1
connection con1;
--echo # Reaping 'drop function f1'...
reap;
--echo # --> connection con2
connection con2;
--echo # Reaping 'select f1()'
--error ER_SP_DOES_NOT_EXIST
reap;
--echo # --> connection default
connection default;

--echo #
--echo # Test CREATE FUNCTION.
--echo #
create function f1() returns int return 1;
begin;
select f1();
--echo # --> connection con1
connection con1;
--echo # Sending 'create function f1'...
send create function f1() returns int return 2;
--echo # --> connection con2 
connection con2;
--echo # Waiting for 'create function f1' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and
info='create function f1() returns int return 2';
--source include/wait_condition.inc
--echo # Demonstrate that there is a pending exclusive lock.
--echo # Sending 'select f1()'...
send select f1();
--echo # --> connection con3
connection con3;
--echo # Waiting for 'select f1()' to get blocked by a pending MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and info='select f1()';
--echo # --> connection default
connection default;
commit;
--echo # --> connection con1
connection con1;
--echo # Reaping 'create function f1'...
--error ER_SP_ALREADY_EXISTS
reap;
--echo # --> connection con2
connection con2;
--echo # Reaping 'select f1()'
reap;
--echo # --> connection default
connection default;

--echo # 
--echo # Test ALTER FUNCTION.
--echo # 
begin;
select f1();
--echo # --> connection con1
connection con1;
--echo # Sending 'alter function f1'...
send alter function f1 contains sql;
--echo # --> connection con2 
connection con2;
--echo # Waiting for 'alter function f1' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and
info='alter function f1 contains sql';
--source include/wait_condition.inc
--echo # Demonstrate that there is a pending exclusive lock.
--echo # Sending 'select f1()'...
send select f1();
--echo # --> connection con3
connection con3;
--echo # Waiting for 'select f1()' to get blocked by a pending MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and info='select f1()';
--echo # --> connection default
connection default;
commit;
--echo # --> connection con1
connection con1;
--echo # Reaping 'alter function f1'...
reap;
--echo # --> connection con2
connection con2;
--echo # Reaping 'select f1()'
reap;
--echo # --> connection default
connection default;
drop function f1;
drop procedure p1;

--echo #
--echo # 5) Locks should be taken on routines
--echo # used indirectly by views or triggers.
--echo #
--echo #
--echo # A function is used from a trigger.
--echo #
create function f1() returns int return 1;
create table t1 (a int);
create table t2 (a int, b int);
create trigger t1_ai after insert on t1 for each row
  insert into t2 (a, b) values (new.a, f1());
begin;
insert into t1 (a) values (1);
--echo # --> connection con1
connection con1;
--echo # Sending 'drop function f1'
send drop function f1;
--echo # --> connection con2 
connection con2;
--echo # Waiting for 'drop function f1' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and
info='drop function f1';
--source include/wait_condition.inc
--echo # --> connnection default
connection default;
commit;
--echo # --> connection con1
connection con1;
--echo # Reaping 'drop function f1'...
reap;
--echo # --> connection default
connection default;
--echo #
--echo # A function is used from a view.
--echo #
create function f1() returns int return 1;
create view v1 as select f1() as a;
begin;
select * from v1;
--echo # --> connection con1
connection con1;
--echo # Sending 'drop function f1'
send drop function f1;
--echo # --> connection con2 
connection con2;
--echo # Waiting for 'drop function f1' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and
info='drop function f1';
--source include/wait_condition.inc
--echo # --> connnection default
connection default;
commit;
--echo # --> connection con1
connection con1;
--echo # Reaping 'drop function f1'...
reap;
--echo # --> connection default
connection default;
--echo #
--echo # A procedure is used from a function.
--echo #
delimiter |;
create function f1() returns int
begin
  declare v_out int;
  call p1(v_out);
  return v_out;
end|
delimiter ;|
create procedure p1(out v_out int) set v_out=3;
begin;
select * from v1;
--echo # --> connection con1
connection con1;
--echo # Sending 'drop procedure p1'
send drop procedure p1;
--echo # --> connection con2 
connection con2;
--echo # Waiting for 'drop procedure p1' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored procedure metadata lock' and
info='drop procedure p1';
--source include/wait_condition.inc
--echo # --> connnection default
connection default;
commit;
--echo # --> connection con1
connection con1;
--echo # Reaping 'drop procedure p1'...
reap;
--echo # --> connection default
connection default;

--echo #
--echo # Deep nesting: a function is used from a procedure used
--echo # from a function used from a view used in a trigger.
--echo #
create function f2() returns int return 4;
create procedure p1(out v_out int) set v_out=f2();
drop trigger t1_ai;
create trigger t1_ai after insert on t1 for each row
  insert into t2 (a, b) values (new.a, (select max(a) from v1));
begin;
insert into t1 (a) values (3);
--echo # --> connection con1
connection con1;
--echo # Sending 'drop function f2'
send drop function f2;
--echo # --> connection con2 
connection con2;
--echo # Waiting for 'drop function f2' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and
info='drop function f2';
--source include/wait_condition.inc
--echo # --> connnection default
connection default;
commit;
--echo # --> connection con1
connection con1;
--echo # Reaping 'drop function f2'...
reap;
--echo # --> connection default
connection default;

drop view v1;
drop function f1;
drop procedure p1;
drop table t1, t2;

--echo #
--echo # 6) Check that ER_LOCK_DEADLOCK is reported if 
--echo # acquisition of a shared lock fails during a transaction or
--echo # we need to back off to flush the sp cache.
--echo #
--echo # Sic: now this situation does not require a back off since we
--echo # flush the cache on the fly.
--echo #
create function f1() returns int return 7;
create table t1 (a int);
begin;
select * from t1;
# Used to have a back-off here, with optional ER_LOCK_DEADLOCK
#--error ER_LOCK_DEADLOCK
select f1();
commit;
drop table t1;
drop function f1;

--echo #
--echo # 7) Demonstrate that under LOCK TABLES we accumulate locks
--echo # on stored routines, and release metadata locks in 
--echo # ROLLBACK TO SAVEPOINT. That is done only for those stored
--echo # routines that are not part of LOCK TABLES prelocking list.
--echo # Those stored routines that are part of LOCK TABLES
--echo # prelocking list are implicitly locked when entering
--echo # LOCK TABLES, and ROLLBACK TO SAVEPOINT has no effect on
--echo # them.
--echo #
create function f1() returns varchar(20) return "f1()";
create function f2() returns varchar(20) return "f2()";
create view v1 as select f1() as a;
set @@session.autocommit=0;
lock table v1 read;
select * from v1;
savepoint sv;
select f2();
--echo # --> connection con1
connection con1;
--echo # Sending 'drop function f1'...
send drop function f1;
--echo # --> connection con2
connection con2;
--echo # Waiting for 'drop function f1' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and
info='drop function f1';
--source include/wait_condition.inc
--echo # Sending 'drop function f2'...
send drop function f2;
--echo # --> connection default
connection default;
--echo # Waiting for 'drop function f2' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and
info='drop function f2';
--source include/wait_condition.inc
rollback to savepoint sv;
--echo # --> connection con2
connection con2;
--echo # Reaping 'drop function f2'...
reap;
--echo # --> connection default
connection default;
unlock tables;
--echo # --> connection con1
connection con1;
--echo # Reaping 'drop function f1'...
reap;
--echo # --> connection default
connection default;
--error ER_SP_DOES_NOT_EXIST
drop function f1;
--error ER_SP_DOES_NOT_EXIST
drop function f2;
drop view v1;
set @@session.autocommit=default;

--echo # 
--echo # 8) Check the situation when we're preparing or executing a
--echo # prepared statement, and as part of that try to flush the
--echo # session sp cache. However, one of the procedures that
--echo # needs a flush is in use. Verify that there is no infinite
--echo # reprepare loop and no crash. 
--echo #
create function f1() returns int return 1;
delimiter |;
--echo # 
--echo # We just mention p1() in the body of f2() to make
--echo # sure that p1() metadata is validated when validating
--echo # 'select f2()'.
--echo # Recursion is not allowed in stored functions, so 
--echo # an attempt to just invoke p1() from f2() which is in turn
--echo # called from p1() would have given a run-time error.
--echo #
create function f2() returns int
begin
  if @var is null then
    call p1();
  end if;
  return 1;
end|
create procedure p1()
begin
  select f1() into @var;
  execute stmt;
end|
delimiter ;|
--echo # --> connection con2
connection con2;
prepare stmt from "select f2()";
--echo # --> connection default
connection default;
begin;
select f1();
--echo # --> connection con1
connection con1;
--echo # Sending 'alter function f1 ...'...
send alter function f1 comment "comment";
--echo # --> connection con2
connection con2;
--echo # Waiting for 'alter function f1 ...' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and
info like 'alter function f1 comment%';
--source include/wait_condition.inc
--echo # Sending 'call p1()'...
send call p1();
connection default;
--echo # Waiting for 'call p1()' to get blocked on MDL lock on f1...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and
info='select f1() into @var';
--source include/wait_condition.inc
--echo # Let 'alter function f1 ...' go through...
commit;
--echo # --> connection con1
connection con1;
--echo # Reaping 'alter function f1 ...'
reap;
--echo # --> connection con2
connection con2;
--echo # Reaping 'call p1()'...
reap;
deallocate prepare stmt;
--echo # --> connection default
connection default;
drop function f1;
drop function f2;
drop procedure p1;

--echo # 
--echo # 9) Check the situation when a stored function is invoked
--echo # from a stored procedure, and recursively invokes the
--echo # stored procedure that is in use. But for the second
--echo # invocation, a cache flush is requested. We can't
--echo # flush the procedure that's in use, and are forced
--echo # to use an old version. It is not a violation of
--echo # consistency, since we unroll top-level calls.
--echo # Just verify the code works.
--echo #
create function f1() returns int return 1;
begin;
select f1();
--echo # --> connection con1
connection con1;
--echo # Sending 'alter function f1 ...'...
send alter function f1 comment "comment";
--echo # --> connection con2
connection con2;
--echo # Waiting for 'alter function f1 ...' to get blocked on MDL lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and
info like 'alter function f1 comment%';
--source include/wait_condition.inc
delimiter |;
--echo # 
--echo # We just mention p1() in the body of f2() to make
--echo # sure that p1() is prelocked for f2().
--echo # Recursion is not allowed in stored functions, so 
--echo # an attempt to just invoke p1() from f2() which is in turn
--echo # called from p1() would have given a run-time error.
--echo #
create function f2() returns int
begin
  if @var is null then
    call p1();
  end if;
  return 1;
end|
create procedure p1()
begin
  select f1() into @var;
  select f2() into @var;
end|
delimiter ;|
--echo # Sending 'call p1()'...
send call p1();
connection default;
--echo # Waiting for 'call p1()' to get blocked on MDL lock on f1...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='Waiting for stored function metadata lock' and
info='select f1() into @var';
--source include/wait_condition.inc
--echo # Let 'alter function f1 ...' go through...
commit;
--echo # --> connection con1
connection con1;
--echo # Reaping 'alter function f1 ...'
reap;
--echo # --> connection con2
connection con2;
--echo # Reaping 'call p1()'...
reap;
--echo # --> connection default
connection default;
drop function f1;
drop function f2;
drop procedure p1;

--echo #
--echo # 10) A select from information_schema.routines now
--echo # flushes the stored routines caches. Test that this
--echo # does not remove from the cache a stored routine
--echo # that is already prelocked.
--echo #
create function f1() returns int return get_lock("30977", 100000);
create function f2() returns int return 2;
delimiter |;
create function f3() returns varchar(255) 
begin
  declare res varchar(255);
  declare c cursor for select routine_name from
    information_schema.routines where routine_name='f1';
  select f1() into @var;
  open c;
  fetch c into res;
  close c;
  select f2() into @var;
  return res;
end|
delimiter ;|
--echo # --> connection con1
connection con1;
select get_lock("30977", 0);
--echo # --> connection default
connection default;
--echo # Sending 'select f3()'...
send select f3();
--echo # --> connection con1 
connection con1;
--echo # Waiting for 'select f3()' to get blocked on the user level lock...
let $wait_condition=select count(*)=1 from information_schema.processlist
where state='User lock' and info='select f1() into @var';
--source include/wait_condition.inc
--echo # Do something to change the cache version.
create function f4() returns int return  4;
drop function f4;
select release_lock("30977");
--echo # --> connection default 
connection default;
--echo # Reaping 'select f3()'...
--echo # Routine 'f2()' should exist and get executed successfully.
reap;
select @var;
drop function f1;
drop function f2;
drop function f3;


--echo # 11) Check the situation when the connection is flushing the
--echo # SP cache which contains a procedure that is being executed.
--echo #
--echo # Function f1() calls p1(). Procedure p1() has a DROP
--echo # VIEW statement, which, we know, invalidates the routines cache.
--echo # During cache flush p1() must not be flushed since it's in
--echo # use.
--echo #
delimiter |;
create function f1() returns int
begin
  call p1();
  return 1;
end|
create procedure p1() 
begin
  create view v1 as select 1;
  drop view v1;
  select f1() into @var;
  set @exec_count=@exec_count+1;
end|
delimiter ;|
set @exec_count=0;
--error ER_SP_RECURSION_LIMIT
call p1();
select @exec_count;
set @@session.max_sp_recursion_depth=5;
set @exec_count=0;
--error ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG
call p1();
select @exec_count;
drop procedure p1;
drop function f1;
set @@session.max_sp_recursion_depth=default;

--echo # --> connection con1
connection con1;
disconnect con1;
--source include/wait_until_disconnected.inc
--echo # --> connection con2
connection con2;
disconnect con2;
--source include/wait_until_disconnected.inc
--echo # --> connection con3
connection con3;
disconnect con3;
--source include/wait_until_disconnected.inc
--echo # --> connection default
connection default;


--echo #
--echo # SHOW CREATE PROCEDURE p1 called from p1, after p1 was altered
--echo #
--echo # We are just covering the existing behaviour with tests. The
--echo # results are not necessarily correct."
--echo #

delimiter |;
CREATE PROCEDURE p1()
BEGIN
  SHOW CREATE PROCEDURE p1;
  SELECT get_lock("test", 100000);
  SHOW CREATE PROCEDURE p1;
END|
delimiter ;|

connect (con2, localhost, root);
connect (con3, localhost, root);

--echo # Connection default
connection default;
SELECT get_lock("test", 10);

--echo # Connection 2
connection con2;
--echo # Will halt before executing SHOW CREATE PROCEDURE p1
--echo # Sending:
--send CALL p1()

--echo # Connection 3
connection con3;
let $wait_condition=SELECT COUNT(*)=1 FROM information_schema.processlist 
  WHERE state='User lock' and info='SELECT get_lock("test", 100000)';
--source include/wait_condition.inc
--echo # Alter p1
DROP PROCEDURE p1;
CREATE PROCEDURE p1() BEGIN END;

--echo # Connection default
connection default;
--echo # Resume CALL p1, now with new p1
SELECT release_lock("test");

--echo # Connection 2
connection con2;
--echo # Reaping: CALL p1()
--reap

--echo # Connection 3
connection con3;
disconnect con3;
--source include/wait_until_disconnected.inc
--echo # Connection 2
connection con2;
disconnect con2;
--source include/wait_until_disconnected.inc
--echo # Connection default;
connection default;
DROP PROCEDURE p1;


--echo #
--echo # Bug#57663 Concurrent statement using stored function and DROP DATABASE
--echo #           breaks SBR
--echo #

--disable_warnings
DROP DATABASE IF EXISTS db1;
DROP FUNCTION IF EXISTS f1;
--enable_warnings

connect(con1, localhost, root);
connect(con2, localhost, root);

--echo # Test 1: Check that DROP DATABASE block if a function is used
--echo #         by an active transaction.

--echo # Connection default
connection default;
CREATE DATABASE db1;
CREATE FUNCTION db1.f1() RETURNS INTEGER RETURN 1;
START TRANSACTION;
SELECT db1.f1();

--echo # Connection con1
connection con1;
--echo # Sending:
--send DROP DATABASE db1

--echo # Connection default
connection default;
--echo # Waiting for DROP DATABASE to be blocked by the lock on f1()
let $wait_condition= SELECT COUNT(*)= 1 FROM information_schema.processlist
  WHERE state= 'Waiting for stored function metadata lock'
  AND info='DROP DATABASE db1';
--source include/wait_condition.inc
COMMIT;

--echo # Connection con1
connection con1;
--echo # Reaping: DROP DATABASE db1
--reap

--echo # Test 2: Check that DROP DATABASE blocks if a procedure is
--echo #         used by an active transaction.

--echo # Connection default
connection default;
CREATE DATABASE db1;
CREATE PROCEDURE db1.p1() BEGIN END;
delimiter |;
CREATE FUNCTION f1() RETURNS INTEGER
BEGIN
  CALL db1.p1();
  RETURN 1;
END|
delimiter ;|
START TRANSACTION;
SELECT f1();

--echo # Connection con1
connection con1;
--echo # Sending:
--send DROP DATABASE db1

--echo # Connection default
connection default;
--echo # Waiting for DROP DATABASE to be blocked by the lock on p1()
let $wait_condition= SELECT COUNT(*)= 1 FROM information_schema.processlist
  WHERE state= 'Waiting for stored procedure metadata lock'
  AND info='DROP DATABASE db1';
--source include/wait_condition.inc
COMMIT;

--echo # Connection con1
connection con1;
--echo # Reaping: DROP DATABASE db1
--reap

--echo # Test 3: Check that DROP DATABASE is not selected as a victim if a
--echo #         deadlock is discovered with DML statements.

--echo # Connection default
connection default;
CREATE DATABASE db1;
CREATE TABLE db1.t1 (a INT);
CREATE FUNCTION db1.f1() RETURNS INTEGER RETURN 1;
START TRANSACTION;
# DROP DATABASE will lock tables (t1) before functions (f1)
SELECT db1.f1();

--echo # Connection con1
connection con1;
--echo # Sending:
--send DROP DATABASE db1

--echo # Connection default
connection default;
--echo # Waiting for DROP DATABASE to be blocked by the lock on f1()
let $wait_condition= SELECT COUNT(*)= 1 FROM information_schema.processlist
  WHERE state= 'Waiting for stored function metadata lock'
  AND info='DROP DATABASE db1';
--source include/wait_condition.inc
--error ER_LOCK_DEADLOCK
SELECT * FROM db1.t1;
COMMIT;

--echo # Connection con1
connection con1;
--echo # Reaping: DROP DATABASE db1
--reap

--echo # Test 4: Check that active DROP DATABASE blocks stored routine DDL.

--echo # Connection default
connection default;
CREATE DATABASE db1;
CREATE FUNCTION db1.f1() RETURNS INTEGER RETURN 1;
CREATE FUNCTION db1.f2() RETURNS INTEGER RETURN 2;
START TRANSACTION;
SELECT db1.f2();

--echo # Connection con1
connection con1;
--echo # Sending:
--send DROP DATABASE db1

--echo # Connection con2
connection con2;
--echo # Waiting for DROP DATABASE to be blocked by the lock on f2()
let $wait_condition= SELECT COUNT(*)= 1 FROM information_schema.processlist
  WHERE state= 'Waiting for stored function metadata lock'
  AND info='DROP DATABASE db1';
--source include/wait_condition.inc
--echo # Sending:
--send ALTER FUNCTION db1.f1 COMMENT "test"

--echo # Connection default
connection default;
--echo # Waiting for ALTER FUNCTION to be blocked by the schema lock on db1
let $wait_condition= SELECT COUNT(*)= 1 FROM information_schema.processlist
  WHERE state= 'Waiting for schema metadata lock'
  AND info='ALTER FUNCTION db1.f1 COMMENT "test"';
--source include/wait_condition.inc
COMMIT;

--echo # Connection con1
connection con1;
--echo # Reaping: DROP DATABASE db1
--reap
disconnect con1;
--source include/wait_until_disconnected.inc

--echo # Connection con2
connection con2;
--echo # Reaping: ALTER FUNCTION f1 COMMENT 'test'
--error ER_SP_DOES_NOT_EXIST
--reap
disconnect con2;
--source include/wait_until_disconnected.inc

--echo # Connection default
connection default;
DROP FUNCTION f1;


--echo #
--echo # End of 5.5 tests
--echo #
