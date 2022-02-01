alter table performance_schema.host_cache add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.host_cache;
ALTER TABLE performance_schema.host_cache ADD INDEX test_index(IP);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.host_cache(IP);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.host_cache;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
