--disable_query_log

# Restore ndb_read_backup value
set @@global.ndb_read_backup = @save_ndb_read_backup_1237_value;

--enable_query_log
