# Continuation of tests for optimizer trace

--source include/have_optimizer_trace.inc

let $DEFAULT_TRACE_MEM_SIZE=1048576; # 1MB
eval set optimizer_trace_max_mem_size=$DEFAULT_TRACE_MEM_SIZE;

set optimizer_trace="enabled=on,end_marker=on";

--echo # check that if a sub-statement should not be traced,
--echo # it is not traced even if inside a traced top statement
--echo
set optimizer_trace_offset=0, optimizer_trace_limit=100;
delimiter |;
create function f1(arg char(1)) returns int
begin
  declare res int;
  declare dummy varchar(1);
  select 1 into res from dual;
  select TRACE+NULL into dummy from information_schema.OPTIMIZER_TRACE limit 1;
  select 2 into res from dual;
  return 3;
end|
# ps-protocol specific note: as we asked to retain all traces,
# we see the one of PREPARE too.
select f1("c")|
--echo
# we should not see the trace of "select TRACE+NULL..."
# because tracing is disabled when OPTIMIZER_TRACE table is used.
select * from information_schema.OPTIMIZER_TRACE|
delimiter ;|
set optimizer_trace_offset=default, optimizer_trace_limit=default;
drop function f1;

--echo # check that if a tracing gets disabled in a routine's  body,
--echo # substatements are not traced
--echo
set optimizer_trace_offset=0, optimizer_trace_limit=100;
delimiter |;
create function f1(arg char(1)) returns int
begin
  declare res int;
  declare dummy varchar(1);
  set optimizer_trace="enabled=off";
  select 1 into res from dual;
  select TRACE+NULL into dummy from information_schema.OPTIMIZER_TRACE limit 1;
  select 2 into res from dual;
  return 3;
end|
select f1("c")|
--echo
select * from information_schema.OPTIMIZER_TRACE|
delimiter ;|
set optimizer_trace_offset=default, optimizer_trace_limit=default;
select @@optimizer_trace;
set optimizer_trace="enabled=on";
drop function f1;

--echo
--echo # Check that if a sub-statement reads OPTIMIZER_TRACE,
--echo # thus reading the unfinished trace of its caller statement,
--echo # there is no crash.
--echo

create temporary table optt
(id int primary key auto_increment,
QUERY varchar(200),
TRACE text);
create table t1 (a int, key(a));
insert into t1 values(2);
set optimizer_trace_offset=0, optimizer_trace_limit=100;
delimiter |;
create function f1(arg char(1)) returns int
begin
  declare res int;
  insert into optt select NULL, QUERY, TRACE from information_schema.OPTIMIZER_TRACE;
  return 3;
end|
select * from t1 where a in (select f1("c") from t1)|
--echo
delimiter ;|
set optimizer_trace="enabled=off";
--echo this should find unfinished traces
select count(*) from optt where TRACE NOT LIKE "%] /* steps */\n}";
select count(*)<>0 from optt;
--echo this should not
select count(*) from information_schema.OPTIMIZER_TRACE where TRACE NOT LIKE "%] /* steps */\n}";
select count(*)<>0 from information_schema.OPTIMIZER_TRACE;

set optimizer_trace_offset=default, optimizer_trace_limit=default;
drop temporary table optt;
drop function f1;
drop table t1;
set optimizer_trace="enabled=on";

--echo
--echo # check of crash with I_S.VIEWS (TABLE_LIST::alias==NULL)
--echo
create table t1(a int, b int);
create view v1 as select a from t1;
select VIEW_DEFINITION from information_schema.VIEWS
where TABLE_SCHEMA="test" and TABLE_NAME="v1";
select locate("\"view\": \"v1\"", TRACE) != 0
from information_schema.OPTIMIZER_TRACE;
drop table t1;
drop view v1;

--echo
--echo # check for readable display of BIT values
--echo
create table t1 (a bit(5), key(a));
insert into t1 values(b'00000'),(b'01101');
select cast(a as unsigned) from t1 where a > b'01100';
# Note that in the trace we get either 0x0c or 12
select TRACE from information_schema.OPTIMIZER_TRACE;
drop table t1;

--echo
--echo # check that trace lists all pushed down ON conditions
--echo
create table t1 (i int not null);
insert into t1 values (0),    (2),(3),(4);
create table t2 (i int not null);
insert into t2 values (0),(1),    (3),(4);
create table t3 (i int not null);
insert into t3 values (0),(1),(2),    (4);
select * from
 t1 LEFT JOIN
 ( t2 LEFT JOIN
   ( t3 
   )
   ON t3.i = t2.i
 )
 ON t2.i = t1.i
 WHERE t3.i IS NULL
 ;
select TRACE from information_schema.OPTIMIZER_TRACE;
drop table t1,t2,t3;

--echo
--echo # test of tracing a query with an HAVING condition, in
--echo # ps-protocol, does not crash
--echo
# Comes from having.test

CREATE TABLE t1 (f1 INT, f2 VARCHAR(1));
INSERT INTO t1 VALUES (16,'f');
INSERT INTO t1 VALUES (16,'f');
CREATE TABLE t2 (f1 INT, f2 VARCHAR(1));
INSERT INTO t2 VALUES (13,'f');
INSERT INTO t2 VALUES (20,'f');
CREATE TABLE t3 (f1 INT, f2 VARCHAR(1));
INSERT INTO t3 VALUES (7,'f');

SELECT t1.f2 FROM t1
STRAIGHT_JOIN (t2 JOIN t3 ON t3.f2  = t2.f2  ) ON t3 .f2  = t2 .f2
HAVING ('v', 'i') NOT IN (SELECT f2, MIN(f2) FROM t1)
ORDER BY f2;
select TRACE from information_schema.OPTIMIZER_TRACE;

DROP TABLES t1,t2,t3;

--echo
--echo # Test that tracing a query with a materialized FROM-clause
--echo # derived table using a GROUP BY, does not crash
--echo
# Comes from profiling.test
create table t1 (a int, b int);
insert into t1 values (1,1), (2,null), (3, 4);
select max(x) from (select sum(a) as x from t1 group by b) as teeone;
select TRACE from information_schema.OPTIMIZER_TRACE;
drop table t1;

--echo
--echo # To have no crash above, we had to restore the ref_array at
--echo # end of JOIN::exec(). This impacts how the query looks like,
--echo # but not too much, as seen in the error message below.
--echo # Comes from func_gconcat.test.
--echo
CREATE TABLE t1(f1 int);
INSERT INTO t1 values (0),(0);
set optimizer_trace="enabled=off";
--disable_ps_protocol
--error ER_ILLEGAL_VALUE_FOR_TYPE
SELECT POLYGON((SELECT 1 FROM (SELECT 1 IN (GROUP_CONCAT(t1.f1)) FROM t1, t1 t GROUP BY t.f1 ) d));
--enable_ps_protocol
set optimizer_trace="enabled=on";
--disable_ps_protocol
--error ER_ILLEGAL_VALUE_FOR_TYPE
SELECT POLYGON((SELECT 1 FROM (SELECT 1 IN (GROUP_CONCAT(t1.f1)) FROM t1, t1 t GROUP BY t.f1 ) d));
--enable_ps_protocol
DROP TABLE t1;

--echo
--echo # Check that SQL PREPARE and SQL EXECUTE each produce one trace.
--echo
set optimizer_trace_offset=0, optimizer_trace_limit=100;
prepare stmt from "select 1";
select * from information_schema.OPTIMIZER_TRACE;
set optimizer_trace_offset=0, optimizer_trace_limit=100;
execute stmt;
select * from information_schema.OPTIMIZER_TRACE;
deallocate prepare stmt;
set optimizer_trace_offset=default, optimizer_trace_limit=default;

--echo
--echo # Test of SELECTs in IF in stored routine.
--echo # Same test for CASE WHEN.
--echo
create table t1 (a int);
delimiter |;
create procedure p1()
begin
  if exists(select 1) then
    insert into t1 values(1);
  end if;
  if exists(select 2) then
    insert into t1 values(2);
  end if;
  if (select count(*) from t1) then
    insert into t1 values(3);
  end if;
  set @a=(select count(a) from t1 where a>0);
  case (select count(a) from t1 where a>1)
    when 2 then set @b=2;
    else set @b=3;
  end case;
end|
delimiter ;|
set optimizer_trace_offset=0, optimizer_trace_limit=100;
set @old_max=@@optimizer_trace_max_mem_size;
set optimizer_trace_max_mem_size=40000;
call p1();
# SET @a=(SELECT) is not traced because part of SET
# which is a real command and not traced.
select * from information_schema.OPTIMIZER_TRACE;
select * from t1;
select @a,@b;
set optimizer_trace_max_mem_size=@old_max;
drop procedure p1;
drop table t1;

--echo
--echo # Test of tracing of DO.
--echo

set optimizer_trace_offset=0, optimizer_trace_limit=100;
do (select 42);
select * from information_schema.OPTIMIZER_TRACE;

--echo
--echo # Test of tracing of subquery used in parameter of routine call
--echo
create table t1(a int);
insert into t1 values(1),(2);
delimiter |;
create procedure p1(x int)
begin
  declare b int;
  set b=(select 2+x from dual);
end|
delimiter ;|
set optimizer_trace_offset=0, optimizer_trace_limit=100;
call p1((select a from t1 limit 1));
select * from information_schema.OPTIMIZER_TRACE;
drop procedure p1;
drop table t1;
set optimizer_trace_offset=default, optimizer_trace_limit=default;

--echo
--echo # Test that printing expanded query does not alter query's
--echo # results.
--echo # Comes from ctype_utf8mb4_heap.test
--echo
create table t1 (f1 varchar(1) not null) default charset utf8mb4;
insert into t1 values (''), ('');
select concat(concat(_latin1'->',f1),_latin1'<-') from t1;
select * from information_schema.optimizer_trace;
drop table t1;

--echo
--echo # Bug#12546331 - SEGFAULT IN SUBSELECT_INDEXSUBQUERY_ENGINE::PRINT WITH OPTIMIZER TRACE
--echo

CREATE TABLE t1 (  
  col_int_nokey INT,  
  col_int_key INT,  
  col_varchar_key varchar(1),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t1 VALUES
  (NULL,8,'x'),
  (8,7,'d'),
  (1,1,'r'),
  (9,7,'f'),
  (4,9,'y'),
  (3,NULL,'u'),
  (2,1,'m'),
  (NULL,9,NULL),
  (2,2,'o'),
  (NULL,9,'w'),
  (6,2,'m'),
  (7,4,'q'),
  (2,0,NULL),
  (5,4,'d'),
  (7,8,'g'),
  (6,NULL,'x'),
  (6,NULL,'f'),
  (2,0,'p'),
  (9,NULL,'j'),
  (6,8,'c')
;

CREATE TABLE t2 (
  col_int_nokey INT,
  col_int_key INT,
  col_varchar_key varchar(1),
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t2 VALUES
  (2,4,'v'),
  (150,62,'v'),
  (NULL,7,'c'),
  (2,1,NULL),
  (5,0,'x'),
  (3,7,'i'),
  (1,7,'e'),
  (4,1,'p'),
  (NULL,7,'s'),
  (2,1,'j'),
  (6,5,'z'),
  (6,2,'c'),
  (8,0,'a'),
  (2,1,'q'),
  (6,8,'y'),
  (8,1,NULL),
  (3,1,'r'),
  (3,9,'v'),
  (9,1,NULL),
  (6,5,'r')
;

SELECT col_int_nokey
FROM (
  SELECT *
  FROM t2
  WHERE col_varchar_key > 'a'
    OR ( 7 , 5 ) NOT IN (
      SELECT col_int_nokey , col_int_key
      FROM t1 )
  ) AS alias1;

DROP TABLE t1;
DROP TABLE t2;

--echo
--echo BUG#12552262 - INVALID JSON WITH TWO CALLS TO TEST_QUICK_SELECT
--echo

CREATE TABLE t1 (
  col_varchar_10_latin1_key varchar(10) DEFAULT NULL,
  col_int_key INT,
  KEY col_int_key (col_int_key)
);

CREATE TABLE t2 (
  col_varchar_10_latin1_key varchar(10) DEFAULT NULL,
  col_varchar_10_utf8_key varchar(10) CHARACTER SET utf8 DEFAULT NULL,
  col_int_key INT,
  KEY col_varchar_10_utf8_key (col_varchar_10_utf8_key),
  KEY col_int_key (col_int_key)
);

INSERT INTO t2 VALUES ('qykbaqfyhz','l',NULL);

CREATE TABLE t3 (
  col_int_key INT,
  col_varchar_10_utf8_key varchar(10) CHARACTER SET utf8 DEFAULT NULL,
  col_varchar_10_latin1_key varchar(10) DEFAULT NULL,
  KEY col_varchar_10_utf8_key (col_varchar_10_utf8_key),
  KEY col_varchar_10_latin1_key (col_varchar_10_latin1_key)
);

INSERT INTO t3 VALUES (0,'s','it');
INSERT INTO t3 VALUES (9,'IQTHK','JCAQM');

SELECT table2.col_int_key
FROM t3 AS table1
  LEFT JOIN t1 AS table2 ON table1.col_int_key < table2.col_int_key
  LEFT JOIN t2 AS table3 ON table2.col_varchar_10_latin1_key >=
table3.col_varchar_10_utf8_key
;

select * from information_schema.optimizer_trace;

DROP TABLE t1,t2,t3;
