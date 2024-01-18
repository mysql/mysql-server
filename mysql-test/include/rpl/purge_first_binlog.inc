# ==== Purpose ====
#
# Purge the first (oldest) binary log.
#
# ==== Usage ====
#
# --source include/rpl/purge_first_binlog.inc

--let $include_filename= rpl/purge_first_binlog.inc
--source include/begin_include_file.inc

--let $_purge_first_log= query_get_value(SHOW BINARY LOGS, Log_name, 2)
--disable_query_log
eval PURGE BINARY LOGS TO '$_purge_first_log';

--let $include_filename= rpl/purge_first_binlog.inc
--source include/end_include_file.inc
