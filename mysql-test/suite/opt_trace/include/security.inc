# Test that trace does not show information forbidden
# by lack of privileges.

--source include/have_optimizer_trace.inc
# Grant tests not performed with embedded server
-- source include/not_embedded.inc

connection default;
let $DEFAULT_TRACE_MEM_SIZE=1048576; # 1MB
set @old_size = @@global.optimizer_trace_max_mem_size;
eval set global optimizer_trace_max_mem_size=$DEFAULT_TRACE_MEM_SIZE;

connection default;
select user();
create database somedb;
use somedb;
create table t1(a varchar(100));
insert into t1 values("first");
create table t2(a varchar(100));
insert into t2 values("first");
create table t3(a varchar(100));
insert into t3 values("first");
delimiter |;
create procedure p1() sql security definer
begin
  declare b int;
  if (select count(*) from t1)
  then
    select 22 into b from dual;
  end if;
  select a into b from t1 limit 1;
  insert into t1 values(current_user());
end|
create function f1() returns int sql security definer
begin
  declare b int;
  select 48 into b from dual;
  select a into b from t1 limit 1;
  insert into t1 values(current_user());
  return 36;
end|
create trigger trg2 before insert on t2 for each row 
begin
  insert into t3 select * from t3;
end|
delimiter ;|
create sql security definer view v1 as select * from t1;
create user user1@localhost identified by '';
grant all on *.* to user1@localhost with grant option;
connect (con_user1, localhost, user1,, somedb);

--echo
connection con_user1;
select user();
set optimizer_trace="enabled=on";
# SHOW GRANTS scans a hash, which gives a random order
--sorted_result
show grants;

--echo
--echo # ==========================================================
--echo # Part A.
--echo # Test that security context changes are allowed when, and only
--echo # when, invoker has all global privileges.
--echo # ==========================================================
--echo

--echo # Because invoker has all global privileges, all traces are visible:
set optimizer_trace_offset=0,optimizer_trace_limit=100;
call p1();
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
--echo # this SET always purges all remembered traces
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select f1();
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
insert into t2 values(current_user());
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # Show that really all global privileges are needed: let root
--echo # revoke just one from user1. Because user1 does not have all global
--echo # privileges anymore, security context changes are forbidden,
--echo # thus there is no trace.
--echo

connection default;
select user();
revoke shutdown on *.* from user1@localhost;
# removing a global privilege never affects an existing connection:
disconnect con_user1;
connect (con_user1, localhost, user1,, somedb);

--echo
connection con_user1;
select user();
set optimizer_trace="enabled=on";
--sorted_result
show grants;

set optimizer_trace_offset=0,optimizer_trace_limit=100;
call p1();
--echo # In CALL we execute stored procedure and notice a security
--echo # context change. The context change is probably only relevant
--echo # for substatements, but we still hide CALL. This is to be
--echo # consistent with what we do when routine body should not be
--echo # exposed. And it also feels safer to disable I_S output as
--echo # soon as possible.
--echo # Ps-protocol-specific note: mysqltest uses normal protocol for CALL
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select f1();
select QUERY, TRACE, INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
insert into t2 values(current_user());
select QUERY, TRACE, INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # Verify that user1 cannot circumvent security checks by
--echo # setting @@optimizer_trace_offset so that I_S output is disabled
--echo # before the object (routine) is checked, and enabled in the
--echo # middle of object usage, when 'offset' is passed.
--echo

set optimizer_trace_offset=2,optimizer_trace_limit=1;
call p1();
--echo # Even though the routine's execution started before
--echo # 'offset', it detected the security context changes. So the
--echo # trace of CALL gets the "missing privilege" mark but we don't
--echo # see it as CALL was before 'offset'.
select QUERY, TRACE, INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # Finally, verify that if the routine's definer does modify
--echo # @@optimizer_trace from "enabled=off" to "enabled=on", in the
--echo # body of the routine, then tracing works. This is no security
--echo # issue, as it was done by the routine's definer.
--echo

connection default;
select user();
delimiter |;
create procedure p2() sql security definer
begin
  declare b int;
  set optimizer_trace="enabled=on";
  select 22 into b from dual;
end|
delimiter ;|

--echo
connection con_user1;
select user();

set optimizer_trace="enabled=off";
set optimizer_trace_offset=0,optimizer_trace_limit=100;
call p2();
select QUERY, TRACE, INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
--echo # Variable is as set by the routine
select @@optimizer_trace;

--echo
--echo # ==========================================================
--echo # Part B.
--echo # Do same tests but with SQL SECURITY INVOKER objects, to verify that
--echo # the restriction on security context changes is not present.
--echo # ==========================================================
--echo

connection default;
select user();
alter procedure p1 sql security invoker;
alter function f1 sql security invoker;
alter sql security invoker view v1 as select * from t1;
--echo # Triggers cannot be SQL SECURITY INVOKER so we don't test
--echo # them here.
alter procedure p2 sql security invoker;
delete from t1 where a<>"first";

--echo
connection con_user1;
select user();

set optimizer_trace_offset=0,optimizer_trace_limit=100;
call p1();
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select f1();
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=2,optimizer_trace_limit=1;
call p1();
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace="enabled=off";
set optimizer_trace_offset=0,optimizer_trace_limit=100;
call p2();
--echo # SELECT substatement is traced (no security context change)
select QUERY, TRACE, INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
select @@optimizer_trace;

--echo
--echo # ==========================================================
--echo # Part C.
--echo # User1 got traces. Determine the minimum set of privileges he
--echo # needed for that.
--echo # ==========================================================
--echo

connection default;
drop procedure p2; # p2 is not worth testing more
select user();
revoke all privileges, grant option from user1@localhost;
--echo # Grant minimum privileges to use the routines and views,
--echo # without considering optimizer trace:
grant execute on procedure p1 to user1@localhost;
grant execute on function f1 to user1@localhost;
grant select (a) on v1 to user1@localhost;
--echo # Objects above are SQL SECURITY INVOKER, so invoker needs
--echo # privileges on objects used internally:
grant select (a) on t1 to user1@localhost;
grant insert (a) on t1 to user1@localhost;
delete from t1 where a<>"first";
disconnect con_user1;
connect (con_user1, localhost, user1,, somedb);

--echo
connection con_user1;
select user();
set optimizer_trace="enabled=on";
--sorted_result
show grants;

--echo
--echo # Those privileges are not enough to see traces:
set optimizer_trace_offset=0,optimizer_trace_limit=100;
call p1();
--echo # In CALL we execute stored procedure and notice that body should
--echo # not be exposed. The trace of this CALL would not expose the
--echo # body. Trace of substatements would. But, due to
--echo # implementation, CALL is hidden.
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select f1();
--echo # SELECT is hidden (same reason as for CALL).
--echo # Ps-protocol-specific note: preparation of SELECT above does not
--echo # execute f1, so does not risk exposing body, so its trace is
--echo # visible.
select QUERY, TRACE, INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
--echo # Cannot see anything as it would expose body of view
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # C.0) Add more privileges:
--echo

connection default;
select user();
--echo # - for use of t1 in routines and view:
grant select on t1 to user1@localhost;
--echo # - for use of routines:
grant select on mysql.proc to user1@localhost;
--echo # - for use of view:
grant select, show view on v1 to user1@localhost;
delete from t1 where a<>"first";

--echo
connection con_user1;
select user();

set optimizer_trace_offset=0,optimizer_trace_limit=100;
call p1();
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
--echo # Trace exposed body of routine, and content of t1, which we
--echo # could see anyway:
show create procedure p1;
select * from t1 limit 1;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select f1();
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
--echo # Trace exposed body of routine, and content of t1, which we
--echo # could see anyway:
show create function f1;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
--echo # Trace exposed body of view, and content of t1, which we
--echo # could see anyway:
show create view v1;

--echo
--echo # Now remove each privilege to verify that it was needed:
--echo # C.1) remove table-level SELECT privilege on t1
--echo
connection default;
select user();
revoke select on t1 from user1@localhost;
grant select (a) on t1 to user1@localhost;
delete from t1 where a<>"first";

--echo
connection con_user1;
select user();

set optimizer_trace_offset=0,optimizer_trace_limit=100;
call p1();
--echo # Cannot see those substatements which use t1
select QUERY, TRACE, INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select f1();
--echo # Cannot see those substatements which use t1
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
--echo # Trace exposed body of routine, which we could see anyway:
set optimizer_trace="enabled=off";
show create function f1;
set optimizer_trace="enabled=on";
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
--echo # Cannot see anything as it might expose some data from columns
--echo # of t1
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # C.2) remove table-level SELECT privilege on mysql.proc
--echo

connection default;
select user();
--echo # Put back privilege removed in C.1
grant select on t1 to user1@localhost;
--echo # And remove a next one:
revoke select on mysql.proc from user1@localhost;
delete from t1 where a<>"first";

--echo
connection con_user1;
select user();

--echo # We have no right to see routines' bodies:
set optimizer_trace="enabled=off";
show create procedure p1;
show create function f1;
--echo # Verify that optimizer trace does not influence the privilege
--echo # checking in SHOW CREATE:
set optimizer_trace="enabled=on";
show create procedure p1;
show create function f1;

set optimizer_trace_offset=0,optimizer_trace_limit=100;
call p1();
--echo # Cannot see anything as it would expose body of routine
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select f1();
select QUERY, TRACE, INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # C.3) remove table-level SELECT privilege on view
--echo

connection default;
select user();
--echo # Put back privilege removed in C.2
grant select on mysql.proc to user1@localhost;
--echo # And remove a next one:
revoke select on v1 from user1@localhost;
grant select (a) on v1 to user1@localhost;
delete from t1 where a<>"first";

--echo
connection con_user1;
select user();

set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
--echo # Cannot see anything as it might expose some data from columns
--echo # of v1
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # C.4) remove SHOW VIEW privilege on view
--echo

connection default;
select user();
--echo # Put back privilege removed in C.3
grant select on v1 to user1@localhost;
--echo # And remove a next one:
revoke show view on v1 from user1@localhost;
delete from t1 where a<>"first";

--echo
connection con_user1;
select user();

set optimizer_trace="enabled=off";
--echo # We have no right to see view's body:
--error ER_TABLEACCESS_DENIED_ERROR
show create view v1;
set optimizer_trace="enabled=on";
--echo # Verify that optimizer trace does not influence the privilege
--echo # checking in SHOW CREATE:
--error ER_TABLEACCESS_DENIED_ERROR
show create view v1;

set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
--echo # Cannot see anything as it would expose body of view
select QUERY, TRACE, INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # ==========================================================
--echo # Part D.
--echo # Like Part C, but instead of SQL SECURITY INVOKER objects
--echo # created by root and used by User1, let's have SQL SECURITY
--echo # DEFINER objects created and used by User1. Determine the
--echo # minimum set of privileges he needs for that.
--echo # ==========================================================
--echo

connection default;
select user();
drop procedure p1;
drop function f1;
drop view v1;
drop trigger trg2;
revoke all privileges, grant option from user1@localhost;
--echo # Grant minimum privileges to create and use objects,
--echo # without considering optimizer trace:
grant create routine on somedb.* to user1@localhost;
grant trigger on t2 to user1@localhost;
grant create view on somedb.* to user1@localhost;
grant select (a) on t1 to user1@localhost;
grant insert (a) on t1 to user1@localhost;
grant insert (a) on t2 to user1@localhost;
grant select (a) on t3 to user1@localhost;
grant insert (a) on t3 to user1@localhost;
delete from t1 where a<>"first";
disconnect con_user1;
connect (con_user1, localhost, user1,, somedb);

--echo
connection con_user1;
select user();
set optimizer_trace="enabled=on";

delimiter |;
create procedure p1() sql security definer
begin
  declare b int;
  if (select count(*) from t1)
  then
    select 22 into b from dual;
  end if;
  select a into b from t1 limit 1;
  insert into t1 values(current_user());
end|
create function f1() returns int sql security definer
begin
  declare b int;
  select 48 into b from dual;
  select a into b from t1 limit 1;
  insert into t1 values(current_user());
  return 36;
end|
create trigger trg2 before insert on t2 for each row 
begin
  insert into t3 select * from t3;
end|
delimiter ;|
create sql security definer view v1 as select * from t1;

--echo # Creating a view is not enough to be able to SELECT it...
connection default;
select user();
grant select (a) on v1 to user1@localhost;

--echo
connection con_user1;
select user();

--echo # Those privileges are not enough to see traces:
set optimizer_trace_offset=0,optimizer_trace_limit=100;
call p1();
--echo # Can see body of routine (as definer), but not statements using t1
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select f1();
--echo # Can see body of routine (as definer), but not statements using t1
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
show create function f1;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
--echo # Cannot see anything as it might expose some data from columns
--echo # of t1
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
insert into t2 values(current_user());
--echo # Cannot see anything as it might expose some data from
--echo # columns of t2
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
--echo # Also test a query accessing t1 in FROM clause:
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select a from (select a from t1 where a like "f%") as tt where a like "fi%";
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # D.0) Add more privileges:
--echo

connection default;
select user();
--echo # - for use of t1 in routines and view:
grant select on t1 to user1@localhost;
--echo # - for use of view:
grant select, show view on v1 to user1@localhost;
--echo # - for use of trigger
grant select on t2 to user1@localhost;
grant select on t3 to user1@localhost;
delete from t1 where a<>"first";

--echo
connection con_user1;
select user();

set optimizer_trace_offset=0,optimizer_trace_limit=100;
call p1();
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
--echo # Trace exposed body of routine, and content of t1, which we
--echo # could see anyway:
show create procedure p1;
select * from t1 limit 1;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select f1();
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
--echo # Trace exposed body of routine, and content of t1, which we
--echo # could see anyway:
show create function f1;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
--echo # Trace exposed body of view, and content of t1, which we
--echo # could see anyway:
show create view v1;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
insert into t2 values(current_user());
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;
--echo # Trace exposed body of trigger, and content of t2/t3, which we
--echo # could see anyway:
show create trigger trg2;
select * from t2, t3 limit 1;
--echo # Trace exposed content of t1 which we could see anyway:
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select a from (select a from t1 where a like "f%") as tt where a like "fi%";
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # For routines, as they only use t1 and we added only one
--echo # privilege on t1, we have nothing to remove.
--echo
--echo # Now remove each privilege to verify that it was needed for
--echo # the view.
--echo # D.1) remove table-level SELECT privilege on v1
--echo

connection default;
select user();

revoke select on v1 from user1@localhost;
grant select (a) on v1 to user1@localhost;

--echo
connection con_user1;
select user();

set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
--echo # Cannot see anything as it might expose some data from columns
--echo # of v1
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # D.2) remove table-level SHOW VIEW privilege on v1
--echo

connection default;
select user();

--echo # Put back privilege removed in D.1
grant select on v1 to user1@localhost;
--echo # And remove a next one:
revoke show view on v1 from user1@localhost;

--echo
connection con_user1;
select user();

--echo # We have no right to see view's body:
--error ER_TABLEACCESS_DENIED_ERROR
show create view v1;
set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
--echo # Cannot see anything as it would expose body of view
select QUERY, TRACE, INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # D.3) remove table-level SELECT privilege on t1
--echo

connection default;
select user();

--echo # Put back privilege removed in D.2
grant show view on v1 to user1@localhost;
--echo # And remove a next one:
revoke select on t1 from user1@localhost;
grant select (a) on t1 to user1@localhost;

--echo
connection con_user1;
select user();

set optimizer_trace_offset=0,optimizer_trace_limit=100;
select * from v1;
--echo # Cannot see anything as it might expose some data from columns
--echo # of t1
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # Now remove each privilege to verify that it was needed for
--echo # the trigger:
--echo # D.4) remove table-level SELECT privilege on t2
--echo

connection default;
select user();

revoke select on t2 from user1@localhost;
grant select (a) on t2 to user1@localhost;

--echo
connection con_user1;
select user();

set optimizer_trace_offset=0,optimizer_trace_limit=100;
insert into t2 values(current_user());
--echo # Cannot see anything as it might expose some data from
--echo # columns of t2
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # D.5) remove table-level SELECT privilege on t3
--echo

--echo
connection default;
select user();

--echo # Put back privilege removed in D.4
grant select on t2 to user1@localhost;
--echo # And remove a next one:
revoke select on t3 from user1@localhost;
grant select (a) on t3 to user1@localhost;

--echo
connection con_user1;
select user();

set optimizer_trace_offset=0,optimizer_trace_limit=100;
insert into t2 values(current_user());
--echo # Cannot see substatement as it might expose some data from
--echo # columns of t3
select QUERY, length(TRACE), INSUFFICIENT_PRIVILEGES from information_schema.OPTIMIZER_TRACE;

--echo
--echo # Cleanup
connection default;
select user();
drop user user1@localhost;
disconnect con_user1;

--echo
--echo # ==========================================================
--echo # Part E.
--echo # Misc tests.
--echo # ==========================================================
--echo

connection default;
select user();
drop view v1;
create sql security definer view v1 as select * from t1 where 'secret';
create user user1@localhost identified by '';
grant create, insert, select on somedb.* to user1@localhost;
grant create routine on somedb.* to user1@localhost;
connect (con_user1, localhost, user1,, somedb);

--echo
connection con_user1;
select user();

--echo user1 cannot see view's body:
--error 1142
show create view v1;

--echo user1 creates a procedure
delimiter |;
create procedure proc() sql security definer
begin
  set optimizer_trace="enabled=on";
  set optimizer_trace_offset=0,optimizer_trace_limit=100;
  select * from v1 limit 0;
  create table leak select * from information_schema.optimizer_trace;
  set optimizer_trace="enabled=off";
end|
delimiter ;|

connection default;
select user();

--echo root runs procedure, without fear of risk as it is SQL SECURITY DEFINER
call proc();

--echo
connection con_user1;
select user();
--echo user1 cannot see view's body:
select * from leak;

--echo
--echo # Cleanup
connection default;
select user();
drop database somedb;
drop user user1@localhost;
set @@global.optimizer_trace_max_mem_size = @old_size;
