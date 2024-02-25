drop table if exists test.setup_objects;
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
create table test.setup_objects as
select * from performance_schema.setup_objects;
truncate table performance_schema.setup_objects;
select * from performance_schema.setup_objects;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	ENABLED	TIMED
select * from performance_schema.setup_objects
order by object_type, object_schema, object_name;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	ENABLED	TIMED
select * from performance_schema.setup_objects
where object_type = 'TABLE'
  order by object_type, object_schema, object_name;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	ENABLED	TIMED
select * from performance_schema.setup_objects
order by object_type, object_schema, object_name;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	ENABLED	TIMED
insert into performance_schema.setup_objects
set object_type='ILLEGAL', object_schema='foo', object_name='bar',
enabled='YES', timed='YES';
ERROR 23000: Cannot add or update a child row: a foreign key constraint fails ()
insert into performance_schema.setup_objects
set object_type='TABLE', object_schema='foo', object_name='bar',
enabled='YES', timed='YES';
insert into performance_schema.setup_objects
set object_type='TABLE', object_schema='foo', object_name='bar',
enabled='YES', timed='NO';
ERROR 23000: Can't write; duplicate key in table 'setup_objects'
delete from performance_schema.setup_objects
where object_type='TABLE' and object_schema='foo';
select * from performance_schema.setup_objects
order by object_type, object_schema, object_name;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	ENABLED	TIMED
insert into performance_schema.setup_objects
set object_type='TABLE', object_schema='foo', object_name='bar',
enabled='NO', timed='YES';
select * from performance_schema.setup_objects
order by object_type, object_schema, object_name;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	ENABLED	TIMED
TABLE	foo	bar	NO	YES
update performance_schema.setup_objects
set object_type='ILLEGAL';
ERROR HY000: Invalid performance_schema usage.
update performance_schema.setup_objects
set object_schema='ILLEGAL';
ERROR HY000: Invalid performance_schema usage.
update performance_schema.setup_objects
set object_name='ILLEGAL';
ERROR HY000: Invalid performance_schema usage.
update performance_schema.setup_objects
set enabled='NO', timed='NO';
select * from performance_schema.setup_objects
order by object_type, object_schema, object_name;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	ENABLED	TIMED
TABLE	foo	bar	NO	NO
update performance_schema.setup_objects
set enabled='YES', timed='YES';
delete from performance_schema.setup_objects
where object_type = 'TABLE';
delete from performance_schema.setup_objects;
LOCK TABLES performance_schema.setup_objects READ;
UNLOCK TABLES;
LOCK TABLES performance_schema.setup_objects WRITE;
UNLOCK TABLES;
truncate table performance_schema.setup_objects;
insert into performance_schema.setup_objects
select * from test.setup_objects;
drop table test.setup_objects;
SET sql_mode = default;
