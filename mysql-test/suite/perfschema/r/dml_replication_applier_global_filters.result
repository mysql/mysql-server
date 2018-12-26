SELECT * FROM performance_schema.replication_applier_global_filters
LIMIT 1;
INSERT INTO performance_schema.replication_applier_global_filters
SET COUNTER=2;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_applier_global_filters'
UPDATE performance_schema.replication_applier_global_filters
SET COUNTER=12 WHERE COUNTER=0;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_applier_global_filters'
DELETE FROM performance_schema.replication_applier_global_filters
WHERE COUNTER=0;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_global_filters'
DELETE FROM performance_schema.replication_applier_global_filters;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_global_filters'
LOCK TABLES performance_schema.replication_applier_global_filters READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_applier_global_filters'
UNLOCK TABLES;
LOCK TABLES performance_schema.replication_applier_global_filters WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_applier_global_filters'
UNLOCK TABLES;
