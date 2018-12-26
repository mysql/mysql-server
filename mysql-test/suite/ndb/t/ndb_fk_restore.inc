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
--disable_warnings
show create table t2;
--enable_warnings
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

# crummy language
-- let $disable_indexes2=0
if ($disable_indexes) {
-- let $disable_indexes2=1
}
if ($rebuild_indexes) {
-- let $disable_indexes2=1
}

# use --skip-core-file because core from exitHandler() is useless

if (!$disable_indexes2) {

-- echo # restore meta
-- exec $NDB_RESTORE --skip-core-file --verbose=255 -b $the_backup_id -n 1 -m --print_meta $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT

-- echo # restore data
-- exec $NDB_RESTORE --skip-core-file --verbose=255 -b $the_backup_id -n 1 -r $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT
-- exec $NDB_RESTORE --skip-core-file --verbose=255 -b $the_backup_id -n 2 -r $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT

}

if ($disable_indexes2) {

-- echo # restore meta (disable indexes)
-- exec $NDB_RESTORE --skip-core-file --verbose=255 -b $the_backup_id -n 1 -m --print_meta --disable-indexes $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT

# option --disable-indexes should have no effect here but we imitate MCM
-- echo # restore data (disable indexes)
-- exec $NDB_RESTORE --skip-core-file --verbose=255 -b $the_backup_id -n 1 -r --disable-indexes $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT
-- exec $NDB_RESTORE --skip-core-file --verbose=255 -b $the_backup_id -n 2 -r --disable-indexes $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT

}

if ($rebuild_indexes) {

-- echo # rebuild indexes
-- exec $NDB_RESTORE --skip-core-file --verbose=255 -b $the_backup_id -n 1 --print_meta --rebuild-indexes $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT

}

# disable-indexes makes restored table unusable via mysql except for drop

-- echo # show meta
if (!$disable_indexes) {
--disable_warnings
show create table t2;
--enable_warnings
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

# reset
-- let $disable_indexes=
-- let $rebuild_indexes=
-- let $disable_indexes2=

-- disable_query_log
drop table ndb_show_tables_results;
-- enable_query_log
