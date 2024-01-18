
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
--source include/rpl/for_each_server_stmt.inc

--source include/rpl/deinit.inc
