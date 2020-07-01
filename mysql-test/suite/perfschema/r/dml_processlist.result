select @@global.performance_schema_show_processlist into @save_processlist;
set @@global.performance_schema_show_processlist = 'on';
select * from performance_schema.processlist
where user like 'event_scheduler';
select * from performance_schema.processlist
where user = 'FOO';
insert into performance_schema.processlist
values (12, 'foo', 'bar', 'test', null, 1000, 'state', 'info');
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'processlist'
update performance_schema.processlist
set id=12, user='foo';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'processlist'
delete from performance_schema.processlist
where id <> 99;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'processlist'
delete from performance_schema.processlist;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'processlist'
LOCK TABLES performance_schema.processlist READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'processlist'
UNLOCK TABLES;
LOCK TABLES performance_schema.processlist WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'processlist'
UNLOCK TABLES;
set @@global.performance_schema_show_processlist = @save_processlist;
