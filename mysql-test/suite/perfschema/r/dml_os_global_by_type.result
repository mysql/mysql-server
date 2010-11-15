select * from performance_schema.objects_summary_global_by_type
where object_schema = 'performance_schema' limit 1;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	COUNT_STAR	SUM_TIMER_WAIT	MIN_TIMER_WAIT	AVG_TIMER_WAIT	MAX_TIMER_WAIT
#	#	#	#	#	#	#	#
select * from performance_schema.objects_summary_global_by_type
where object_schema='FOO';
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	COUNT_STAR	SUM_TIMER_WAIT	MIN_TIMER_WAIT	AVG_TIMER_WAIT	MAX_TIMER_WAIT
select * from performance_schema.objects_summary_global_by_type
order by count_star limit 1;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	COUNT_STAR	SUM_TIMER_WAIT	MIN_TIMER_WAIT	AVG_TIMER_WAIT	MAX_TIMER_WAIT
#	#	#	#	#	#	#	#
select * from performance_schema.objects_summary_global_by_type
order by count_star desc limit 1;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	COUNT_STAR	SUM_TIMER_WAIT	MIN_TIMER_WAIT	AVG_TIMER_WAIT	MAX_TIMER_WAIT
#	#	#	#	#	#	#	#
select * from performance_schema.objects_summary_global_by_type
where object_type = 'TABLE' order by object_schema, object_name limit 1;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	COUNT_STAR	SUM_TIMER_WAIT	MIN_TIMER_WAIT	AVG_TIMER_WAIT	MAX_TIMER_WAIT
#	#	#	#	#	#	#	#
select * from performance_schema.objects_summary_global_by_type
where object_type = 'TABLE' order by object_schema desc, object_name desc limit 1;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	COUNT_STAR	SUM_TIMER_WAIT	MIN_TIMER_WAIT	AVG_TIMER_WAIT	MAX_TIMER_WAIT
#	#	#	#	#	#	#	#
insert into performance_schema.objects_summary_global_by_type
set object_type='TABLE', schema_name='FOO', object_name='BAR',
count_star=1, sum_timer_wait=2, min_timer_wait=3,
avg_timer_wait=4, max_timer_wait=5;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'objects_summary_global_by_type'
update performance_schema.objects_summary_global_by_type
set count_star=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'objects_summary_global_by_type'
update performance_schema.objects_summary_global_by_type
set count_star=12 where object_name like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'objects_summary_global_by_type'
delete from performance_schema.objects_summary_global_by_type
where count_star=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'objects_summary_global_by_type'
delete from performance_schema.objects_summary_global_by_type;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'objects_summary_global_by_type'
LOCK TABLES performance_schema.objects_summary_global_by_type READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'objects_summary_global_by_type'
UNLOCK TABLES;
LOCK TABLES performance_schema.objects_summary_global_by_type WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'objects_summary_global_by_type'
UNLOCK TABLES;
