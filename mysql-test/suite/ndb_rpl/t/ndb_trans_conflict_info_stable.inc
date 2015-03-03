# Ignore information_schema.global_status deprecation warning
--disable_warnings

--disable_query_log
# Where multiple user transactions from the Secondary site are
# involved, the actual number of rows in-conflict, the number
# of epoch transactions with conflicts, and the number of
# iterations, vary depending on the distribution of the user
# transactions across epochs.  e.g. if 3 transactions are in
# one epoch (and executed in one batch), then only actual
# conflicts will be recorded, but if they are separated, then
# implied rows may conflict as well.  This can make these
# counter values non-deterministic, so this .inc file is used
# to check that the stable counters are correct.  The stable
# counters are the number of rejected rows, and the number
# of rejected transactions, which must be the same, regardless
# of how the epoch boundaries lie.
#
#SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_epoch_trans AS ndb_conflict_fn_epoch_trans FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_EPOCH_TRANS";
#SELECT VARIABLE_VALUE-@init_ndb_conflict_trans_row_conflict_count AS ndb_conflict_trans_row_conflict_count FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_TRANS_ROW_CONFLICT_COUNT";
SELECT VARIABLE_VALUE-@init_ndb_conflict_trans_row_reject_count AS ndb_conflict_trans_row_reject_count FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_TRANS_ROW_REJECT_COUNT";
SELECT VARIABLE_VALUE-@init_ndb_conflict_trans_reject_count AS ndb_conflict_trans_reject_count FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_TRANS_REJECT_COUNT";
#SELECT VARIABLE_VALUE-@init_ndb_conflict_trans_detect_iter_count AS ndb_conflict_trans_detect_iter_count FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_TRANS_DETECT_ITER_COUNT";
#SELECT VARIABLE_VALUE-@init_ndb_conflict_trans_conflict_commit_count AS ndb_conflict_trans_conflict_commit_count FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_TRANS_CONFLICT_COMMIT_COUNT";
SELECT VARIABLE_VALUE-@init_ndb_conflict_epoch_delete_delete_count AS ndb_epoch_delete_delete_count FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_EPOCH_DELETE_DELETE_COUNT";
--enable_query_log

--enable_warnings
