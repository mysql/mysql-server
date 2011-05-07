select * from performance_schema.events_statements_summary_by_account_by_event_name
where event_name like 'statement/%' limit 1;
select * from performance_schema.events_statements_summary_by_account_by_event_name
where event_name='FOO';
insert into performance_schema.events_statements_summary_by_account_by_event_name
set event_name='FOO', user='BAR', host='BAZ',
count_star=1, sum_timer_wait=2, min_timer_wait=3,
avg_timer_wait=4, max_timer_wait=5;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'events_statements_summary_by_account_by_event_name'
update performance_schema.events_statements_summary_by_account_by_event_name
set count_star=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_statements_summary_by_account_by_event_name'
update performance_schema.events_statements_summary_by_account_by_event_name
set count_star=12 where event_name like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_statements_summary_by_account_by_event_name'
delete from performance_schema.events_statements_summary_by_account_by_event_name
where count_star=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_statements_summary_by_account_by_event_name'
delete from performance_schema.events_statements_summary_by_account_by_event_name;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_statements_summary_by_account_by_event_name'
LOCK TABLES performance_schema.events_statements_summary_by_account_by_event_name READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_statements_summary_by_account_by_event_name'
UNLOCK TABLES;
LOCK TABLES performance_schema.events_statements_summary_by_account_by_event_name WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_statements_summary_by_account_by_event_name'
UNLOCK TABLES;
