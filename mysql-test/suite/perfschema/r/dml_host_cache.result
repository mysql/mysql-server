select * from performance_schema.host_cache
where IP like '%' limit 1;
select * from performance_schema.host_cache
where IP='localhost';
insert into performance_schema.host_cache
set IP='FOO', SUM_BLOCKING_ERRORS=1, COUNT_FCRDNS_ERRORS=2;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'host_cache'
update performance_schema.host_cache
set COUNT_UNKNOWN_ERRORS=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'host_cache'
update performance_schema.host_cache
set SUM_BLOCKING_ERRORS=12 where IP='127.0.0.1';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'host_cache'
select HOST from performance_schema.host_cache
where IP='::1';
HOST
delete from performance_schema.host_cache
where IP='::1';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'host_cache'
delete from performance_schema.host_cache;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'host_cache'
LOCK TABLES performance_schema.host_cache READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'host_cache'
UNLOCK TABLES;
LOCK TABLES performance_schema.host_cache WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'host_cache'
UNLOCK TABLES;
