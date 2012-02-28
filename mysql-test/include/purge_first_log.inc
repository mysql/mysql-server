# ==== Purpose ====
#
# Purge the first (oldest) binary log.
#
# ==== Usage ====
#
# --source include/purge_first_log.inc

--let $include_filename= purge_first_log.inc
--source include/begin_include_file.inc

--let $_purge_first_log= query_get_value(SHOW BINARY LOGS, Log_name, 2)
--disable_query_log
eval PURGE BINARY LOGS TO '$_purge_first_log';

--let $include_filename= purge_first_log.inc
--source include/end_include_file.inc
