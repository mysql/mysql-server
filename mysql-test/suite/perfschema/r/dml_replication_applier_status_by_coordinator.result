SELECT * FROM performance_schema.replication_applier_status_by_coordinator
LIMIT 1;
SELECT * FROM performance_schema.replication_applier_status_by_coordinator
WHERE service_state='YES' or last_error_message='ERROR';
INSERT INTO performance_schema.replication_applier_status_by_coordinator
SET service_state='YES', last_error_message='ERROR', thread_id=2,
last_error_number=1234;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_applier_status_by_coordinator'
UPDATE performance_schema.replication_applier_status_by_coordinator
SET last_error_number=1234;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_applier_status_by_coordinator'
UPDATE performance_schema.replication_applier_status_by_coordinator
SET last_error_number=1234 where service_state like "YES";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_applier_status_by_coordinator'
DELETE FROM performance_schema.replication_applier_status_by_coordinator
WHERE thread_id=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_status_by_coordinator'
DELETE FROM performance_schema.replication_applier_status_by_coordinator;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_status_by_coordinator'
LOCK TABLES performance_schema.replication_applier_status_by_coordinator READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_applier_status_by_coordinator'
UNLOCK TABLES;
LOCK TABLES performance_schema.replication_applier_status_by_coordinator WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_applier_status_by_coordinator'
UNLOCK TABLES;
