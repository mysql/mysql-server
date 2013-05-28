#

-- disable_query_log
CREATE TEMPORARY TABLE IF NOT EXISTS ndb_show_tables_results (
  id INT,
  type VARCHAR(20),
  state VARCHAR(20),
  logging VARCHAR(20),
  _database VARCHAR(255),
  _schema VARCHAR(20),
  name VARCHAR(255)
);
-- enable_query_log

-- echo # show meta
show create table t2;
-- source ndb_show_tables_result.inc
-- replace_regex /_[0-9]+/_XX/ /[0-9]+\/[0-9]+/XX\/XX/
-- sorted_result
select type,name from ndb_show_tables_results
where type in ('''ForeignKey''','''FKParentTrigger''','''FKChildTrigger''');

-- echo # show counts
select count(*) from t1;
select count(*) from t2;

-- echo # run backup
-- source include/ndb_backup.inc

-- echo # drop tables
drop table t2, t1;

# use --skip-core-file because core from exitHandler() is useless

-- echo # restore meta
if (!$disable_indexes) {
-- exec $NDB_RESTORE --no-defaults --skip-core-file --verbose=255 -b $the_backup_id -n 1 -m --print_meta $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT
}
if ($disable_indexes) {
-- exec $NDB_RESTORE --no-defaults --skip-core-file --verbose=255 -b $the_backup_id -n 1 -m --print_meta --disable-indexes $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT
}

-- echo # restore data
-- exec $NDB_RESTORE --no-defaults --skip-core-file --verbose=255 -b $the_backup_id -n 1 -r $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT
-- exec $NDB_RESTORE --no-defaults --skip-core-file --verbose=255 -b $the_backup_id -n 2 -r $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT

# disable-indexes makes restored table unusable via mysql except for drop

-- echo # show meta
if (!$disable_indexes) {
show create table t2;
}
-- source ndb_show_tables_result.inc
-- replace_regex /_[0-9]+/_XX/ /[0-9]+\/[0-9]+/XX\/XX/
-- sorted_result
select type,name from ndb_show_tables_results
where type in ('''ForeignKey''','''FKParentTrigger''','''FKChildTrigger''');

-- echo # show counts
if (!$disable_indexes) {
select count(*) from t1;
select count(*) from t2;
}

-- disable_query_log
drop table ndb_show_tables_results;
-- enable_query_log
