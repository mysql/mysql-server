--disable_query_log

# Turn off ndb_read_backup, output from explain(s) and SHOW CREATE
# would differ
set @save_ndb_read_backup_1237_value = @@global.ndb_read_backup;
set @@global.ndb_read_backup = OFF;

--enable_query_log
