select * from performance_schema.file_summary_by_event_name
where event_name like 'Wait/io/%' limit 1;
select * from performance_schema.file_summary_by_event_name
where event_name='FOO';
insert into performance_schema.file_summary_by_event_name
set event_name='FOO', count_read=1, count_write=2,
sum_number_of_bytes_read=4, sum_number_of_bytes_write=5;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'file_summary_by_event_name'
update performance_schema.file_summary_by_event_name
set count_read=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'file_summary_by_event_name'
update performance_schema.file_summary_by_event_name
set count_write=12 where event_name like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'file_summary_by_event_name'
delete from performance_schema.file_summary_by_event_name
where count_read=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'file_summary_by_event_name'
delete from performance_schema.file_summary_by_event_name;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'file_summary_by_event_name'
LOCK TABLES performance_schema.file_summary_by_event_name READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'file_summary_by_event_name'
UNLOCK TABLES;
LOCK TABLES performance_schema.file_summary_by_event_name WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'file_summary_by_event_name'
UNLOCK TABLES;
