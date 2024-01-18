# ==== Purpose ====
#
# Temporarily disable the binary log by setting @@session.sql_log_bin=0.
#
# The old value is stored in a session variable, so that it can be restored
# using restore_binlog.inc
#
# ==== Usage ====
#
# --source include/rpl/disable_binlog.inc
# ... do things with binlog disabled ...
# --source include/rpl/restore_binlog.inc

--let $include_filename = rpl/disable_binlog.inc
--source include/begin_include_file.inc

--disable_query_log
SET @session.disable_binlog_save = @@session.sql_log_bin,
    @@session.sql_log_bin = 0;

--let $include_filename = rpl/disable_binlog.inc
--source include/end_include_file.inc
