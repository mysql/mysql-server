select * from performance_schema.memory_summary_by_account_by_event_name
where event_name like 'memory/%' limit 1;
select * from performance_schema.memory_summary_by_account_by_event_name
where event_name='FOO';
insert into performance_schema.memory_summary_by_account_by_event_name
set event_name='FOO', user='BAR', host='BAZ',
count_alloc=1, count_free=2, sum_number_of_bytes_alloc=3,
sum_number_of_bytes_free=4, low_count_used=5;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'memory_summary_by_account_by_event_name'
update performance_schema.memory_summary_by_account_by_event_name
set count_alloc=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'memory_summary_by_account_by_event_name'
update performance_schema.memory_summary_by_account_by_event_name
set count_alloc=12 where event_name like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'memory_summary_by_account_by_event_name'
delete from performance_schema.memory_summary_by_account_by_event_name
where count_alloc=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'memory_summary_by_account_by_event_name'
delete from performance_schema.memory_summary_by_account_by_event_name;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'memory_summary_by_account_by_event_name'
LOCK TABLES performance_schema.memory_summary_by_account_by_event_name READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'memory_summary_by_account_by_event_name'
UNLOCK TABLES;
LOCK TABLES performance_schema.memory_summary_by_account_by_event_name WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'memory_summary_by_account_by_event_name'
UNLOCK TABLES;
