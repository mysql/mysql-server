select @@global.performance_schema_show_processlist into @save_processlist;
set @@global.performance_schema_show_processlist = 'on';
alter table performance_schema.processlist
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.processlist;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'processlist'
alter table performance_schema.processlist
add index test_index(info);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
create unique index test_index
on performance_schema.processlist(host);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
drop index `PRIMARY`
  on performance_schema.processlist;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.processlist;
DROP TABLE test.create_select;
set @@global.performance_schema_show_processlist = @save_processlist;
