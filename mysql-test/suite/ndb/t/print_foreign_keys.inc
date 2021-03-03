#
# Print the foreign key constraints associated with the given table
#
# ==== Usage ====
# [--let $ndb_db_name= <database name>]
# --let $ndb_table_name= <table name>
# --source print_foreign_keys.inc
#
# Parameters:
#   $ndb_db_name, $ndb_table_name,


# Table name is mandatory option
if (!$ndb_table_name) {
 --die ERROR IN TEST: Please set $ndb_table_name before calling print_foreign_keys.inc script
}

if (!$ndb_db_name) {
 let $ndb_db_name=test;
}


let $dump_file = $MYSQLTEST_VARDIR/tmp/ndb_show_tables_dump.txt;

# Run ndb_show_tables and dump output to file
# Option gives -p parsable format
--exec $NDB_SHOW_TABLES -p > $dump_file

--disable_query_log
--disable_warnings ER_TABLE_EXISTS_ERROR ONCE
CREATE TABLE test.ndb_object_list (
  id INT,
  type VARCHAR(20),
  state VARCHAR(20),
  logging VARCHAR(20),
  _database VARCHAR(255),
  _schema VARCHAR(20),
  name VARCHAR(512)
);

eval LOAD DATA INFILE '$dump_file' INTO TABLE test.ndb_object_list;
--remove_file $dump_file

# Clean the column "name" from any windows line endings
UPDATE test.ndb_object_list SET name = TRIM(TRAILING '\r' FROM name);
# Trim surrounding quotation marks from type, _database and name
UPDATE test.ndb_object_list SET type = TRIM(BOTH '\'' FROM type);
UPDATE test.ndb_object_list SET _database = TRIM(BOTH '\'' FROM _database);
UPDATE test.ndb_object_list SET name = TRIM(BOTH '\'' FROM name);

# Find the object id of the table whose FKs we have to list and verify
let $ndb_table_id= `SELECT id FROM ndb_object_list
                    WHERE type = "UserTable"
                      AND _database = "$ndb_db_name"
                      AND name = "$ndb_table_name"`;

# Now list all the foreign key names using this table id.
# They will be of form <parent_id>/<child_id>/fk_name
--vertical_results
# truncated table as parent
eval SELECT REGEXP_REPLACE(fk_info.name, '$ndb_table_id/[0-9]+/', '')
       AS "Foreign keys referenced by $ndb_db_name.$ndb_table_name : "
     FROM test.ndb_object_list as fk_info
     WHERE fk_info.type = 'ForeignKey'
       AND fk_info.name LIKE '$ndb_table_id/%';
# truncated table as child
eval SELECT REGEXP_REPLACE(fk_info.name, '[0-9]+/$ndb_table_id/', '')
       AS "Foreign keys on $ndb_db_name.$ndb_table_name : "
     FROM test.ndb_object_list as fk_info
       WHERE fk_info.type = 'ForeignKey'
     AND fk_info.name LIKE '%/$ndb_table_id/%';
--horizontal_results

# Drop the info tables
DROP TABLE test.ndb_object_list;
# Reset input parameters
--let $ndb_db_name=
--let $ndb_table_name=

--enable_query_log
