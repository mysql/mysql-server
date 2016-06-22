select * from performance_schema.events_errors_summary_by_host_by_error limit 1;
select * from performance_schema.events_errors_summary_by_host_by_error;
insert into performance_schema.events_errors_summary_by_host_by_error
set error_name='FOO', sum_error_raised=1;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'events_errors_summary_by_host_by_error'
update performance_schema.events_errors_summary_by_host_by_error
set sum_error_raised=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_errors_summary_by_host_by_error'
update performance_schema.events_errors_summary_by_host_by_error
set sum_error_raised=12 where error_name like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_errors_summary_by_host_by_error'
delete from performance_schema.events_errors_summary_by_host_by_error
where sum_error_raised=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_errors_summary_by_host_by_error'
delete from performance_schema.events_errors_summary_by_host_by_error;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_errors_summary_by_host_by_error'
LOCK TABLES performance_schema.events_errors_summary_by_host_by_error READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_errors_summary_by_host_by_error'
UNLOCK TABLES;
LOCK TABLES performance_schema.events_errors_summary_by_host_by_error WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_errors_summary_by_host_by_error'
UNLOCK TABLES;
