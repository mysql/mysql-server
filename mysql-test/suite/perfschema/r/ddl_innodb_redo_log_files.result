alter table performance_schema.innodb_redo_log_files add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.innodb_redo_log_files;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'innodb_redo_log_files'
ALTER TABLE performance_schema.innodb_redo_log_files ADD INDEX test_index(IS_FULL);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.innodb_redo_log_files(FILE_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
