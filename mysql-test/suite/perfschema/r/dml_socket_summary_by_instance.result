select * from performance_schema.socket_summary_by_instance
limit 1;
select * from performance_schema.socket_summary_by_instance
where object_instance_begin='FOO';
insert into performance_schema.socket_summary_by_instance
set object_instance_begin=1,
count_star=1, sum_timer_wait=2, min_timer_wait=3,
avg_timer_wait=4, max_timer_wait=5;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'socket_summary_by_instance'
update performance_schema.socket_summary_by_instance
set count_star=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'socket_summary_by_instance'
update performance_schema.socket_summary_by_instance
set count_star=12 where object_instance_begin like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'socket_summary_by_instance'
delete from performance_schema.socket_summary_by_instance
where count_star=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'socket_summary_by_instance'
delete from performance_schema.socket_summary_by_instance;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'socket_summary_by_instance'
LOCK TABLES performance_schema.socket_summary_by_instance READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'socket_summary_by_instance'
UNLOCK TABLES;
LOCK TABLES performance_schema.socket_summary_by_instance WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'socket_summary_by_instance'
UNLOCK TABLES;
