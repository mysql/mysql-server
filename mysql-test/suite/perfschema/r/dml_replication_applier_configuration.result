SELECT * FROM performance_schema.replication_applier_configuration
LIMIT 1;
SELECT * FROM performance_schema.replication_applier_configuration
WHERE desired_delay=12;
SELECT * FROM performance_schema.replication_applier_configuration
WHERE privilege_checks_user=NULL;
SELECT * FROM performance_schema.replication_applier_configuration
WHERE require_row_format="NO";
SELECT * FROM performance_schema.replication_applier_configuration
WHERE require_table_primary_key_check="STREAM";
SELECT * FROM performance_schema.replication_applier_configuration
WHERE assign_gtids_to_anonymous_transactions_type="OFF";
SELECT * FROM performance_schema.replication_applier_configuration
WHERE assign_gtids_to_anonymous_transactions_value=NULL;
INSERT INTO performance_schema.replication_applier_configuration
SET desired_delay=2;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
INSERT INTO performance_schema.replication_applier_configuration
SET privilege_checks_user="'user'@'%'";
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
INSERT INTO performance_schema.replication_applier_configuration
SET require_row_format="YES";
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
INSERT INTO performance_schema.replication_applier_configuration
SET require_table_primary_key_check="ON";
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
INSERT INTO performance_schema.replication_applier_configuration
SET assign_gtids_to_anonymous_transactions="LOCAL";
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
UPDATE performance_schema.replication_applier_configuration
SET desired_delay=12 WHERE desired_delay=2;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
UPDATE performance_schema.replication_applier_configuration
SET privilege_checks_user="'user'@'%'" WHERE privilege_checks_user=NULL;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
UPDATE performance_schema.replication_applier_configuration
SET require_row_format="YES" WHERE require_row_format="NO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
UPDATE performance_schema.replication_applier_configuration
SET require_table_primary_key_check="OFF" WHERE require_table_primary_key_check="STREAM";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
UPDATE performance_schema.replication_applier_configuration
SET assign_gtids_to_anonymous_transactions="LOCAL" WHERE assign_gtids_to_anonymous_transactions="OFF";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
DELETE FROM performance_schema.replication_applier_configuration
WHERE desired_delay=12;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
DELETE FROM performance_schema.replication_applier_configuration
WHERE privilege_checks_user="'user'@'%'";
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
DELETE FROM performance_schema.replication_applier_configuration
WHERE require_row_format="NO";
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
DELETE FROM performance_schema.replication_applier_configuration
WHERE require_table_primary_key_check="STREAM";
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
DELETE FROM performance_schema.replication_applier_configuration
WHERE assign_gtids_to_anonymous_transactions_type="OFF";
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
DELETE FROM performance_schema.replication_applier_configuration
WHERE assign_gtids_to_anonymous_transactions_value="OFF";
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
DELETE FROM performance_schema.replication_applier_configuration;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
LOCK TABLES performance_schema.replication_applier_configuration READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
UNLOCK TABLES;
LOCK TABLES performance_schema.replication_applier_configuration WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_applier_configuration'
UNLOCK TABLES;
