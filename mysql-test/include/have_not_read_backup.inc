--require r/have_not_read_backup.require
disable_query_log;
show variables like 'ndb_read_backup';
enable_query_log;
