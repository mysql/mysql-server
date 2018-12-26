drop table if exists test.setup_objects;
update performance_schema.setup_instruments set enabled='NO';
update performance_schema.setup_instruments set enabled='YES', timed='YES'
  where name in ('wait/io/table/sql/handler', 'wait/lock/table/sql/handler');
truncate table performance_schema.table_io_waits_summary_by_table;
truncate table performance_schema.table_lock_waits_summary_by_table;
create table test.setup_objects as select * from performance_schema.setup_objects;
truncate table performance_schema.setup_objects;
select * from performance_schema.setup_objects;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	ENABLED	TIMED
insert into performance_schema.setup_objects
values ('TABLE', 'db1', 't1', 'YES', 'YES');
insert into performance_schema.setup_objects
values ('TABLE', 'db1', 't2', 'YES', 'YES');
select * from performance_schema.setup_objects
order by OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	ENABLED	TIMED
TABLE	db1	t1	YES	YES
TABLE	db1	t2	YES	YES
SET sql_mode = default;
drop database if exists db1;
create database db1;
create table db1.t1 (a int, b char(10) default 'default', unique key uidx(a));
create table db1.t2 (a int, b char(10) default 'default');
insert into db1.t1 values('11', 'db1.t1');
insert into db1.t1 values('12', 'db1.t1');
insert into db1.t1 values('13', 'db1.t1');
insert into db1.t2 values('21', 'db1.t2');
insert into db1.t2 values('22', 'db1.t2');
insert into db1.t2 values('23', 'db1.t2');
select OBJECT_SCHEMA, OBJECT_NAME, COUNT_STAR, COUNT_READ, COUNT_WRITE
from performance_schema.table_lock_waits_summary_by_table
where OBJECT_SCHEMA='db1';
OBJECT_SCHEMA	OBJECT_NAME	COUNT_STAR	COUNT_READ	COUNT_WRITE
db1	t1	3	0	3
db1	t2	3	0	3
select OBJECT_SCHEMA, OBJECT_NAME, COUNT_STAR, COUNT_READ, COUNT_WRITE
from performance_schema.table_io_waits_summary_by_table
where OBJECT_SCHEMA='db1';
OBJECT_SCHEMA	OBJECT_NAME	COUNT_STAR	COUNT_READ	COUNT_WRITE
db1	t1	3	0	3
db1	t2	3	0	3
update performance_schema.setup_objects
set ENABLED='NO' where OBJECT_SCHEMA = 'db1' and OBJECT_NAME='t1';
select OBJECT_SCHEMA, OBJECT_NAME, COUNT_STAR, COUNT_READ, COUNT_WRITE
from performance_schema.table_lock_waits_summary_by_table
where OBJECT_SCHEMA='db1';
OBJECT_SCHEMA	OBJECT_NAME	COUNT_STAR	COUNT_READ	COUNT_WRITE
db1	t1	3	0	3
db1	t2	3	0	3
select OBJECT_SCHEMA, OBJECT_NAME, COUNT_STAR, COUNT_READ, COUNT_WRITE
from performance_schema.table_io_waits_summary_by_table
where OBJECT_SCHEMA='db1';
OBJECT_SCHEMA	OBJECT_NAME	COUNT_STAR	COUNT_READ	COUNT_WRITE
db1	t1	3	0	3
db1	t2	3	0	3
update performance_schema.setup_objects
set ENABLED='YES' where OBJECT_SCHEMA = 'db1' and OBJECT_NAME='t1';
select OBJECT_SCHEMA, OBJECT_NAME, COUNT_STAR, COUNT_READ, COUNT_WRITE
from performance_schema.table_lock_waits_summary_by_table
where OBJECT_SCHEMA='db1';
OBJECT_SCHEMA	OBJECT_NAME	COUNT_STAR	COUNT_READ	COUNT_WRITE
db1	t1	3	0	3
db1	t2	3	0	3
select OBJECT_SCHEMA, OBJECT_NAME, COUNT_STAR, COUNT_READ, COUNT_WRITE
from performance_schema.table_io_waits_summary_by_table
where OBJECT_SCHEMA='db1';
OBJECT_SCHEMA	OBJECT_NAME	COUNT_STAR	COUNT_READ	COUNT_WRITE
db1	t1	3	0	3
db1	t2	3	0	3
truncate table performance_schema.setup_objects;
select count(*) from performance_schema.setup_objects;
count(*)
0
drop database db1;
truncate table performance_schema.setup_objects;
insert into performance_schema.setup_objects select * from test.setup_objects;
drop table test.setup_objects;
update performance_schema.setup_instruments set enabled='YES', timed='YES';
