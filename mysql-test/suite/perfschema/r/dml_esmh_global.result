select * from performance_schema.events_statements_histogram_global
where bucket_number = 1000 limit 1;
select * from performance_schema.events_statements_histogram_global
where count_bucket=0;
insert into performance_schema.events_statements_histogram_global
set digest='XXYYZZ', count_star=1, sum_timer_wait=2, min_timer_wait=3,
avg_timer_wait=4, max_timer_wait=5;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'events_statements_histogram_global'
update performance_schema.events_statements_histogram_global
set count_star=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_statements_histogram_global'
update performance_schema.events_statements_histogram_global
set count_star=12 where digest like "XXYYZZ";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_statements_histogram_global'
delete from performance_schema.events_statements_histogram_global
where count_star=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_statements_histogram_global'
delete from performance_schema.events_statements_histogram_global;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_statements_histogram_global'
LOCK TABLES performance_schema.events_statements_histogram_global READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_statements_histogram_global'
UNLOCK TABLES;
LOCK TABLES performance_schema.events_statements_histogram_global WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_statements_histogram_global'
UNLOCK TABLES;
