# ==== Purpose ====
#
# Restore binlog mode to what it was before the previous call to
# disable_binlog.inc.
#
# ==== Usage ====
#
# --source include/disable_binlog.inc
# ... do things with binlog disabled ...
# --source include/restore_binlog.inc

--let $include_filename = restore_binlog.inc
--source include/begin_include_file.inc

--disable_query_log
SET @@session.sql_log_bin = @session.disable_binlog_save;

--source include/end_include_file.inc
