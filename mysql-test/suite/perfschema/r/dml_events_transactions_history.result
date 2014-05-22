select * from performance_schema.events_transactions_history
where event_name in ('transaction') limit 1;
select * from performance_schema.events_transactions_history
where event_name='FOO';
select * from performance_schema.events_transactions_history
where event_name in ('transaction') order by timer_wait limit 1;
select * from performance_schema.events_transactions_history
where event_name in ('transaction') order by timer_wait desc limit 1;
insert into performance_schema.events_transactions_history
set thread_id='1', event_id=1,
event_name='FOO', timer_start=1, timer_end=2, timer_wait=3;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'events_transactions_history'
update performance_schema.events_transactions_history
set timer_start=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_transactions_history'
update performance_schema.events_transactions_history
set timer_start=12 where thread_id=0;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_transactions_history'
delete from performance_schema.events_transactions_history
where thread_id=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_transactions_history'
delete from performance_schema.events_transactions_history;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_transactions_history'
LOCK TABLES performance_schema.events_transactions_history READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_transactions_history'
UNLOCK TABLES;
LOCK TABLES performance_schema.events_transactions_history WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_transactions_history'
UNLOCK TABLES;
