#
# Capture some api counter values
#
# inputs 
#   $_count_suffix
--disable_query_log
--disable_result_log
eval select variable_value from performance_schema.global_status 
 where variable_name like 'ndb_api_wait_exec_complete_count$_count_suffix' into @exec_$_count_suffix;
eval select variable_value from performance_schema.global_status 
 where variable_name like 'ndb_api_pk_op_count$_count_suffix' into @op_$_count_suffix;
--enable_result_log
--enable_query_log
