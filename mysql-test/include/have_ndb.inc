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

-- let have_embedded=`select version() like '%embedded%'`
if ($have_embedded)
{
# Embedded server does not have 'wait_setup' mechanism
# so must use script to wait till it's started up
--source include/ndb_not_readonly.inc
}

#
# cleanup
#

disable_warnings;
--error 0,1051
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9,t10;
flush tables;
flush status;
enable_warnings;

enable_query_log;
enable_result_log;

--let NDB_BACKUPS=$NDB_BACKUP_DIR/BACKUP/BACKUP

