#
# Bug #27016247: SET PERSIST_ONLY DOES NOT CONSIDER RUNTIME VALIDATION
#
# Should fail: event_scheduler does not allow setting to DISABLED at runtime
SET PERSIST event_scheduler=DISABLED;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of 'DISABLED'
# Should fail: data type still checked even with persist_only
SET PERSIST_ONLY event_scheduler=123.456;
ERROR 42000: Incorrect argument type to variable 'event_scheduler'
# Should work: check not called for PERSIST_ONLY
SET PERSIST_ONLY event_scheduler=DISABLED;
# cleanup
RESET PERSIST event_scheduler;
#
# Bug #27374791: RESET PERSIST DOES IMPLICIT COMMIT AND DOESN'T NEED
#                RELOAD PRIVILEGES
#
CREATE DATABASE bug27374791;
USE bug27374791;
CREATE TABLE T( i int);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
BEGIN;
INSERT INTO T values (9);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET PERSIST max_connections=31;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
RESET PERSIST;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET GLOBAL max_connections=DEFAULT;
DROP DATABASE bug27374791;
#
# Bug #27322254: WITH PERSIST_ONLY, VARIABLE VALUE EQUALS 18446744073709547520
#                IS STORED AS -4096
#
SELECT @@max_binlog_cache_size;
@@max_binlog_cache_size
18446744073709547520
SET PERSIST max_binlog_cache_size= @@global.max_binlog_cache_size;
SELECT * FROM performance_schema.persisted_variables WHERE
VARIABLE_NAME= 'max_binlog_cache_size';
VARIABLE_NAME	VARIABLE_VALUE
max_binlog_cache_size	18446744073709547520
SET PERSIST_ONLY max_binlog_cache_size= @@global.max_binlog_cache_size;
SELECT * FROM performance_schema.persisted_variables WHERE
VARIABLE_NAME= 'max_binlog_cache_size';
VARIABLE_NAME	VARIABLE_VALUE
max_binlog_cache_size	18446744073709547520
SET @a=cast(@@max_binlog_cache_size as char);
SELECT @a;
@a
18446744073709547520
RESET PERSIST;
SET GLOBAL max_binlog_cache_size= DEFAULT;
#
# Bug #27536421: SET PERSIST CHANGES THE VALUE OF OPTIMIZER_TRACE_OFFSET
#                AFTER RESTART
#
SELECT @@global.optimizer_trace_offset, @@global.activate_all_roles_on_login,
@@global.auto_increment_increment, @@global.auto_increment_offset,
@@global.binlog_error_action, @@global.binlog_format,
@@global.cte_max_recursion_depth, @@global.eq_range_index_dive_limit,
@@global.innodb_monitor_disable, @@global.histogram_generation_max_mem_size,
@@global.innodb_max_dirty_pages_pct, @@global.init_connect,
@@global.max_join_size;
@@global.optimizer_trace_offset	@@global.activate_all_roles_on_login	@@global.auto_increment_increment	@@global.auto_increment_offset	@@global.binlog_error_action	@@global.binlog_format	@@global.cte_max_recursion_depth	@@global.eq_range_index_dive_limit	@@global.innodb_monitor_disable	@@global.histogram_generation_max_mem_size	@@global.innodb_max_dirty_pages_pct	@@global.init_connect	@@global.max_join_size
-1	0	1	1	ABORT_SERVER	###	1000	200	NULL	20000000	90.000000		18446744073709551615
SET PERSIST optimizer_trace_offset = default;
SET PERSIST activate_all_roles_on_login= ON;
SET PERSIST auto_increment_increment= 4, auto_increment_offset= 2;
SET PERSIST binlog_error_action= IGNORE_ERROR, binlog_format= ROW;
SET PERSIST cte_max_recursion_depth= 4294967295, eq_range_index_dive_limit= 4294967295;
SET PERSIST innodb_monitor_disable='latch';
SET PERSIST innodb_max_dirty_pages_pct= 97.3;
SET PERSIST init_connect='SET autocommit=0';
SET PERSIST max_join_size= 18446744073709551615;
SELECT @@global.optimizer_trace_offset, @@global.activate_all_roles_on_login,
@@global.auto_increment_increment, @@global.auto_increment_offset,
@@global.binlog_error_action, @@global.binlog_format,
@@global.cte_max_recursion_depth, @@global.eq_range_index_dive_limit,
@@global.innodb_monitor_disable,
@@global.innodb_max_dirty_pages_pct, @@global.init_connect,
@@global.max_join_size;
@@global.optimizer_trace_offset	@@global.activate_all_roles_on_login	@@global.auto_increment_increment	@@global.auto_increment_offset	@@global.binlog_error_action	@@global.binlog_format	@@global.cte_max_recursion_depth	@@global.eq_range_index_dive_limit	@@global.innodb_monitor_disable	@@global.innodb_max_dirty_pages_pct	@@global.init_connect	@@global.max_join_size
-1	1	4	2	IGNORE_ERROR	###	4294967295	4294967295	latch	97.300000	SET autocommit=0	18446744073709551615
SELECT * FROM performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
activate_all_roles_on_login	ON
auto_increment_increment	4
auto_increment_offset	2
binlog_error_action	IGNORE_ERROR
binlog_format	ROW
cte_max_recursion_depth	4294967295
eq_range_index_dive_limit	4294967295
init_connect	SET autocommit=0
innodb_max_dirty_pages_pct	97.300000
innodb_monitor_disable	latch
max_join_size	18446744073709551615
optimizer_trace_offset	-1
# Restart server
# restart
SELECT @@global.optimizer_trace_offset, @@global.activate_all_roles_on_login,
@@global.auto_increment_increment, @@global.auto_increment_offset,
@@global.binlog_error_action, @@global.binlog_format,
@@global.cte_max_recursion_depth, @@global.eq_range_index_dive_limit,
@@global.innodb_monitor_disable,
@@global.innodb_max_dirty_pages_pct, @@global.init_connect,
@@global.max_join_size;
@@global.optimizer_trace_offset	@@global.activate_all_roles_on_login	@@global.auto_increment_increment	@@global.auto_increment_offset	@@global.binlog_error_action	@@global.binlog_format	@@global.cte_max_recursion_depth	@@global.eq_range_index_dive_limit	@@global.innodb_monitor_disable	@@global.innodb_max_dirty_pages_pct	@@global.init_connect	@@global.max_join_size
-1	1	4	2	IGNORE_ERROR	###	4294967295	4294967295	latch	97.300000	SET autocommit=0	18446744073709551615
SELECT * FROM performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
activate_all_roles_on_login	ON
auto_increment_increment	4
auto_increment_offset	2
binlog_error_action	IGNORE_ERROR
binlog_format	ROW
cte_max_recursion_depth	4294967295
eq_range_index_dive_limit	4294967295
init_connect	SET autocommit=0
innodb_max_dirty_pages_pct	97.300000
innodb_monitor_disable	latch
max_join_size	18446744073709551615
optimizer_trace_offset	-1
# Cleanup
RESET PERSIST;
SET GLOBAL optimizer_trace_offset = default, activate_all_roles_on_login = default,
auto_increment_increment = default, auto_increment_offset = default,
binlog_error_action = default, binlog_format = default,
cte_max_recursion_depth = default, eq_range_index_dive_limit = default,
innodb_monitor_disable = default,
innodb_max_dirty_pages_pct = default, init_connect = default,
max_join_size = default;
Warnings:
Warning	1230	Default value is not defined for this set option. Please specify correct counter or module name.
#
# Bug #27629719: SET PERSIST STORES TRUNCATED TIMESTAMPS
#
# Set one variable in the mysqld-auto.cnf
SET PERSIST max_join_size= 10000000;
# Set another variable in the mysqld-auto.cnf
SET PERSIST init_connect='';
# Restart the server so it sets the variable time from mysqld-auto.cnf
# restart
# Check the microseconds of set_time: must be 2 (i.e. different for the two vars)
SELECT COUNT(DISTINCT MICROSECOND(set_time)) FROM performance_schema.variables_info
WHERE variable_name IN ('max_join_size', 'init_connect');
COUNT(DISTINCT MICROSECOND(set_time))
2
# Cleanup
SET GLOBAL max_join_size=DEFAULT, init_connect=DEFAULT;
RESET PERSIST;
#
# Bug #27489026: PERSIST_ONLY DOESN'T RESPECT DEFAULT INSTEAD COPIES GLOBAL VALUE
#
SELECT @@global.binlog_cache_size;
@@global.binlog_cache_size
32768
SELECT @@global.collation_database;
@@global.collation_database
utf8mb4_0900_ai_ci
SELECT @@global.optimizer_trace_offset;
@@global.optimizer_trace_offset
-1
SELECT @@global.optimizer_switch;
@@global.optimizer_switch
index_merge=on,index_merge_union=on,index_merge_sort_union=on,index_merge_intersection=on,engine_condition_pushdown=on,index_condition_pushdown=on,mrr=on,mrr_cost_based=on,block_nested_loop=on,batched_key_access=off,materialization=on,semijoin=on,loosescan=on,firstmatch=on,duplicateweedout=on,subquery_materialization_cost_based=on,use_index_extensions=on,condition_fanout_filter=on,derived_merge=on,use_invisible_indexes=off,skip_scan=on,hash_join=on,subquery_to_derived=off,prefer_ordering_index=on,hypergraph_optimizer=off,derived_condition_pushdown=on
SELECT @@global.enforce_gtid_consistency;
@@global.enforce_gtid_consistency
OFF
SELECT @@global.sql_mode;
@@global.sql_mode
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
SET @@global.binlog_cache_size= 4096;
SET @@persist_only.binlog_cache_size= default,
@@persist_only.collation_database= default,
@@persist_only.optimizer_trace_offset= default,
@@persist_only.optimizer_switch= default,
@@persist_only.enforce_gtid_consistency= default,
@@persist_only.sql_mode= default;
SELECT * FROM performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
binlog_cache_size	32768
collation_database	utf8mb4_0900_ai_ci
enforce_gtid_consistency	0
optimizer_switch	index_merge=on,index_merge_union=on,index_merge_sort_union=on,index_merge_intersection=on,engine_condition_pushdown=on,index_condition_pushdown=on,mrr=on,mrr_cost_based=on,block_nested_loop=on,batched_key_access=off,materialization=on,semijoin=on,loosescan=on,firstmatch=on,duplicateweedout=on,subquery_materialization_cost_based=on,use_index_extensions=on,condition_fanout_filter=on,derived_merge=on,use_invisible_indexes=off,skip_scan=on,hash_join=on,subquery_to_derived=off,prefer_ordering_index=on,hypergraph_optimizer=off,derived_condition_pushdown=on
optimizer_trace_offset	-1
sql_mode	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
# Restart server
# restart
SELECT @@global.binlog_cache_size;
@@global.binlog_cache_size
32768
SELECT @@global.collation_database;
@@global.collation_database
utf8mb4_0900_ai_ci
SELECT @@global.optimizer_trace_offset;
@@global.optimizer_trace_offset
-1
SELECT @@global.optimizer_switch;
@@global.optimizer_switch
index_merge=on,index_merge_union=on,index_merge_sort_union=on,index_merge_intersection=on,engine_condition_pushdown=on,index_condition_pushdown=on,mrr=on,mrr_cost_based=on,block_nested_loop=on,batched_key_access=off,materialization=on,semijoin=on,loosescan=on,firstmatch=on,duplicateweedout=on,subquery_materialization_cost_based=on,use_index_extensions=on,condition_fanout_filter=on,derived_merge=on,use_invisible_indexes=off,skip_scan=on,hash_join=on,subquery_to_derived=off,prefer_ordering_index=on,hypergraph_optimizer=off,derived_condition_pushdown=on
SELECT @@global.enforce_gtid_consistency;
@@global.enforce_gtid_consistency
OFF
SELECT @@global.sql_mode;
@@global.sql_mode
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
SELECT * FROM performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
binlog_cache_size	32768
collation_database	utf8mb4_0900_ai_ci
enforce_gtid_consistency	0
optimizer_switch	index_merge=on,index_merge_union=on,index_merge_sort_union=on,index_merge_intersection=on,engine_condition_pushdown=on,index_condition_pushdown=on,mrr=on,mrr_cost_based=on,block_nested_loop=on,batched_key_access=off,materialization=on,semijoin=on,loosescan=on,firstmatch=on,duplicateweedout=on,subquery_materialization_cost_based=on,use_index_extensions=on,condition_fanout_filter=on,derived_merge=on,use_invisible_indexes=off,skip_scan=on,hash_join=on,subquery_to_derived=off,prefer_ordering_index=on,hypergraph_optimizer=off,derived_condition_pushdown=on
optimizer_trace_offset	-1
sql_mode	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
# Cleanup
RESET PERSIST;
#
# Bug #27523095: SET PERSIST MANDATORY_ROLES FAILS TO START THE SERVER
#
SET PERSIST mandatory_roles= default;
# Restart server and check if server starts
# restart
SELECT * FROM performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
mandatory_roles	
RESET PERSIST;
#
# Bug #27903874: [MYSQL 8.0 GA DEBUG BUILD] ASSERTION `!THD ||
#                THD->LOCKED_TABLES_MODE || !THD->M
#
CREATE DATABASE bug27903874;
USE bug27903874;
SET @@autocommit=FALSE;
CREATE TABLE t(a CHAR (1))ENGINE=InnoDB;
RESET PERSIST size;
ERROR HY000: Variable size does not exist in persisted config file
SELECT JSON_OBJECTAGG(id,x) FROM t;
ERROR 42S22: Unknown column 'id' in 'field list'
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
DROP DATABASE bug27903874;
#
# Bug #27512616: SET PERSIST COMMAND FAILS TO MAP DEFAULT(NULL) VALUES IN
#                MYSQLD-AUTO.CNF
#
SELECT @@global.innodb_tmpdir;
@@global.innodb_tmpdir
NULL
SET PERSIST innodb_tmpdir = default;
SELECT @@global.innodb_ft_user_stopword_table;
@@global.innodb_ft_user_stopword_table
NULL
SET PERSIST innodb_ft_user_stopword_table = NULL;
# Restart server and check if server starts
# restart
SELECT variable_name FROM performance_schema.variables_info WHERE variable_source='PERSISTED';
variable_name
innodb_ft_user_stopword_table
innodb_tmpdir
SELECT @@global.innodb_tmpdir, @@global.innodb_ft_user_stopword_table;
@@global.innodb_tmpdir	@@global.innodb_ft_user_stopword_table
NULL	NULL
RESET PERSIST;
#
# Bug #28564239: RESET PERSIST REQUIRES NO PRIVILEGES
#
CREATE USER u1;
SET PERSIST_ONLY ft_query_expansion_limit=80, innodb_api_enable_mdl=1;
SET PERSIST sort_buffer_size=156000,max_connections= 52;
SET PERSIST max_heap_table_size=887808, replica_net_timeout=160;
SHOW GRANTS;
Grants for u1@%
GRANT USAGE ON *.* TO `u1`@`%`
RESET PERSIST max_heap_table_size;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
RESET PERSIST ft_query_expansion_limit;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
RESET PERSIST;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
GRANT SUPER ON *.* TO u1;
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
SHOW GRANTS;
Grants for u1@%
GRANT SUPER ON *.* TO `u1`@`%`
RESET PERSIST max_heap_table_size;
RESET PERSIST ft_query_expansion_limit;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
RESET PERSIST;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
REVOKE SUPER ON *.* FROM u1;
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO u1;
SHOW GRANTS;
Grants for u1@%
GRANT USAGE ON *.* TO `u1`@`%`
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO `u1`@`%`
RESET PERSIST max_connections;
RESET PERSIST ft_query_expansion_limit;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
RESET PERSIST;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
REVOKE SYSTEM_VARIABLES_ADMIN ON *.* FROM u1;
GRANT PERSIST_RO_VARIABLES_ADMIN ON *.* TO u1;
SHOW GRANTS;
Grants for u1@%
GRANT USAGE ON *.* TO `u1`@`%`
GRANT PERSIST_RO_VARIABLES_ADMIN ON *.* TO `u1`@`%`
RESET PERSIST replica_net_timeout;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
RESET PERSIST ft_query_expansion_limit;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
RESET PERSIST;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO u1;
SHOW GRANTS;
Grants for u1@%
GRANT USAGE ON *.* TO `u1`@`%`
GRANT PERSIST_RO_VARIABLES_ADMIN,SYSTEM_VARIABLES_ADMIN ON *.* TO `u1`@`%`
RESET PERSIST replica_net_timeout;
RESET PERSIST ft_query_expansion_limit;
RESET PERSIST;
SELECT * FROM performance_schema.persisted_variables;
VARIABLE_NAME	VARIABLE_VALUE
SET GLOBAL sort_buffer_size = default, max_connections = default,
replica_net_timeout = default, max_heap_table_size = default;
DROP USER u1;
#
# Bug #28466045: SET PERSIST NOT SETTING CORRECT VALUES FOR VARIABLES
#                LIKE INNODB_STRICT_MODE
#
SELECT @@global.innodb_strict_mode, @@global.innodb_lock_wait_timeout;
@@global.innodb_strict_mode	@@global.innodb_lock_wait_timeout
1	50
SELECT @@global.myisam_sort_buffer_size;
@@global.myisam_sort_buffer_size
8388608
SELECT @@global.myisam_stats_method;
@@global.myisam_stats_method
nulls_unequal
SET PERSIST innodb_strict_mode=0;
SET GLOBAL innodb_lock_wait_timeout = 150;
SELECT @@global.innodb_lock_wait_timeout;
@@global.innodb_lock_wait_timeout
150
SET PERSIST innodb_lock_wait_timeout = 110;
SELECT @@global.innodb_lock_wait_timeout;
@@global.innodb_lock_wait_timeout
110
SET GLOBAL myisam_sort_buffer_size=16777216;
SET PERSIST myisam_sort_buffer_size=default;
SELECT @@global.innodb_lock_wait_timeout;
@@global.innodb_lock_wait_timeout
110
SET PERSIST myisam_stats_method=nulls_equal;
# Restart server
# restart
SELECT @@global.innodb_strict_mode;
@@global.innodb_strict_mode
0
SELECT @@global.innodb_lock_wait_timeout;
@@global.innodb_lock_wait_timeout
110
SELECT @@global.myisam_sort_buffer_size;
@@global.myisam_sort_buffer_size
8388608
SELECT @@global.myisam_stats_method;
@@global.myisam_stats_method
nulls_equal
SELECT variable_name FROM performance_schema.variables_info WHERE variable_source='PERSISTED';
variable_name
innodb_lock_wait_timeout
innodb_strict_mode
myisam_sort_buffer_size
myisam_stats_method
SET GLOBAL innodb_strict_mode=default, innodb_lock_wait_timeout=default,
myisam_stats_method=default;
RESET PERSIST;
#
# Bug #30030648: NO DEPRECATION WARNING ON SERVER RESTART FOR SET PERSISTED OPTIONS
#
# should report warning
SET GLOBAL avoid_temporal_upgrade=TRUE;
Warnings:
Warning	1287	'@@avoid_temporal_upgrade' is deprecated and will be removed in a future release.
# should report warning
SET PERSIST_ONLY avoid_temporal_upgrade=TRUE;
Warnings:
Warning	1287	'@@avoid_temporal_upgrade' is deprecated and will be removed in a future release.
# should report warning
SET PERSIST avoid_temporal_upgrade=TRUE;
Warnings:
Warning	1287	'@@avoid_temporal_upgrade' is deprecated and will be removed in a future release.
# Restart server
include/assert_grep.inc [Assert that there is a warning at bootstrap when setting --avoid_temporal_upgrade]
RESET PERSIST;
#
# Bug #28749668: SKIP_NAME_RESOLVE CAN BE PERSIT_ONLY-ED TO ON, BUT NOT OFF
#
SELECT @@global.skip_name_resolve;
@@global.skip_name_resolve
0
CREATE USER 'bug28749668'@'%';
GRANT ALL ON *.* TO 'bug28749668'@'%';
SET PERSIST_ONLY skip_name_resolve=0;
# Restart server and check if server starts
# restart:
SELECT @@global.skip_name_resolve;
@@global.skip_name_resolve
0
SELECT * FROM performance_schema.persisted_variables;
VARIABLE_NAME	VARIABLE_VALUE
skip_name_resolve	OFF
RESET PERSIST;
DROP USER 'bug28749668'@'%';
#
# Bug #30169731: MYSQL SERVER DOESN'T START AND NO ERROR IN THE LOG
#
# config file is in wrong json format
# server should fail to start
include/assert_grep.inc [Server should fail to start as mysqld-auto.cnf file is with wrong JSON format.]
# start server with all defaults
# restart:
#
# Bug #32761053: SET PERSIST_ONLY ... = DEFAULT IS BROKEN FOR ENUM VARIABLES
#
SET PERSIST_ONLY replica_exec_mode = DEFAULT;
SELECT * FROM performance_schema.persisted_variables
WHERE variable_name='replica_exec_mode';
VARIABLE_NAME	VARIABLE_VALUE
replica_exec_mode	STRICT
RESET PERSIST;
#
# Bug #32640588: SOURCE FOR SKIP_SLAVE_START DISPLAYED AS COMPILED
#   WHEN IT IS PERSISTED
#
# Should both be "COMPILED"
SELECT VARIABLE_NAME, VARIABLE_SOURCE FROM performance_schema.variables_info
WHERE variable_name LIKE 'skip_%_start' ORDER BY 1;
VARIABLE_NAME	VARIABLE_SOURCE
skip_replica_start	COMPILED
skip_slave_start	COMPILED
SET PERSIST_ONLY skip_replica_start = ON;
# Should still be "COMPILED" pre-restart
SELECT VARIABLE_NAME, VARIABLE_SOURCE FROM performance_schema.variables_info
WHERE variable_name LIKE 'skip_%_start' ORDER BY 1;
VARIABLE_NAME	VARIABLE_SOURCE
skip_replica_start	COMPILED
skip_slave_start	COMPILED
# restart:
# Success criteria: Should be "PERSISTED" post-restart
SELECT VARIABLE_NAME, VARIABLE_SOURCE FROM performance_schema.variables_info
WHERE variable_name LIKE 'skip_%_start' ORDER BY 1;
VARIABLE_NAME	VARIABLE_SOURCE
skip_replica_start	PERSISTED
skip_slave_start	PERSISTED
RESET PERSIST;
# restart:
#
# Bug #33388488: NO DEPRECATION WARNING ON SERVER RESTART IF TWO OR MORE
# PERSISTED OPTIONS
#
SET PERSIST transaction_write_set_extraction='XXHASH64';
Warnings:
Warning	1287	'@@transaction_write_set_extraction' is deprecated and will be removed in a future release.
SET PERSIST avoid_temporal_upgrade=TRUE;
Warnings:
Warning	1287	'@@avoid_temporal_upgrade' is deprecated and will be removed in a future release.
# restart:--log_error=LOG_ERR --no-console
include/assert_grep.inc [Assert that there is a warning at bootstrap when setting transaction_write_set_extraction]
include/assert_grep.inc [Assert that there is a warning at bootstrap when setting avoid_temporal_upgrade]
RESET PERSIST;
SET PERSIST_ONLY transaction_write_set_extraction='XXHASH64';
Warnings:
Warning	1287	'@@transaction_write_set_extraction' is deprecated and will be removed in a future release.
SET PERSIST_ONLY avoid_temporal_upgrade=TRUE;
Warnings:
Warning	1287	'@@avoid_temporal_upgrade' is deprecated and will be removed in a future release.
# restart:--log_error=LOG_ERR --no-console
include/assert_grep.inc [Assert that there is a warning at bootstrap when setting transaction_write_set_extraction]
include/assert_grep.inc [Assert that there is a warning at bootstrap when setting avoid_temporal_upgrade]
RESET PERSIST;
# restart:
# End of the 8.0 tests
