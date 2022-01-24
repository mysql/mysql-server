# ----------------------------------------------------------------------
# Setup
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
# Creating local configuration file for keyring component: component_keyring_file
# Creating manifest file for current MySQL server instance
[connection slave]
# Creating local configuration file for keyring component: component_keyring_file
# Creating manifest file for current MySQL server instance
[connection master]
# ----------------------------------------------------------------------
include/rpl_stop_server.inc [server_number=1]
include/rpl_start_server.inc [server_number=1]
include/rpl_stop_server.inc [server_number=2]
include/rpl_start_server.inc [server_number=2]
[connection slave]
include/start_slave.inc
[connection master]
CREATE TABLE t1 (c1 TEXT) ENGINE=INNODB;
# Adding debug point 'ensure_binlog_cache_temporary_file_is_encrypted' to @@GLOBAL.debug
INSERT INTO t1 VALUES (REPEAT('123', 16384.0));
# Removing debug point 'ensure_binlog_cache_temporary_file_is_encrypted' from @@GLOBAL.debug
SET GLOBAL binlog_encryption=OFF;
# Adding debug point 'ensure_binlog_cache_temp_file_encryption_is_disabled' to @@GLOBAL.debug
INSERT INTO t1 VALUES (REPEAT('off', 16384.0));
# Removing debug point 'ensure_binlog_cache_temp_file_encryption_is_disabled' from @@GLOBAL.debug
SET GLOBAL binlog_encryption=ON;
# Adding debug point 'ensure_binlog_cache_temporary_file_is_encrypted' to @@GLOBAL.debug
INSERT INTO t1 VALUES (REPEAT('on1', 16384.0));
# Removing debug point 'ensure_binlog_cache_temporary_file_is_encrypted' from @@GLOBAL.debug
# Adding debug point 'ensure_binlog_cache_is_reset' to @@GLOBAL.debug
INSERT INTO t1 VALUES ("567");
BEGIN;
INSERT INTO t1 VALUES ("789");
ROLLBACK;
# Removing debug point 'ensure_binlog_cache_is_reset' from @@GLOBAL.debug
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:t1,slave:t1]
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
[connection master]
SET GLOBAL binlog_encryption= OFF;
[connection slave]
SET GLOBAL binlog_encryption= OFF;
SET GLOBAL keyring_operations= ON;
# ----------------------------------------------------------------------
# Teardown
# Removing manifest file for current MySQL server instance
# Removing manifest file for current MySQL server instance
# Removing local keyring file for keyring component: component_keyring_file
# Removing local keyring file for keyring component: component_keyring_file
# Removing local configuration file for keyring component: component_keyring_file
# Removing local configuration file for keyring component: component_keyring_file
include/rpl_end.inc
# Restarting server without the manifest file
# ----------------------------------------------------------------------
