
--echo #
--echo # WL#9787: Allow SET PERSIST to set read-only variables too
--echo #

# Save the initial number of concurrent sessions
--source include/count_sessions.inc
# This test is also meant to check read-only persisted value of log-replica-updates,
# thus, it makes sense to run this test when log-bin and log-replica-updates are enabled.
-- source include/have_log_bin.inc

--echo # Syntax check for PERSIST_ONLY option
SET PERSIST_ONLY autocommit=0;
SET @@persist_only.max_execution_time=60000;
SET PERSIST_ONLY max_user_connections=10, PERSIST_ONLY max_allowed_packet=8388608;
SET @@persist_only.max_user_connections=10, PERSIST_ONLY max_allowed_packet=8388608;
SET @@persist_only.max_user_connections=10, @@persist_only.max_allowed_packet=8388608;
SET PERSIST_ONLY max_user_connections=10, @@persist_only.max_allowed_packet=8388608;

--echo # Syntax check for PERSIST_ONLY/GLOBAL combination
SET PERSIST_ONLY autocommit=0, GLOBAL max_user_connections=10;
SET @@persist_only.autocommit=0, @@global.max_user_connections=10;
SET GLOBAL autocommit=0, PERSIST_ONLY max_user_connections=10;
SET @@global.autocommit=0, @@persist_only.max_user_connections=10;

--echo # Syntax check for PERSIST_ONLY/SESSION combination
SET PERSIST_ONLY autocommit=0, SESSION auto_increment_offset=10;
SET @@persist_only.autocommit=0, @@session.auto_increment_offset=10;
SET SESSION auto_increment_offset=20, PERSIST_ONLY max_user_connections=10;
SET @@session.auto_increment_offset=20, @@persist_only.max_user_connections=10;
SET PERSIST_ONLY autocommit=0, auto_increment_offset=10;
SET autocommit=0, PERSIST_ONLY auto_increment_offset=10;

--echo # Syntax check for PERSIST_ONLY/SESSION/GLOBAL combination
SET PERSIST_ONLY autocommit=0, SESSION auto_increment_offset=10, GLOBAL max_error_count= 128;
SET SESSION autocommit=0, GLOBAL auto_increment_offset=10, PERSIST_ONLY max_allowed_packet=8388608;
SET GLOBAL autocommit=0, PERSIST_ONLY auto_increment_offset=10, SESSION max_error_count= 128;
SET @@persist_only.autocommit=0, @@session.auto_increment_offset=10, @@global.max_allowed_packet=8388608;
SET @@session.autocommit=0, @@global.auto_increment_offset=10, @@persist_only.max_allowed_packet=8388608;
SET @@global.autocommit=0, @@persist_only.auto_increment_offset=10, @@session.max_error_count= 128;

--echo # Syntax check for PERSIST_ONLY/SESSION/GLOBAL/PERSIST combination
SET PERSIST_ONLY autocommit=0, SESSION auto_increment_offset=10, GLOBAL max_error_count= 128, PERSIST sort_buffer_size=256000;
SET SESSION autocommit=0, GLOBAL auto_increment_offset=10, PERSIST_ONLY max_allowed_packet=8388608, PERSIST max_heap_table_size=999424;
SET GLOBAL autocommit=0, PERSIST long_query_time= 8.3452, PERSIST_ONLY auto_increment_offset=10, SESSION max_error_count= 128;
SET @@persist_only.autocommit=0, @@session.auto_increment_offset=10, @@persist.max_execution_time=44000, @@global.max_allowed_packet=8388608;
SET @@persist.concurrent_insert=ALWAYS, @@session.autocommit=0, @@global.auto_increment_offset=10, @@persist_only.max_allowed_packet=8388608;
SET @@global.autocommit=0, @@persist_only.auto_increment_offset=10, @@persist.autocommit=0, @@session.max_error_count= 128;

let $MYSQLD_DATADIR= `select @@datadir`;
--remove_file $MYSQLD_DATADIR/mysqld-auto.cnf

--echo # Restart server
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

CALL mtr.add_suppression("You need to use --log-bin to make --log-replica-updates work.");

--echo # default values
SELECT @@global.binlog_gtid_simple_recovery;
SELECT VARIABLE_NAME FROM performance_schema.variables_info
  WHERE VARIABLE_SOURCE = 'PERSISTED';
SET PERSIST_ONLY binlog_gtid_simple_recovery=0;

--echo # Restart server and check for values
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # after restart
SELECT @@global.binlog_gtid_simple_recovery;
SELECT VARIABLE_NAME FROM performance_schema.variables_info
  WHERE VARIABLE_SOURCE = 'PERSISTED';

--echo # default values
SELECT @@global.ft_query_expansion_limit;
SELECT @@global.innodb_api_enable_mdl;

--echo # persist few more static variables
SET PERSIST_ONLY ft_query_expansion_limit=80, innodb_api_enable_mdl=1;
SELECT VARIABLE_NAME FROM performance_schema.variables_info
  WHERE VARIABLE_SOURCE = 'PERSISTED';

--echo # Restart server
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # after restart
SELECT @@global.ft_query_expansion_limit;
SELECT @@global.innodb_api_enable_mdl;
SELECT VARIABLE_NAME FROM performance_schema.variables_info
  WHERE VARIABLE_SOURCE = 'PERSISTED';

--echo # modify existing persisted variables
SET PERSIST_ONLY ft_query_expansion_limit=200, innodb_api_enable_mdl=0;

--echo # Restart server
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # after restart
SELECT @@global.ft_query_expansion_limit;
SELECT @@global.innodb_api_enable_mdl;

SELECT @@global.innodb_read_io_threads;
SELECT @@global.log_replica_updates;

--echo # modify existing persisted variables and add new
SET PERSIST_ONLY innodb_read_io_threads= 16;
SET PERSIST_ONLY log_replica_updates= 1;

--echo # Restart server
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # after restart
SELECT @@global.ft_query_expansion_limit;
SELECT @@global.innodb_api_enable_mdl;
SELECT @@global.innodb_read_io_threads;
SELECT @@global.log_replica_updates;

--echo # check contents of persistent config file
--echo
SET @@persist_only.max_connections=99;
SET PERSIST_ONLY table_open_cache_instances= 8;

--echo # try persist_only for dynamic variables
SELECT @@global.max_connections, @@global.session_track_system_variables;
SELECT @@global.transaction_isolation;
SET @@persist_only.max_connections=99;
SET PERSIST_ONLY session_track_system_variables= 'max_connections';
SET @@persist_only.transaction_isolation= 'READ-COMMITTED';
--echo # should not change the values.
SELECT @@global.max_connections, @@global.session_track_system_variables;
SELECT @@global.transaction_isolation;

--echo # Restart server
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # after restart
SELECT @@global.max_connections, @@global.session_track_system_variables;
SELECT @@global.transaction_isolation;

SELECT VARIABLE_NAME FROM performance_schema.variables_info
  WHERE VARIABLE_SOURCE = 'PERSISTED';

--echo # Restart server with persisted_globals_load disabled.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--exec echo "restart:--persisted-globals-load=0" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # should have values different from persistent config file
SELECT @@global.max_connections, @@global.session_track_system_variables;
SELECT @@global.transaction_isolation;

--echo # check when persisted_globals_load is disabled.
--echo # should return 0 rows.
SELECT VARIABLE_NAME FROM performance_schema.variables_info
  WHERE VARIABLE_SOURCE = 'PERSISTED';

SET PERSIST_ONLY replica_type_conversions= ALL_UNSIGNED;
SET @@persist_only.relay_log_space_limit=4096;

--echo # Restart server with persisted_globals_load disabled.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--exec echo "restart:--persisted-globals-load=0" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # should return 0 rows.
SELECT VARIABLE_NAME FROM performance_schema.variables_info
  WHERE VARIABLE_SOURCE = 'PERSISTED';

--echo # Restart server
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

SELECT @@global.relay_log_space_limit, @@global.replica_type_conversions;
SELECT VARIABLE_NAME FROM performance_schema.variables_info
  WHERE VARIABLE_SOURCE = 'PERSISTED';

--echo # check for PERSIST_RO_VARIABLES_ADMIN privilege
CREATE USER wl9787;

--connect (con1,localhost,wl9787,,)

--connection con1
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL sort_buffer_size=256000;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET PERSIST max_heap_table_size=999424;
--error ER_PERSIST_ONLY_ACCESS_DENIED_ERROR
SET PERSIST_ONLY ft_query_expansion_limit=80;

--connection default
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO wl9787;

--connection con1
SET GLOBAL sort_buffer_size=256000;
SET PERSIST max_heap_table_size=999424;
--error ER_PERSIST_ONLY_ACCESS_DENIED_ERROR
SET PERSIST_ONLY ft_query_expansion_limit=80;

--connection default
GRANT PERSIST_RO_VARIABLES_ADMIN ON *.* TO wl9787;

--connection con1
SET PERSIST_ONLY ft_query_expansion_limit=80;

--echo # revoke SYSTEM_VARIABLES_ADMIN
--connection default
REVOKE SYSTEM_VARIABLES_ADMIN ON *.* FROM wl9787;

--connection con1
--echo # persisting static variables needs both SYSTEM_VARIABLES_ADMIN
       # and PERSIST_RO_VARIABLES_ADMIN
--error ER_PERSIST_ONLY_ACCESS_DENIED_ERROR
SET PERSIST_ONLY ft_query_expansion_limit=80;

--connection default
REVOKE PERSIST_RO_VARIABLES_ADMIN  ON *.* FROM wl9787;
GRANT SUPER ON *.* TO wl9787;

--connection con1
--echo # persisting static variables does not need SUPER access
--error ER_PERSIST_ONLY_ACCESS_DENIED_ERROR
SET PERSIST_ONLY ft_query_expansion_limit=80;

--connection default
--echo # reset persisted variables
RESET PERSIST;

--echo # test reset on readonly persisted variables
SET @@persist_only.replica_type_conversions = ALL_UNSIGNED;
SELECT * FROM performance_schema.persisted_variables ORDER BY 1;
SET PERSIST auto_increment_increment=10;
SET PERSIST innodb_checksum_algorithm=strict_crc32;
SET PERSIST_ONLY ft_query_expansion_limit= DEFAULT;
SELECT * FROM performance_schema.persisted_variables ORDER BY 1;
--echo # reset replica_type_conversions
RESET PERSIST replica_type_conversions;
--echo # return 0 rows.
SELECT * FROM performance_schema.persisted_variables
  WHERE VARIABLE_NAME = 'replica_type_conversions';
RESET PERSIST auto_increment_increment;
SELECT * FROM performance_schema.persisted_variables
  WHERE VARIABLE_NAME IN ('auto_increment_increment')
  ORDER BY 1;
RESET PERSIST;
--echo # return 0 rows.
SELECT * FROM performance_schema.persisted_variables ORDER BY 1;

--echo # Restart server
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # test non persistent read only variables
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.basedir= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.character_sets_dir= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.ft_stopword_file= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.lc_messages_dir= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.log_error= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.pid_file= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.plugin_dir= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.secure_file_priv= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.replica_load_tmpdir= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.tmpdir= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.relay_log= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.relay_log_index= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.log_bin_basename= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.log_bin_index= "/";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.bind_address= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.port= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.skip_networking= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.socket= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.core_file= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.innodb_read_only= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.persisted_globals_load= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.datadir= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.innodb_data_file_path= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.innodb_force_load_corrupted= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.innodb_page_size= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.version= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.version_comment= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.version_compile_machine= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.version_compile_os= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.have_compress= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.have_dynamic_loading= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.license= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.protocol_version= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.lower_case_file_system= "";
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@persist_only.innodb_buffer_pool_load_at_startup= "";

# cleanup
RESET PERSIST;
DROP USER wl9787;
--remove_file $MYSQLD_DATADIR/mysqld-auto.cnf

--echo #
--echo # Bug26395134: SET PERSIST_ONLY HAS WRONG EFFECT ON P_S.VARIABLES_INFO
--echo #

SELECT VARIABLE_SOURCE, SET_USER, SET_HOST FROM performance_schema.variables_info
  WHERE VARIABLE_NAME = 'max_connections';
SET PERSIST_ONLY max_connections = 151;
# ensure user/host/timestamp/variable_source are not changed.
SELECT VARIABLE_SOURCE, SET_USER, SET_HOST FROM performance_schema.variables_info
  WHERE VARIABLE_NAME = 'max_connections';

--echo # Restart server
--let $restart_parameters=restart
--source include/restart_mysqld.inc

# ensure variable source is set to PERSISTED
SELECT VARIABLE_SOURCE, SET_USER, SET_HOST FROM performance_schema.variables_info
  WHERE VARIABLE_NAME = 'max_connections';

# cleanup
RESET PERSIST;
--remove_file $MYSQLD_DATADIR/mysqld-auto.cnf

--echo #
--echo # Bug27193853: ASSERT `(0)' AT SYS_VARS.H:2416 FOR SET PERSIST_ONLY
--echo #               GTID_OWNED/EXECUTED
--echo #

--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY gtid_owned='aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET PERSIST_ONLY gtid_executed='aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';

--echo #
--echo # Bug#30094645: SET PERSIST_ONLY TO 0/1 BOOLEAN GLOBAL VARIABLES HAVING SESSION, RESTART FAILS
--echo #

SET PERSIST_ONLY log_replica_updates = 1,super_read_only=1, end_markers_in_json = 1;

--echo # Restart server
--source include/restart_mysqld.inc

select @@global.log_replica_updates, @@global.super_read_only, @@global.end_markers_in_json;

--error ER_WRONG_VALUE_FOR_VAR
SET PERSIST_ONLY super_read_only="invalid value";
--error ER_WRONG_VALUE_FOR_VAR
SET PERSIST_ONLY end_markers_in_json = flase;

# default value is 0 which once persisted should make next server restart to start
SET PERSIST_ONLY super_read_only=default;

--echo # Restart server
--source include/restart_mysqld.inc

#cleanup
RESET PERSIST;
--echo # Restart server with defaults
--source include/restart_mysqld.inc

--echo #
--echo # Bug#30318828: SET PERSIST/PERSIST_ONLY NOT INNODB_BUFFER_POOL_INSTANCES CORRECTLY
--echo #

SET PERSIST innodb_buffer_pool_size = 1073742336;
SET PERSIST_ONLY innodb_buffer_pool_instances = 2;

--echo # Restart server
--source include/restart_mysqld.inc

SELECT FORMAT_BYTES(@@global.innodb_buffer_pool_size) AS Size, @@global.innodb_buffer_pool_instances AS Instances;

#cleanup
RESET PERSIST;
--echo # Restart server
--source include/restart_mysqld.inc
