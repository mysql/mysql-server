--echo Conflict algorithm counter diff :
--disable_query_log
let $resmax=
  `SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_max
     FROM performance_schema.global_status
       WHERE VARIABLE_NAME = "NDB_CONFLICT_FN_MAX"`;
let $resold=
  `SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_old
     FROM performance_schema.global_status
       WHERE VARIABLE_NAME = "NDB_CONFLICT_FN_OLD"`;
let $resmaxdelwin=
  `SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_max_del_win
     FROM performance_schema.global_status
       WHERE VARIABLE_NAME = "NDB_CONFLICT_FN_MAX_DEL_WIN"`;
let $resmaxins=
  `SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_max_ins
     FROM performance_schema.global_status
       WHERE VARIABLE_NAME = "NDB_CONFLICT_FN_MAX_INS"`;
let $resmaxdelwinins=
  `SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_max_del_win_ins
     FROM performance_schema.global_status
       WHERE VARIABLE_NAME = "NDB_CONFLICT_FN_MAX_DEL_WIN_INS"`;
let $resepoch=
  `SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_epoch
     FROM performance_schema.global_status
       WHERE VARIABLE_NAME = "NDB_CONFLICT_FN_EPOCH"`;
let $resepoch2=
  `SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_epoch2
     FROM performance_schema.global_status
       WHERE VARIABLE_NAME = "NDB_CONFLICT_FN_EPOCH2"`;
let $resdeldel=
  `SELECT VARIABLE_VALUE-@init_ndb_epoch_delete_delete
     FROM performance_schema.global_status
       WHERE VARIABLE_NAME = "NDB_CONFLICT_EPOCH_DELETE_DELETE_COUNT"`;
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
if ($resmaxins)
{
  --eval select $resmaxins as ndb_conflict_fn_max_ins
}
if ($resmaxdelwinins)
{
  --eval select $resmaxdelwinins as ndb_conflict_fn_max_del_win_ins
}


#SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_old AS ndb_conflict_fn_old FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_OLD";
#SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_max_del_win AS ndb_conflict_fn_max_del_win FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_MAX_DEL_WIN";
#SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_epoch AS ndb_conflict_fn_epoch FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_EPOCH";
#SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_epoch2 AS ndb_conflict_fn_epoch2 FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_EPOCH2";
#SELECT VARIABLE_VALUE-@init_ndb_epoch_delete_delete AS ndb_epoch_delete_delete FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_EPOCH_DELETE_DELETE_COUNT";
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
}
