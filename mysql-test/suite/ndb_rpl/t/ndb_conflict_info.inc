--echo Conflict algorithm counter diff :
--disable_query_log
--disable_result_log
let $resmax=query_get_value(SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_max AS r FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_MAX", r,1);
let $resold=query_get_value(SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_old AS r FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_OLD", r, 1);
let $resmaxdelwin=query_get_value(SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_max_del_win AS r FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_MAX_DEL_WIN", r, 1);
let $resepoch=query_get_value(SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_epoch AS r FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_EPOCH",r,1);
let $resepoch2=query_get_value(SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_epoch2 AS r FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_EPOCH2",r,1);
let $resdeldel=query_get_value(SELECT VARIABLE_VALUE-@init_ndb_epoch_delete_delete AS r FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_EPOCH_DELETE_DELETE_COUNT",r,1);
--enable_result_log
if ($resmax)
{
  --eval select $resmax as ndb_conflict_fn_max
}
if ($resold)
{
  --eval select $resold as ndb_conflict_fn_old
}
if ($resmaxdelwin)
{
  --eval select $resmaxdelwin as ndb_conflict_fn_max_del_win
}
if ($resepoch)
{
  --eval select $resepoch as ndb_conflict_fn_epoch
}
if ($resepoch2)
{
  --eval select $resepoch2 as ndb_conflict_fn_epoch2
}
if ($resdeldel)
{
  --eval select $resdeldel as ndb_epoch_delete_delete
}


#SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_old AS ndb_conflict_fn_old FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_OLD";
#SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_max_del_win AS ndb_conflict_fn_max_del_win FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_MAX_DEL_WIN";
#SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_epoch AS ndb_conflict_fn_epoch FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_EPOCH";
#SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_epoch2 AS ndb_conflict_fn_epoch2 FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_EPOCH2";
#SELECT VARIABLE_VALUE-@init_ndb_epoch_delete_delete AS ndb_epoch_delete_delete FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_EPOCH_DELETE_DELETE_COUNT";
--enable_query_log
--echo Exception table contents
if (!$extended_exceptions_table)
{
--replace_column 3 # 5 # 6 #
--error 0,1146
SELECT server_id, master_server_id, master_epoch, count, a, d FROM `t1$EX` ORDER BY server_id, master_server_id, master_epoch, count;
--replace_column 3 # 4 #
--error 0,1146
SELECT * FROM `t1$EX` ORDER BY a, d;
--replace_column 3 # 5 # 6 #
--error 0,1146
SELECT * FROM `t2$EX` ORDER BY server_id, master_server_id, master_epoch, count;
--replace_column 3 # 4 #
--error 0,1146
SELECT * FROM `t2$EX` ORDER BY a, d;
}
if ($extended_exceptions_table)
{
--replace_column 3 # 5 # 6 # 7 # 8 # 9 # 10 # 11 #
--error 0,1146
SELECT ndb$server_id, ndb$master_server_id, ndb$master_epoch, ndb$count, a, b$old, b$new, d FROM `t1$EX` ORDER BY ndb$server_id, ndb$master_server_id, ndb$master_epoch, ndb$count;
--replace_column 3 # 4 # 8 # 9 # 10 # 11 #
--error 0,1146
SELECT * FROM `t1$EX` ORDER BY ndb$count;
--error 0,1146
SELECT ndb$op_type, ndb$cft_cause FROM `t1$EX` ORDER BY ndb$op_type, ndb$cft_cause;
--replace_column 3 # 5 # 6 # 7 # 8 # 9 # 10 # 11 #
--error 0,1146
SELECT * FROM `t2$EX` ORDER BY ndb$server_id, ndb$master_server_id, ndb$master_epoch, ndb$count;
--replace_column 3 # 4 # 8 # 9 # 10 # 11 #
--error 0,1146
SELECT * FROM `t2$EX` ORDER BY ndb$count;
--error 0,1146
SELECT ndb$op_type, ndb$cft_cause FROM `t2$EX` ORDER BY ndb$op_type, ndb$cft_cause;
}
