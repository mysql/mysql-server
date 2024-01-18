# ==== Purpose ====
#
# 1. Verify that the temporary file of binlog cache is encrypted when
#    the binlog cache spills to disk if binlog_encryption is on.
# 2. Verify that 'SET GLOBAL binlog_encryption' takes effect immediately
#    on binlog cache temporary file encryption.
# 3. Verify that binlog cache is reset at commit/rollback time.
#
# ==== Implementation ====
#
# 1. Start the master with binlog_encryption on.
# 2. Create a table t1 with a TEXT column.
# 3. Execute a trx to insert a big text into the tables to make
#    binlog cache spill to disk and ensure that the temporary
#    file of binlog cache is encrypted.
# 4. SET GLOBAL binlog_encryption=OFF
# 5. Execute a trx to ensure binlog cache temporary file encryption
#    is disabled.
# 6. SET GLOBAL binlog_encryption=ON
# 7. Execute a trx to insert a big text into the tables to make
#    binlog cache spill to disk and ensure that the temporary
#    file of binlog cache is encrypted.
# 8. Commit a trx ensure that binlog cache is reset at commit time.
# 9. Rollback a trx ensure that binlog cache is reset at rollback time.
# 10. Verify that there is no difference on the table between
#     master and slave.
#
# ==== References ====
#
# Wl#12079  Encrypt binary log caches at rest
# Bug#29475234  'BINLOG_ENCRYPTION' VALUE WONT TAKE IMMEDIATE EFFECT ON ENCRYPTING BINLOG CACHES

--disable_query_log
call mtr.add_suppression("Could not find the data corresponding to Data ID:");
--enable_query_log

# Restart master and slave with keyring

--let $rpl_server_number= 1
--source include/rpl/stop_server.inc
--let $rpl_server_error= 0
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --binlog_encryption=ON
--let $rpl_omit_print_server_parameters= 1
--source include/rpl/start_server.inc
--let $rpl_server_number= 2
--source include/rpl/stop_server.inc
--let $rpl_server_error= 0
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --binlog_encryption=ON
--let $rpl_omit_print_server_parameters= 1
--source include/rpl/start_server.inc
--source include/rpl/connection_replica.inc
--source include/rpl/start_replica.inc

--source include/rpl/connection_source.inc
--let $data_size= `select 0.5 * @@global.binlog_cache_size`

CREATE TABLE t1 (c1 TEXT) ENGINE=INNODB;

--let $debug_point= ensure_binlog_cache_temporary_file_is_encrypted
--source include/add_debug_point.inc

eval INSERT INTO t1 VALUES (REPEAT('123', $data_size));

--let $debug_point= ensure_binlog_cache_temporary_file_is_encrypted
--source include/remove_debug_point.inc

SET GLOBAL binlog_encryption=OFF;
--let $debug_point= ensure_binlog_cache_temp_file_encryption_is_disabled
--source include/add_debug_point.inc

eval INSERT INTO t1 VALUES (REPEAT('off', $data_size));

--let $debug_point= ensure_binlog_cache_temp_file_encryption_is_disabled
--source include/remove_debug_point.inc

SET GLOBAL binlog_encryption=ON;
--let $debug_point= ensure_binlog_cache_temporary_file_is_encrypted
--source include/add_debug_point.inc

eval INSERT INTO t1 VALUES (REPEAT('on1', $data_size));

--let $debug_point= ensure_binlog_cache_temporary_file_is_encrypted
--source include/remove_debug_point.inc

--let $debug_point= ensure_binlog_cache_is_reset
--source include/add_debug_point.inc

INSERT INTO t1 VALUES ("567");

BEGIN;
INSERT INTO t1 VALUES ("789");
ROLLBACK;


--let $debug_point= ensure_binlog_cache_is_reset
--source include/remove_debug_point.inc

--source include/rpl/sync_to_replica.inc
--let $diff_tables=master:t1,slave:t1
--source include/diff_tables.inc

# Cleanup
--source include/rpl/connection_source.inc
DROP TABLE t1;
--source include/rpl/sync_to_replica.inc
--source include/rpl/connection_source.inc
SET GLOBAL binlog_encryption= OFF;
--source include/rpl/connection_replica.inc
SET GLOBAL binlog_encryption= OFF;
SET GLOBAL keyring_operations= ON;
