select * from performance_schema.threads
where name like 'Thread/%' limit 1;
select * from performance_schema.threads
where name='FOO';
insert into performance_schema.threads
set name='FOO', thread_id=1, processlist_id=2;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'threads'
update performance_schema.threads
set thread_id=12;
ERROR HY000: Invalid performance_schema usage.
update performance_schema.threads
set thread_id=12 where PROCESSLIST_ID=connection_id();
ERROR HY000: Invalid performance_schema usage.
update performance_schema.threads
set instrumented= 'NO' where PROCESSLIST_ID=connection_id();
select instrumented from performance_schema.threads
where PROCESSLIST_ID=connection_id();
instrumented
NO
update performance_schema.threads
set instrumented= 'YES' where PROCESSLIST_ID=connection_id();
select instrumented from performance_schema.threads
where PROCESSLIST_ID=connection_id();
instrumented
YES
update performance_schema.threads
set resource_group = 'FOO' where PROCESSLIST_ID=connection_id();
ERROR HY000: Invalid performance_schema usage.
delete from performance_schema.threads
where id=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'threads'
delete from performance_schema.threads;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'threads'
LOCK TABLES performance_schema.threads READ;
UNLOCK TABLES;
LOCK TABLES performance_schema.threads WRITE;
UNLOCK TABLES;
