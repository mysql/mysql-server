SELECT * FROM performance_schema.replication_applier_status
LIMIT 1;
SELECT * FROM performance_schema.replication_applier_status
WHERE service_state='YES' OR remaining_delay=12;
INSERT INTO performance_schema.replication_applier_status
SET service_state='YES', remaining_delay=12;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_applier_status'
UPDATE performance_schema.replication_applier_status
SET remaining_delay=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_applier_status'
UPDATE performance_schema.replication_applier_status
SET remaining_delay=12 WHERE service_state='YES';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_applier_status'
DELETE FROM performance_schema.replication_applier_status
WHERE service_state='YES';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_status'
DELETE FROM performance_schema.replication_applier_status;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_status'
LOCK TABLES performance_schema.replication_applier_status READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_applier_status'
UNLOCK TABLES;
LOCK TABLES performance_schema.replication_applier_status WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_applier_status'
UNLOCK TABLES;
