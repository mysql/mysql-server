let $Binlog_condition = not like '%ndb_apply_status%';

--source suite/ndb_binlog/t/ndb_binlog_get_binlog_stmts_basic.inc
