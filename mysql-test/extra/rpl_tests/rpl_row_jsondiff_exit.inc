
--echo ######## CLEAN UP ########

--delimiter |
let $rpl_sql=
  SET @@SESSION.BINLOG_ROW_IMAGE = FULL;
  SET @@SESSION.BINLOG_ROW_VALUE_OPTIONS = '';
  SET @@GLOBAL.BINLOG_ROW_IMAGE = FULL;
  SET @@GLOBAL.BINLOG_ROW_VALUE_OPTIONS = '';
|
--delimiter ;
--source include/rpl_for_each_server_stmt.inc

if ($replace_combination_from)
{
  --source include/end_replace_combination.inc
}

--source include/rpl_end.inc
