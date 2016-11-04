--require r/have_ndb_binlog_index_innodb.require
disable_query_log;
eval select (engine = 'InnoDB') as 'have_ndb_binlog_index_innodb' from information_schema.tables where table_name = 'ndb_binlog_index';
enable_query_log;
