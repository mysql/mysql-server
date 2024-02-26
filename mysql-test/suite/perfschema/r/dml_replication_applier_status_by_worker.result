SELECT * FROM performance_schema.replication_applier_status_by_worker
LIMIT 1;
SELECT * FROM performance_schema.replication_applier_status_by_worker
WHERE service_state='YES' OR last_error_message='ERROR';
INSERT INTO performance_schema.replication_applier_status_by_worker
SET service_state='YES', last_error_message='ERROR',
worker_id=1, thread_id=2, last_error_number=1234;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_applier_status_by_worker'
UPDATE performance_schema.replication_applier_status_by_worker
SET worker_id=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_applier_status_by_worker'
UPDATE performance_schema.replication_applier_status_by_worker
SET worker_d=12 where service_state like "YES";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_applier_status_by_worker'
DELETE FROM performance_schema.replication_applier_status_by_worker
WHERE worker_id=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_status_by_worker'
DELETE FROM performance_schema.replication_applier_status_by_worker;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_status_by_worker'
LOCK TABLES performance_schema.replication_applier_status_by_worker READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_applier_status_by_worker'
UNLOCK TABLES;
LOCK TABLES performance_schema.replication_applier_status_by_worker WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_applier_status_by_worker'
UNLOCK TABLES;
