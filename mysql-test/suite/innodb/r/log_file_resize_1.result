SET GLOBAL innodb_log_checkpoint_now = 1;
# Initial state:
SELECT variable_name, variable_value FROM performance_schema.global_status
WHERE variable_name = 'Innodb_redo_log_capacity_resized';
variable_name	variable_value
Innodb_redo_log_capacity_resized	10485760
# Resize redo log to 100M:
SET GLOBAL innodb_redo_log_capacity = 100*1024*1024;
Pattern "User has set innodb_redo_log_capacity to 100M" found
Pattern "Redo log has been resized to 100M" found
# Current state:
SELECT variable_name, variable_value FROM performance_schema.global_status
WHERE variable_name = 'Innodb_redo_log_capacity_resized';
variable_name	variable_value
Innodb_redo_log_capacity_resized	104857600
# Create connection which generates spam to the redo log.
CALL log_spammer();
# Disable checkpointing to ensure redo would grow and to allow test of redo-resize cancellation.
SET GLOBAL innodb_checkpoint_disabled = 1;
# Wait until redo occupies at least 10MB
# Start resizing down the redo log (spammer is working in background)
SET GLOBAL innodb_redo_log_capacity = 9*1024*1024;
Pattern "User has set innodb_redo_log_capacity to 9M" found
Pattern "Redo log has been requested to resize from 100M to 9M" found
# Expected - not found:
Pattern "Redo log has been resized to 9M" not found
# Checkpoint is still disabled so resize hasn't been finished. Cancel the resize.
SET GLOBAL innodb_redo_log_capacity = 102*1024*1024;
Pattern "User has set innodb_redo_log_capacity to 102M" found
Pattern "Redo log resize has been cancelled" found
Pattern "Redo log has been resized to 102M" found
# Restart resizing down.
SET GLOBAL innodb_redo_log_capacity = 8*1024*1024;
Pattern "User has set innodb_redo_log_capacity to 8M" found
Pattern "Redo log has been requested to resize from 102M to 8M" found
# Expected - not found:
Pattern "Redo log has been resized to 8M" not found
SET GLOBAL innodb_checkpoint_disabled = 0;
# Waiting for status = OK....
Pattern "Redo log has been resized to 8M" found
# Current state:
SELECT variable_name, variable_value FROM performance_schema.global_status
WHERE variable_name = 'Innodb_redo_log_capacity_resized';
variable_name	variable_value
Innodb_redo_log_capacity_resized	8388608
Checking if total size of redo log files does not exceed 8388608 bytes
# Kill and restart mysql:
# Kill and restart
# Initial state:
SELECT variable_name, variable_value FROM performance_schema.global_status
WHERE variable_name = 'Innodb_redo_log_capacity_resized';
variable_name	variable_value
Innodb_redo_log_capacity_resized	10485760
# Resize redo log to 500M:
SET GLOBAL innodb_redo_log_capacity = 500*1024*1024;
# Current state:
SELECT variable_name, variable_value FROM performance_schema.global_status
WHERE variable_name = 'Innodb_redo_log_capacity_resized';
variable_name	variable_value
Innodb_redo_log_capacity_resized	524288000
# Create connection which generates spam to the redo log.
CALL log_spammer();
# Wait until redo occupies at least 10MB
# Kill and restart mysql with --innodb-redo-log-capacity=1G:
# Kill and restart: --innodb-redo-log-capacity=1G
# Initial state:
SELECT variable_name, variable_value FROM performance_schema.global_status
WHERE variable_name = 'Innodb_redo_log_capacity_resized';
variable_name	variable_value
Innodb_redo_log_capacity_resized	1073741824
# Start resizing down the redo log (spammer is NOT working in background)
SET GLOBAL innodb_redo_log_capacity = 8*1024*1024;
# Waiting for status = OK....
# Current state:
SELECT variable_name, variable_value FROM performance_schema.global_status
WHERE variable_name = 'Innodb_redo_log_capacity_resized';
variable_name	variable_value
Innodb_redo_log_capacity_resized	8388608
Checking if total size of redo log files does not exceed 8388608 bytes
# Create connection which generates spam to the redo log.
CALL log_spammer();
# Resize redo log to 1GB:
SET GLOBAL innodb_redo_log_capacity = 1024*1024*1024;
# Current state:
SELECT variable_name, variable_value FROM performance_schema.global_status
WHERE variable_name = 'Innodb_redo_log_capacity_resized';
variable_name	variable_value
Innodb_redo_log_capacity_resized	1073741824
# Wait until redo occupies at least 20MB
# Kill and restart mysql without --innodb-redo-log-capacity
# Kill and restart:
Pattern "Redo log has been requested to resize from .* to .*M" found
