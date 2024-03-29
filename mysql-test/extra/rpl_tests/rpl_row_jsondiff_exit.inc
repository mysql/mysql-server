
--echo ######## CLEAN UP ########

--delimiter |
let $rpl_sql=
  SET @@SESSION.BINLOG_ROW_IMAGE = FULL;
  SET @@SESSION.BINLOG_ROW_VALUE_OPTIONS = '';
  SET @@GLOBAL.BINLOG_ROW_IMAGE = FULL;
  SET @@GLOBAL.BINLOG_ROW_VALUE_OPTIONS = '';
  SET SESSION SQL_LOG_BIN = 0; DROP TABLE $table; SET SESSION SQL_LOG_BIN = 1;
|
--delimiter ;
--source include/rpl_for_each_server_stmt.inc

if ($replace_combination_from)
{
  # To avoid an error 'ER_RUNNING_APPLIER_PREVENTS_SWITCH_GLOBAL_BINLOG_FORMAT',
  # which will be caused by the following include/end_replace_combination.inc.
  --source include/rpl_stop_slaves.inc
  --source include/end_replace_combination.inc
  --source include/rpl_start_slaves.inc
}

--source include/rpl_end.inc
