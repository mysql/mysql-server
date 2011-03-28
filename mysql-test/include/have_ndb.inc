#
# Check if cluster is available by selecting from is.engines
# if an error about no such table occurs bail out
#

disable_result_log;
disable_query_log;

--error 0, 1109
select @have_ndb:= count(*) from information_schema.engines where engine='ndbcluster' and support in ('YES', 'DEFAULT', 'ENABLED');


if ($mysql_errno){
  # For backward compatibility, implement old fashioned way
  # to check here ie. use SHOW VARIABLES LIKE "have_ndb"
  die Can not determine if server supports ndb without is.engines table;
}


if (!`select @have_ndb`){
  skip Need ndb engine;
}

enable_query_log;
enable_result_log;

--let NDB_BACKUPS=$NDB_BACKUP_DIR/BACKUP/BACKUP

