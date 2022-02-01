select * from performance_schema.prepared_statements_instances
where owner_object_name like 'XXYYZZ%' limit 1;
OBJECT_INSTANCE_BEGIN	STATEMENT_ID	STATEMENT_NAME	SQL_TEXT	OWNER_THREAD_ID	OWNER_EVENT_ID	OWNER_OBJECT_TYPE	OWNER_OBJECT_SCHEMA	OWNER_OBJECT_NAME	TIMER_PREPARE	COUNT_REPREPARE	COUNT_EXECUTE	SUM_TIMER_EXECUTE	MIN_TIMER_EXECUTE	AVG_TIMER_EXECUTE	MAX_TIMER_EXECUTE	SUM_LOCK_TIME	SUM_ERRORS	SUM_WARNINGS	SUM_ROWS_AFFECTED	SUM_ROWS_SENT	SUM_ROWS_EXAMINED	SUM_CREATED_TMP_DISK_TABLES	SUM_CREATED_TMP_TABLES	SUM_SELECT_FULL_JOIN	SUM_SELECT_FULL_RANGE_JOIN	SUM_SELECT_RANGE	SUM_SELECT_RANGE_CHECK	SUM_SELECT_SCAN	SUM_SORT_MERGE_PASSES	SUM_SORT_RANGE	SUM_SORT_ROWS	SUM_SORT_SCAN	SUM_NO_INDEX_USED	SUM_NO_GOOD_INDEX_USED
select * from performance_schema.prepared_statements_instances
where owner_object_name='XXYYZZ';
OBJECT_INSTANCE_BEGIN	STATEMENT_ID	STATEMENT_NAME	SQL_TEXT	OWNER_THREAD_ID	OWNER_EVENT_ID	OWNER_OBJECT_TYPE	OWNER_OBJECT_SCHEMA	OWNER_OBJECT_NAME	TIMER_PREPARE	COUNT_REPREPARE	COUNT_EXECUTE	SUM_TIMER_EXECUTE	MIN_TIMER_EXECUTE	AVG_TIMER_EXECUTE	MAX_TIMER_EXECUTE	SUM_LOCK_TIME	SUM_ERRORS	SUM_WARNINGS	SUM_ROWS_AFFECTED	SUM_ROWS_SENT	SUM_ROWS_EXAMINED	SUM_CREATED_TMP_DISK_TABLES	SUM_CREATED_TMP_TABLES	SUM_SELECT_FULL_JOIN	SUM_SELECT_FULL_RANGE_JOIN	SUM_SELECT_RANGE	SUM_SELECT_RANGE_CHECK	SUM_SELECT_SCAN	SUM_SORT_MERGE_PASSES	SUM_SORT_RANGE	SUM_SORT_ROWS	SUM_SORT_SCAN	SUM_NO_INDEX_USED	SUM_NO_GOOD_INDEX_USED
insert into performance_schema.prepared_statements_instances
set owner_object_name='XXYYZZ', count_execute=1, sum_timer_execute=2,
min_timer_execute=3, avg_timer_execute=4, max_timer_execute=5;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'prepared_statements_instances'
update performance_schema.prepared_statements_instances set count_execute=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'prepared_statements_instances'
update performance_schema.prepared_statements_instances
set count_execute=12 where owner_object_name like "XXYYZZ";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'prepared_statements_instances'
delete from performance_schema.prepared_statements_instances
where count_execute=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'prepared_statements_instances'
delete from performance_schema.prepared_statements_instances;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'prepared_statements_instances'
LOCK TABLES performance_schema.prepared_statements_instances READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'prepared_statements_instances'
UNLOCK TABLES;
LOCK TABLES performance_schema.prepared_statements_instances WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'prepared_statements_instances'
UNLOCK TABLES;
