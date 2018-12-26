select * from performance_schema.events_statements_current
where event_name like 'statement/%' limit 1;
select * from performance_schema.events_statements_current
where event_name='FOO';
insert into performance_schema.events_statements_current
set thread_id='1', event_id=1,
event_name='FOO', timer_start=1, timer_end=2, timer_wait=3;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'events_statements_current'
update performance_schema.events_statements_current
set timer_start=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_statements_current'
update performance_schema.events_statements_current
set timer_start=12 where thread_id=0;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_statements_current'
delete from performance_schema.events_statements_current
where thread_id=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_statements_current'
delete from performance_schema.events_statements_current;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_statements_current'
LOCK TABLES performance_schema.events_statements_current READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_statements_current'
UNLOCK TABLES;
LOCK TABLES performance_schema.events_statements_current WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_statements_current'
UNLOCK TABLES;
