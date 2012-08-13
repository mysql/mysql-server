# Basic test for optimizer trace

--source include/have_optimizer_trace.inc

# This test gives different results when ps-protocol is enabled
# because in ps-protocol, we read OPTIMIZER_TRACE after
# 'EXECUTE stmt'; for subqueries the IN->EXISTS transformation is done
# at PREPARE time, and not re-done at EXECUTE time, so in normal
# protocol the trace talks about IN->EXISTS, but in ps-protocol it
# doesn't (as it's the EXECUTE trace and IN->EXISTS was done once for
# all at PREPARE; we see IN->EXISTS if we look at the trace of
# PREPARE).
# That's why we have two result files.
# This test is skipped in view-protocol because this changes queries
# submitted to the optimizer (from 'SELECT x FROM ... WHERE ... etc'
# to 'SELECT x FROM mysqltest_tmp_v') so changes trace.
# It's also skipped in sp-protocol for the same reason (SELECT becomes
# CALL).

# When adding tests here, try to have some of them use SELECT and
# others use EXPLAIN SELECT, to verify that both ways give sufficient
# tracing.

# should be empty
select * from information_schema.OPTIMIZER_TRACE;

let $DEFAULT_TRACE_MEM_SIZE=1048576; # 1MB
eval set optimizer_trace_max_mem_size=$DEFAULT_TRACE_MEM_SIZE;
set @@session.optimizer_trace="enabled=on";

select * from information_schema.OPTIMIZER_TRACE;

CREATE TABLE t5 (c int);
INSERT INTO t5 VALUES (NULL);

CREATE TABLE t6 (d int , KEY (d));
INSERT INTO t6 VALUES (NULL),(NULL);

SELECT (SELECT 1 FROM t6 WHERE d = c) AS RESULT FROM t5 ;

# some float values might not be identical on all platforms, we will see.
select * from information_schema.OPTIMIZER_TRACE;
# check frequency of spaces
select (1-length(replace(TRACE, " ", ""))/length(TRACE))*100
  from information_schema.OPTIMIZER_TRACE;

# now all on one line
set optimizer_trace="one_line=on";
SELECT (SELECT 1 FROM t6 WHERE d = c) AS RESULT FROM t5 ;
select * from information_schema.OPTIMIZER_TRACE;
# check frequency of spaces
select (1-length(replace(TRACE, " ", ""))/length(TRACE))*100
  from information_schema.OPTIMIZER_TRACE;

# From now on, use end markers, easier to read
set end_markers_in_json=on;
set optimizer_trace="one_line=off";

# trace should be produced by EXPLAIN too
EXPLAIN SELECT (SELECT 1 FROM t6 WHERE d = ifnull(c,null)) AS RESULT FROM t5 ;
select * from information_schema.OPTIMIZER_TRACE;

SELECT /* should be last */ (SELECT 1 FROM t6 WHERE d = ifnull(c,null)) AS RESULT FROM t5 ;
select * from information_schema.OPTIMIZER_TRACE;

set @@session.optimizer_trace="enabled=off";
SELECT /* bug if you see this*/ (SELECT 1 FROM t6 WHERE d = ifnull(c,null)) AS RESULT FROM t5 ;
# should not see last statement but previous
select QUERY from information_schema.OPTIMIZER_TRACE;

set @@session.optimizer_trace="enabled=on";

# union in subquery
SELECT (SELECT 1 FROM t6 WHERE d = ifnull(c,null) UNION SELECT 2 FROM t6 WHERE d = ifnull(c,null)) AS RESULT FROM t5 ;
select * from information_schema.OPTIMIZER_TRACE;

# union in subquery in WHERE clause
SELECT * FROM t5 WHERE 5 IN (SELECT 1 FROM t6 WHERE d = ifnull(c,null) UNION SELECT 2 FROM t6 WHERE d = ifnull(c,null));
select * from information_schema.OPTIMIZER_TRACE;

# Test max_mem_size: re-run same query with lower max_mem_size:
# save non-truncated trace but don't output it again
select (@query:=QUERY)+NULL, (@trace:=TRACE)+NULL from information_schema.OPTIMIZER_TRACE;
select length(@trace);
# The concatenation of query and trace above has length:
# - >@max_mem_size in normal mode
# - <@max_mem_size in ps-protocol mode (because IN->EXISTS is done at PREPARE
# and we trace only EXECUTE)
# - So in normal mode, the lines below verify truncation,
# whereas in ps-protocol mode they verify non-truncation.
set @max_mem_size=13900;
set optimizer_trace_max_mem_size=@max_mem_size;
select length(@query)+length(@trace) > @@optimizer_trace_max_mem_size;
SELECT * FROM t5 WHERE 5 IN (SELECT 1 FROM t6 WHERE d = ifnull(c,null) UNION SELECT 2 FROM t6 WHERE d = ifnull(c,null));
select (@missing_bytes:=missing_bytes_beyond_max_mem_size) from information_schema.OPTIMIZER_TRACE;
select (@query2:=QUERY)+NULL,(@trace2:=TRACE)+NULL from information_schema.OPTIMIZER_TRACE;
select length(@trace2),
       (length(@trace2) + @missing_bytes) = length(@trace),
       @query2 = @query;
# If truncated, trace length should be around the maximum, possibly a
# bit higher as we stop tracing after passing the maximum.
select length(@query2) + length(@trace2)
       between (@@optimizer_trace_max_mem_size-200) and (@@optimizer_trace_max_mem_size+200);
# if truncated, trace should be a prefix of non-truncated trace
select instr(@trace, @trace2) = 1;

# Test that if the query is longer than max size, trace is not produced
set optimizer_trace_max_mem_size=1;
select 1;
select * from information_schema.OPTIMIZER_TRACE;
# Test that with max size == 0, even query is not produced
set optimizer_trace_max_mem_size=0;
select 1;
select * from information_schema.OPTIMIZER_TRACE;
eval set optimizer_trace_max_mem_size=$DEFAULT_TRACE_MEM_SIZE;

# semijoin

explain SELECT c FROM t5 where c+1 in (select d+1 from t6 where d is null);
select * from information_schema.OPTIMIZER_TRACE;

# subquery materialization

set @old_opt_switch=@@optimizer_switch;
# The SET below must not be output, because only servers supporting
# semijoin will execute it (would make varying output).
if (`select locate('semijoin', @@optimizer_switch) > 0`)
{
--disable_query_log
  set optimizer_switch="semijoin=off,subquery_materialization_cost_based=off";
--enable_query_log
}
explain SELECT c FROM t5 where c+1 in (select d+1 from t6 where d is null);
select * from information_schema.OPTIMIZER_TRACE;
set optimizer_switch=@old_opt_switch;

# 5.1-style subquery transformations
CREATE TABLE t1 (s1 CHAR(5),
                 s2 CHAR(5));
INSERT INTO t1 VALUES ('z','?'),('y','!');
# > ANY -> > MIN...
explain extended select * from t1 where s1 > any (select s2 from t1);
select * from information_schema.OPTIMIZER_TRACE;
explain extended select * from t1 where s1 > any (select max(s2) from t1);
select * from information_schema.OPTIMIZER_TRACE;
# IN -> EXISTS
if (`select locate('semijoin', @@optimizer_switch) > 0`)
{
--disable_query_log
  set optimizer_switch="semijoin=off,materialization=off";
--enable_query_log
}
explain extended select * from t1 where s1 in (select s2 from t1);
select * from information_schema.OPTIMIZER_TRACE;
explain extended select * from t1 where (s1,s2) in (select s2,s1 from t1);
select * from information_schema.OPTIMIZER_TRACE;
set optimizer_switch=@old_opt_switch;
drop table t1;

# explanation of plan choice
create table t1(a int);
create table t2(a int);
insert into t1 values(1),(2),(3);
insert into t2 values(1),(2);
# print many plans:
set @@session.optimizer_prune_level=0;
explain select * from t1,t2;
select * from information_schema.OPTIMIZER_TRACE;
# don't print plans:
select @@optimizer_trace_features;
set @@optimizer_trace_features="greedy_search=off";
explain select * from t1,t2;
select * from information_schema.OPTIMIZER_TRACE;
set @@optimizer_trace_features=default;
set @@session.optimizer_prune_level=default;
drop table t1, t2;

# test late decision to abandon subquery materialization due to BLOBs
# (from subselect_mat.test)
# force materialization to be always considered
if (`select locate('semijoin', @@optimizer_switch) > 0`)
{
--disable_query_log
  set @@optimizer_switch='semijoin=off';
--enable_query_log
}
set @prefix_len = 6;
# BLOB == 16 (small blobs that could be stored in HEAP tables)
set @blob_len = 16;
set @suffix_len = @blob_len - @prefix_len;
create table t1_16 (a1 blob(16), a2 blob(16));
create table t2_16 (b1 blob(16), b2 blob(16));
create table t3_16 (c1 blob(16), c2 blob(16));
insert into t1_16 values
 (concat('1 - 00', repeat('x', @suffix_len)), concat('2 - 00', repeat('x', @suffix_len)));
insert into t1_16 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t1_16 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t2_16 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t2_16 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t2_16 values
 (concat('1 - 03', repeat('x', @suffix_len)), concat('2 - 03', repeat('x', @suffix_len)));
insert into t3_16 values
 (concat('1 - 01', repeat('x', @suffix_len)), concat('2 - 01', repeat('x', @suffix_len)));
insert into t3_16 values
 (concat('1 - 02', repeat('x', @suffix_len)), concat('2 - 02', repeat('x', @suffix_len)));
insert into t3_16 values
 (concat('1 - 03', repeat('x', @suffix_len)), concat('2 - 03', repeat('x', @suffix_len)));
insert into t3_16 values
 (concat('1 - 04', repeat('x', @suffix_len)), concat('2 - 04', repeat('x', @suffix_len)));
# single value transformer
explain extended select left(a1,7), left(a2,7)
from t1_16
where a1 in (select b1 from t2_16 where b1 > '0');
select * from information_schema.OPTIMIZER_TRACE;
drop table t1_16,t2_16,t3_16;
set @@optimizer_switch=@old_opt_switch;

# test of outer-join preventing semijoin
# from subselect.test

CREATE table t1 ( c1 integer );
INSERT INTO t1 VALUES ( 1 );
INSERT INTO t1 VALUES ( 2 );
INSERT INTO t1 VALUES ( 3 );

CREATE TABLE t2 ( c2 integer );
INSERT INTO t2 VALUES ( 1 );
INSERT INTO t2 VALUES ( 4 );
INSERT INTO t2 VALUES ( 5 );

SELECT * FROM t1 LEFT JOIN t2 ON c1 = c2
  WHERE c2 IN ( SELECT c2 FROM t2 WHERE c2 IN ( 1 ) );
select * from information_schema.OPTIMIZER_TRACE;

# union in top-query

SELECT * FROM t1 WHERE c1=5 UNION SELECT * FROM t2 WHERE c2=5;
select * from information_schema.OPTIMIZER_TRACE;

# two subqueries in top query

if (`select locate('semijoin', @@optimizer_switch) > 0`)
{
--disable_query_log
  set optimizer_switch="semijoin=off";
--enable_query_log
}

explain
select * from t1
where concat(c1,'x') IN
      (select left(c2,8) from t2)
and
concat(c1,'y') IN
      (select left(c2,9) from t2);

select * from information_schema.OPTIMIZER_TRACE;

set optimizer_switch=@old_opt_switch;

DROP TABLE t1,t2;

# two constant tables

create table t1 (a int);
insert into t1 values(1);
create table t2 (a int);
insert into t2 values(1);
select * from t1,t2;
select * from information_schema.OPTIMIZER_TRACE;

# semijoin pullout

create table t3 (a int, b int);
create table t4 (a int primary key);
insert into t4 values(1),(2);
# we do it with prepared statements; by reading the expanded query we
# can see that
# - subquery->semijoin transformation is done at first EXECUTE
# - same for semijoin table pullout
# - both are reused by the second EXECUTE (where expanded_query is a
# pure join)
prepare stmt from 'select * from t3 where (a,a,b) in (select * from t1,t2,t4)';
select trace from information_schema.OPTIMIZER_TRACE;
execute stmt;
select trace from information_schema.OPTIMIZER_TRACE;
execute stmt;
select trace from information_schema.OPTIMIZER_TRACE;
DROP TABLE t1,t2,t3,t4;

# Trig_cond printing

create table t1(a int);
insert into t1 values(1),(1);
create table t2(a int);
insert into t2 values(1),(1);
# guarded pushed down conds for WHERE and ON in LEFT JOIN
select * from t1 left join t2 on t2.a=500 where t2.a is NULL;
select * from information_schema.OPTIMIZER_TRACE;
drop table t1,t2;
# guarded added conds for IN->EXISTS:
create table t1(a int, b int);
insert into t1 values(1,NULL),(NULL,2);
create table t2(c int, d int);
insert into t2 values(1,1),(2,2);
select * from t1 where (t1.a,t1.b) not in (select c,d from t2 where c>0);
select * from information_schema.OPTIMIZER_TRACE;

# HAVING
select t1.a,avg(t2.c) as moyenne from t1, t2 where t2.c>-1
  group by t1.a having moyenne<>0;
--replace_regex /("sort_buffer_size":) [0-9]+/\1 "NNN"/
select trace from information_schema.OPTIMIZER_TRACE;
# impossible HAVING
select t1.a,avg(t2.c) as moyenne from t1, t2 where t2.c>-1
  group by t1.a having 4=5;
select trace from information_schema.OPTIMIZER_TRACE;
drop table t1,t2;

# HAVING


# test non-SELECT statements

update t6 set d=5 where d is NULL;
select * from information_schema.OPTIMIZER_TRACE;

delete from t6 where d=5;
select * from information_schema.OPTIMIZER_TRACE;

insert into t6 values(6),(7),(8);
select * from information_schema.OPTIMIZER_TRACE;

insert into t6 select * from t6 where d>7;
select * from information_schema.OPTIMIZER_TRACE;

update t5, t6 set t6.d=t6.d+t5.c+4-t5.c-4 where d>7000;
select * from information_schema.OPTIMIZER_TRACE;

delete t6 from t5, t6 where d>7000;
select * from information_schema.OPTIMIZER_TRACE;

# Test optimizer_trace_offset/limit for consecutive
# non-related statements (as opposed to "for sub-statements"
# which is tested further below).
set optimizer_trace_offset=2,optimizer_trace_limit=2;
# should be empty
select * from information_schema.OPTIMIZER_TRACE;
select 1;
select 2;
select 3;
select 4;
select 5;
# In normal mode, each SELECT is a standalone statement and generates
# one trace: so 3rd and 4th trace means 3rd and 4th SELECT.
# In ps-protocol mode, each SELECT is two statements: PREPARE and
# EXECUTE; each of the two creates one trace; so 3rd and 4th trace
# means 2nd SELECT (preparation) and 2nd SELECT again (execution).
select * from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=-2,optimizer_trace_limit=2;
# should be empty
select * from information_schema.OPTIMIZER_TRACE;
select 1;
select 2;
select 3;
select 4;
select 5;
select * from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=default,optimizer_trace_limit=default;
# should be empty
select * from information_schema.OPTIMIZER_TRACE;

# For stored functions, stored procedures, triggers, with
# offset=-1,limit=1, what we see in the trace is the trace of the last
# statement (i.e. last call to mysql_execute_command()). Thus:
# - for a call to a stored procedure, we will see only the last
# statement of this procedure
# - for a call to a trigger, we will see only the last statement of
# this trigger, and not the caller.
# etc.
# If the user wants to see more, 'offset' should be adjusted.

# Stored functions

create table t1 (
	id   char(16) not null default '',
        data int not null
);
create table t2 (
	s   char(16),
        i   int,
	d   double
);
delimiter |;
insert into t1 values ("a", 1), ("b", 2) |
insert into t2 values ("a", 1, 1.0), ("b", 2, 2.0), ("c", 3, 3.0) |
create function f1() returns int
begin
  declare ret int;
  insert into t1 values("z",0);
  delete from t1 where id="z";
  select sum(data) into ret from t1;
  return ret;
end|
select f1()|
# Here we will see the trace of RETURN.
select * from information_schema.OPTIMIZER_TRACE|
select s, f1() from t2 order by s desc|
select * from information_schema.OPTIMIZER_TRACE|
select * from t6 where d in (select f1() from t2 where s="c")|
select * from information_schema.OPTIMIZER_TRACE|
# Want to see the top and invoked sub-statements; this means 11 traces:
# 1 top statement + two executions of f1() (there is one
# execution inside the range optimizer and one "normal" execution);
# in the stored function we have traces: DECLARE (1 trace), 3 DMLs
# and RETURN (1 trace). 1+2*(1+3+1)=11.
# In ps-protocol mode, we have those 11, plus one for PREPARE of the
# top SELECT.
# We ask for a larger number (60) and will check how many we got.
# Note that when semijoin features are disabled, the subquery is
# treated differently: it is executed, so the order and amount of
# executions of f1() becomes different, which changes the order of
# substatements in the trace.
set @old_opt_switch=@@optimizer_switch;
# Test was created when sj-mat-scan could not handle this query...
set optimizer_switch="materialization=off";
set optimizer_trace_offset=-60, optimizer_trace_limit=60|
select * from t6 where d in (select f1() from t2 where s="c")|
select * from information_schema.OPTIMIZER_TRACE|
select count(*) from information_schema.OPTIMIZER_TRACE|
# Want to see the DELETE (invoked sub-statement):
set optimizer_trace_offset=3, optimizer_trace_limit=1|
select * from t6 where d in (select f1() from t2 where s="c")|
# In normal mode, we have traces for the top SELECT, then the
# function's INSERT then the function's DELETE, so DELETE is the
# third trace, which we see.
# In ps-protocol mode, we also have trace of PREPARE for the top
# SELECT, so we see one trace before the DELETE: the INSERT.
select * from information_schema.OPTIMIZER_TRACE|
set @@optimizer_switch=@old_opt_switch;

# Stored procedures
create procedure p1(arg char(1))
begin
  declare res int;
  select d into res from t6 where d in (select f1() from t2 where s=arg);
  select d+1 into res from t6 where d=res+1;
end|
set @old_opt_switch=@@optimizer_switch;
# Test was created when sj-mat-scan could not handle this query...
set optimizer_switch="materialization=off";
set optimizer_trace_offset=0, optimizer_trace_limit=100;
call p1("c")|
select * from information_schema.OPTIMIZER_TRACE|
set @@optimizer_switch=@old_opt_switch;

# Triggers
create trigger trg1 before insert on t2 for each row
begin
  set new.s=f1();  
end|
set optimizer_trace_offset=0, optimizer_trace_limit=100|
insert into t2 select d,100,200 from t6 where d is not null|
select * from information_schema.OPTIMIZER_TRACE|
select * from t2|
delimiter ;|

# PREPARE/EXECUTE/EXECUTE
prepare stmt from 'select count(*) from t1 where t1.data=?';
set @param="c";
set optimizer_trace_offset=0, optimizer_trace_limit=100;
execute stmt using @param;
select count(*) from information_schema.OPTIMIZER_TRACE;
select TRACE into @trace from information_schema.OPTIMIZER_TRACE;
select @trace;
# second EXECUTE should give same trace
set optimizer_trace_offset=0, optimizer_trace_limit=100;
execute stmt using @param;
select count(*) from information_schema.OPTIMIZER_TRACE;
select TRACE into @trace2 from information_schema.OPTIMIZER_TRACE;
select @trace=@trace2;

# enable/disable tracing in middle of procedure
drop procedure p1;
create temporary table optt like information_schema.OPTIMIZER_TRACE;
delimiter |;
create procedure p1(arg char(1))
begin
  declare res int;
  set optimizer_trace="enabled=off";
  # want to see all of SELECT below
  set optimizer_trace_offset=0, optimizer_trace_limit=100;
  set optimizer_trace="enabled=on";
  select d into res from t6 where d in (select f1() from t2 where s=arg);
  set optimizer_trace="enabled=off"; # and not more
  insert into optt select * from information_schema.OPTIMIZER_TRACE;
  set optimizer_trace_offset=default, optimizer_trace_limit=default;
  select d+1 into res from t6 where d=res+1;
end|
call  p1("c")|
select * from optt|
select @@optimizer_trace|
delimiter ;|

set optimizer_trace="enabled=on";
drop temporary table optt;
drop function f1;
drop procedure p1;
drop trigger trg1;

# Views
# merge-able
create view v1 as select * from t1 where id < "c";
explain select * from v1 where id="b";
select * from information_schema.OPTIMIZER_TRACE;
# insert into view
insert into v1 values("z", 100);
select * from information_schema.OPTIMIZER_TRACE;
delete from v1 where data=100;
select * from information_schema.OPTIMIZER_TRACE;
drop view v1;
# not merge-able
create view v1 as select * from t1 where id < "c" limit 2;
explain select * from v1 where id="b";
select * from information_schema.OPTIMIZER_TRACE;
drop view v1;

# I_S tables
select * from information_schema.session_variables where
VARIABLE_NAME="optimizer_trace";
select * from information_schema.OPTIMIZER_TRACE;

# test that DUMPFILE does no escaping of \n
# otherwise a JSON parser could not read
set end_markers_in_json=off;
select 1 union select 2;
--let $file=$MYSQLTEST_VARDIR/tmp/optimizer_trace.txt
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval select TRACE into dumpfile '$file' from information_schema.OPTIMIZER_TRACE;
# it has been manually checked that this file's content is JSON-compliant
--cat_file $file
--remove_file $file

# Test for crashing bug
--error ER_WRONG_VALUE_FOR_VAR
set optimizer_switch='default,index_merge=on,index_merge=off,default';
select @@optimizer_switch=@old_opt_switch;

# Test for long query (1070 chars)
--disable_query_log
--disable_result_log
select "abc1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111def" as col;
--enable_result_log
--enable_query_log
select * from information_schema.OPTIMIZER_TRACE;

drop table t1,t2;
DROP TABLE t5,t6;
set optimizer_trace=default;
