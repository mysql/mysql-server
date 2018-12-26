drop table if exists t1, t2, t3;
create table t1 (a int, b int generated always as (a % 10) virtual);
create table t2 (a int, b int generated always as (a % 10) virtual);
insert into t1 values (1,default);
insert into t2 values (2,default);
create table t3 (a int, b int generated always as (a % 10) virtual) engine=MERGE UNION=(t1,t2);
ERROR HY000: 'Specified storage engine' is not supported for generated columns.
drop table t1,t2;
DROP VIEW  IF EXISTS v1,v2;
DROP TABLE IF EXISTS t1,t2,t3;
DROP PROCEDURE IF EXISTS p1;
DROP FUNCTION IF EXISTS f1;
DROP TRIGGER IF EXISTS trg1;
DROP TRIGGER IF EXISTS trg2;
set sql_warnings = 0;
