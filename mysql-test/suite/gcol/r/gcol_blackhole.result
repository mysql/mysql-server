SET @@session.default_storage_engine = 'blackhole';
create table t1 (a int, b int generated always as (a+1) virtual);
ERROR HY000: 'Specified storage engine' is not supported for generated columns.
create table t1 (a int);
alter table t1 add column b int generated always as (a+1) virtual;
ERROR HY000: 'Specified storage engine' is not supported for generated columns.
drop table t1;
DROP VIEW  IF EXISTS v1,v2;
DROP TABLE IF EXISTS t1,t2,t3;
DROP PROCEDURE IF EXISTS p1;
DROP FUNCTION IF EXISTS f1;
DROP TRIGGER IF EXISTS trg1;
DROP TRIGGER IF EXISTS trg2;
set sql_warnings = 0;
