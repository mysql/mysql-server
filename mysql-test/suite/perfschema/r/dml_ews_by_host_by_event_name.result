select * from performance_schema.events_waits_summary_by_host_by_event_name
where event_name like 'Wait/Synch/%' limit 1;
HOST	EVENT_NAME	COUNT_STAR	SUM_TIMER_WAIT	MIN_TIMER_WAIT	AVG_TIMER_WAIT	MAX_TIMER_WAIT
#	#	#	#	#	#	#
select * from performance_schema.events_waits_summary_by_host_by_event_name
where event_name='FOO';
HOST	EVENT_NAME	COUNT_STAR	SUM_TIMER_WAIT	MIN_TIMER_WAIT	AVG_TIMER_WAIT	MAX_TIMER_WAIT
insert into performance_schema.events_waits_summary_by_host_by_event_name
set event_name='FOO', thread_id=1,
count_star=1, sum_timer_wait=2, min_timer_wait=3,
avg_timer_wait=4, max_timer_wait=5;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'events_waits_summary_by_host_by_event_name'
update performance_schema.events_waits_summary_by_host_by_event_name
set count_star=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_waits_summary_by_host_by_event_name'
update performance_schema.events_waits_summary_by_host_by_event_name
set count_star=12 where event_name like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'events_waits_summary_by_host_by_event_name'
delete from performance_schema.events_waits_summary_by_host_by_event_name
where count_star=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_waits_summary_by_host_by_event_name'
delete from performance_schema.events_waits_summary_by_host_by_event_name;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'events_waits_summary_by_host_by_event_name'
LOCK TABLES performance_schema.events_waits_summary_by_host_by_event_name READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_waits_summary_by_host_by_event_name'
UNLOCK TABLES;
LOCK TABLES performance_schema.events_waits_summary_by_host_by_event_name WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'events_waits_summary_by_host_by_event_name'
UNLOCK TABLES;
