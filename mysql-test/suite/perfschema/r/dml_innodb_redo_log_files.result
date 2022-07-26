select * from performance_schema.innodb_redo_log_files;
select * from performance_schema.innodb_redo_log_files
where file_name='foo';
FILE_ID	FILE_NAME	START_LSN	END_LSN	SIZE_IN_BYTES	IS_FULL	CONSUMER_LEVEL
insert into performance_schema.innodb_redo_log_files
set file_name='#ib_redo1', file_id=1, start_lsn=8192, end_lsn=10240, size_in_bytes=4096,
is_full='YES', consumer_level=0;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'innodb_redo_log_files'
update performance_schema.innodb_redo_log_files
set file_name='#ib_redo2' where file_id=1;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'innodb_redo_log_files'
delete from performance_schema.innodb_redo_log_files;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'innodb_redo_log_files'
delete from performance_schema.innodb_redo_log_files
where file_name='#ib_redo1';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'innodb_redo_log_files'
LOCK TABLES performance_schema.innodb_redo_log_files READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'innodb_redo_log_files'
UNLOCK TABLES;
LOCK TABLES performance_schema.innodb_redo_log_files WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'innodb_redo_log_files'
UNLOCK TABLES;
