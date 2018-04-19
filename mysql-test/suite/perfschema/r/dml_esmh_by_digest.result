select * from performance_schema.events_statements_histogram_by_digest
where digest like 'XXYYZZ%' limit 1;
SCHEMA_NAME	DIGEST	BUCKET_NUMBER	BUCKET_TIMER_LOW	BUCKET_TIMER_HIGH	COUNT_BUCKET	COUNT_BUCKET_AND_LOWER	BUCKET_QUANTILE
select * from performance_schema.events_statements_histogram_by_digest
where digest='XXYYZZ';
SCHEMA_NAME	DIGEST	BUCKET_NUMBER	BUCKET_TIMER_LOW	BUCKET_TIMER_HIGH	COUNT_BUCKET	COUNT_BUCKET_AND_LOWER	BUCKET_QUANTILE
insert into performance_schema.events_statements_histogram_by_digest
set digest='XXYYZZ', count_star=1, sum_timer_wait=2, min_timer_wait=3,
avg_timer_wait=4, max_timer_wait=5;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'events_statements_histogram_by_digest'
update performance_schema.events_statements_histogram_by_digest
set count_star=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_statements_histogram_by_digest'
update performance_schema.events_statements_histogram_by_digest
set count_star=12 where digest like "XXYYZZ";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_statements_histogram_by_digest'
delete from performance_schema.events_statements_histogram_by_digest
where count_star=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_statements_histogram_by_digest'
delete from performance_schema.events_statements_histogram_by_digest;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_statements_histogram_by_digest'
LOCK TABLES performance_schema.events_statements_histogram_by_digest READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_statements_histogram_by_digest'
UNLOCK TABLES;
LOCK TABLES performance_schema.events_statements_histogram_by_digest WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_statements_histogram_by_digest'
UNLOCK TABLES;
