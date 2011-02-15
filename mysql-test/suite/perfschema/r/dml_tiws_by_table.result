select * from performance_schema.table_io_waits_summary_by_table
limit 1;
select * from performance_schema.table_io_waits_summary_by_table
where object_name='FOO';
insert into performance_schema.table_io_waits_summary_by_table
set object_type='TABLE', object_name='FOO', object_schema='BAR',
count_star=1, sum_timer_wait=2, min_timer_wait=3,
avg_timer_wait=4, max_timer_wait=5;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'table_io_waits_summary_by_table'
update performance_schema.table_io_waits_summary_by_table
set count_star=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'table_io_waits_summary_by_table'
update performance_schema.table_io_waits_summary_by_table
set count_star=12 where object_name like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'table_io_waits_summary_by_table'
delete from performance_schema.table_io_waits_summary_by_table
where count_star=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'table_io_waits_summary_by_table'
delete from performance_schema.table_io_waits_summary_by_table;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'table_io_waits_summary_by_table'
LOCK TABLES performance_schema.table_io_waits_summary_by_table READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'table_io_waits_summary_by_table'
UNLOCK TABLES;
LOCK TABLES performance_schema.table_io_waits_summary_by_table WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'table_io_waits_summary_by_table'
UNLOCK TABLES;
