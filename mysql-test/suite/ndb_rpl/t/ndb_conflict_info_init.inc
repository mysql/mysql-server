--disable_query_log
--disable_result_log
SELECT @init_ndb_conflict_fn_max:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_MAX";
SELECT @init_ndb_conflict_fn_old:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_OLD";
SELECT @init_ndb_conflict_fn_max_del_win:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_MAX_DEL_WIN";
SELECT @init_ndb_conflict_fn_max_ins:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_MAX_INS";
SELECT @init_ndb_conflict_fn_max_del_win_ins:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_MAX_DEL_WIN_INS";
SELECT @init_ndb_conflict_fn_epoch:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_EPOCH";
SELECT @init_ndb_epoch_delete_delete:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_EPOCH_DELETE_DELETE_COUNT";
SELECT @init_ndb_epoch_delete_delete:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_EPOCH_DELETE_DELETE_COUNT";
SELECT @init_ndb_conflict_fn_epoch2:=(VARIABLE_VALUE+0) FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_EPOCH2";
--error 0,1146
DELETE FROM `t1$EX`;
--enable_query_log
--enable_result_log
