# Ignore information_schema.global_status deprecation warning
--disable_warnings

--echo Conflict algorithm counter diff :
--disable_query_log
SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_epoch2 AS ndb_conflict_fn_epoch2 FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_EPOCH2";
SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_epoch2_trans AS ndb_conflict_fn_epoch2_trans FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_EPOCH2_TRANS";
SELECT VARIABLE_VALUE-@init_ndb_conflict_trans_Count AS ndb_conflict_trans_reject_count FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_TRANS_REJECT_COUNT";
SELECT VARIABLE_VALUE-@init_ndb_epoch_delete_delete AS ndb_conflict_epoch_delete_delete_count FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_EPOCH_DELETE_DELETE_COUNT";
SELECT VARIABLE_VALUE-@init_ndb_epoch_refresh_op_count AS ndb_conflict_refresh_op_count FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_REFRESH_OP_COUNT";
--disable_result_log
SELECT @reflect_prep_count:=(VARIABLE_VALUE+0)-(@init_ndb_epoch_reflect_op_prep_count+0) FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_REFLECTED_OP_PREPARE_COUNT";
SELECT @reflect_rej_count:=(VARIABLE_VALUE+0)-(@init_ndb_epoch_refresh_op_discard_count+0) FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_REFLECTED_OP_DISCARD_COUNT";
--enable_result_log
SELECT (@reflect_prep_count+0) > 0 AS reflected_op_prepare_count_greater_than_zero;
SELECT ((@reflect_rej_count+0) < (@reflect_prep_count+0)) AS any_reflected_ops_applied;
SELECT (@init_ndb_last_conflict_epoch+0) < (VARIABLE_VALUE+0) AS last_conflict_epoch_increased FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_LAST_CONFLICT_EPOCH";
--disable_result_log
SELECT @ndb_last_conflict_epoch:= VARIABLE_VALUE FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_LAST_CONFLICT_EPOCH";
SELECT @ndb_last_stable_epoch:=VARIABLE_VALUE FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_LAST_STABLE_EPOCH";
SELECT @ndb_max_replicated_epoch:=VARIABLE_VALUE FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_SLAVE_MAX_REPLICATED_EPOCH";

--enable_result_log

# Handy for getting a log of the actual numbers for debugging without
# affecting the output...
#let $info = `SELECT CONCAT_WS(' ',
#                                  "max_replicated_epoch:", @ndb_max_replicated_epoch, 
#                                  "last_conflict_epoch:", @ndb_last_conflict_epoch, 
#                              "init_last_conflict_epoch:", @init_ndb_last_conflict_epoch,
#                              "max_rep_epoch lt last_conflict_epoch:", (@ndb_max_replicated_epoch < @ndb_last_conflict_epoch),
#                              "Again with numbers", ((@ndb_max_replicated_epoch + 0) < (@ndb_last_conflict_epoch + 0)))`;
#
#--exec echo $info >> /tmp/data


SELECT (@ndb_max_replicated_epoch+0) < (@ndb_last_conflict_epoch+0) AS conflict_handling_in_progess;

SELECT (@ndb_last_stable_epoch+0) < (@ndb_max_replicated_epoch+0) AS stable_epoch_is_older_than_max_replicated;

--enable_query_log

--enable_warnings
