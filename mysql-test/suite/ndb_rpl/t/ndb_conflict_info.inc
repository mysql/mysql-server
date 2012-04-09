SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_max FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_MAX";
SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_old FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_OLD";
SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_max_del_win FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_MAX_DEL_WIN";
SELECT VARIABLE_VALUE-@init_ndb_conflict_fn_epoch FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME LIKE "NDB_CONFLICT_FN_EPOCH";
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
