alter table performance_schema.setup_actors add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
drop table if exists test.setup_actors;
create table test.setup_actors as
select * from performance_schema.setup_actors;
truncate table performance_schema.setup_actors;
select count(*) from performance_schema.setup_actors;
count(*)
0
insert into performance_schema.setup_actors
select * from test.setup_actors;
drop table test.setup_actors;
ALTER TABLE performance_schema.setup_actors ADD INDEX test_index(HOST);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.setup_actors(HOST);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
ALTER TABLE performance_schema.setup_actors ADD KEY ENA(ENABLED);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.setup_actors;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.setup_actors;
DROP TABLE test.create_select;
