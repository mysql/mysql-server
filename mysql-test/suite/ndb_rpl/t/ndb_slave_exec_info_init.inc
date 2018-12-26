--disable_query_log
--disable_result_log
SELECT @init_wait_exec_complete_count_slave:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_API_WAIT_EXEC_COMPLETE_COUNT_SLAVE";
SELECT @init_trans_start_count_slave:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_API_TRANS_START_COUNT_SLAVE";
SELECT @init_pk_op_count_slave:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_API_PK_OP_COUNT_SLAVE";
SELECT @init_trans_commit_count_slave:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_API_TRANS_COMMIT_COUNT_SLAVE";
--enable_query_log
--enable_result_log
