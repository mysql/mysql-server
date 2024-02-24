--error 1
--exec $MYSQL -e "use test; $forbidden_insert"

--echo Backing up data
--source include/ndb_backup.inc
disable_query_log; drop table t2; drop table t1; enable_query_log;

--let $restore= $NDB_RESTORE -b $the_backup_id
--let $backup= $NDB_BACKUPS-$the_backup_id

--echo Restoring data

--exec $restore -n 1 --restore-meta --disable-indexes $backup >> $trash
--exec $restore -n 1 --restore-data                   $backup >> $trash
--exec $restore -n 2 --restore-data                   $backup >> $trash
--exec $restore -n 2 --rebuild-indexes                $backup >> $trash

select * from t1;
select * from t2;
--exec $MYSQL -e "use test; select * from t2 where $where_clause;"

#--error 1
#--exec $MYSQL -e "use test; $forbidden_insert"

disable_query_log; drop table t2; drop table t1; enable_query_log;
