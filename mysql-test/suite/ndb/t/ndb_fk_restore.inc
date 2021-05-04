#

# Create view which displays all foreign keys.
# NOTE! Uses a format which is backward compatible with the output
# from "ndb_show_tables" previously used by this test - for example names
# have surrounding quotes.
--disable_query_log ONCE
CREATE VIEW ndb_show_tables_results AS
  SELECT
    CASE type
      WHEN 25 THEN "'ForeignKey'"
      WHEN 26 THEN "'FKParentTrigger'"
      WHEN 27 THEN "'FKChildTrigger'"
      ELSE 'UnexpectedType'
    END AS type,
    /* Add surrounding quotes */
    CONCAT("'", CONCAT(fq_name, "'")) AS name
  FROM ndbinfo.dict_obj_info WHERE type IN (25, 26, 27);

#SELECT * FROM ndb_show_tables_results;

-- echo # show meta
show create table t2;
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

if (!$disable_indexes2) {

-- echo # restore meta
-- exec $NDB_RESTORE --verbose=255 -b $the_backup_id -n 1 -m --print_meta $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT

-- echo # restore data
-- exec $NDB_RESTORE --verbose=255 -b $the_backup_id -n 1 -r $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT
-- exec $NDB_RESTORE --verbose=255 -b $the_backup_id -n 2 -r $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT

}

if ($disable_indexes2) {

-- echo # restore meta (disable indexes)
-- exec $NDB_RESTORE --verbose=255 -b $the_backup_id -n 1 -m --print_meta --disable-indexes $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT

# option --disable-indexes should have no effect here but we imitate MCM
-- echo # restore data (disable indexes)
-- exec $NDB_RESTORE --verbose=255 -b $the_backup_id -n 1 -r --disable-indexes $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT
-- exec $NDB_RESTORE --verbose=255 -b $the_backup_id -n 2 -r --disable-indexes $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT

}

if ($rebuild_indexes) {

-- echo # rebuild indexes
-- exec $NDB_RESTORE --verbose=255 -b $the_backup_id -n 1 --print_meta --rebuild-indexes $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT

}

# disable-indexes makes restored table unusable via mysql except for drop

-- echo # show meta
if (!$disable_indexes) {
show create table t2;
}
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

--disable_query_log ONCE
DROP VIEW ndb_show_tables_results;
