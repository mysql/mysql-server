#
# Print the foreign key definitions for the given table and then
# verify that they are consistent in DD and NDB
#
# ==== Usage ====
# [--let $ndb_db_name= <database name>]
# --let $ndb_table_name= <table name>
# --source verify_foreign_keys.inc
#
# Parameters:
#   $ndb_db_name, $ndb_table_name
#     DB and name of the table from which the names of
#     FKs should to be printed and verified
#     Note : default db name "test" will be used if
#            none is passed to $ndb_db_name
#

# Table name is mandatory option
if (!$ndb_table_name) {
 --die ERROR IN TEST: Please set $ndb_table_name before calling verify_foreign_keys.inc script
}

if (!$ndb_db_name) {
 let $ndb_db_name=test;
}

# Print foreign key definitions using show create table
eval SHOW CREATE TABLE $ndb_db_name.$ndb_table_name;

let $dump_file = $MYSQLTEST_VARDIR/tmp/ndb_show_tables_dump.txt;

# Build the string used for executing ndb_show_tables
let $show_cmd = $NDB_SHOW_TABLES;
let $show_cmd = $show_cmd -p;  # parsable format

# Run ndb_show_tables and dump output to file
--exec $show_cmd > $dump_file

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
# They will be of form <parent_id>/<$table_id(child)>/fk_name
# Also extract the parent table name to verify it with the ones stored in DD
CREATE TABLE test.ndb_fk_info (name VARCHAR(255), parent VARCHAR(255)) engine ndb;
eval INSERT INTO test.ndb_fk_info
     SELECT REGEXP_REPLACE(fk_info.name, '[0-9]+/$ndb_table_id/', ''), parent_info.name
     FROM test.ndb_object_list as fk_info, test.ndb_object_list as parent_info
     WHERE fk_info.type = 'ForeignKey'
       AND fk_info.name LIKE '%/$ndb_table_id/%'
       AND parent_info.type = 'UserTable'
       AND parent_info.id = REGEXP_REPLACE(fk_info.name, '/$ndb_table_id/.+', '');

let $ndb_fk_count= `SELECT count(*) FROM ndb_fk_info`;

# Verify that the same set of FKs are present in NDB and DD
# Do left and right outer joins to detect if either DD or NDB has
# any foreign key that does not have a match yet.
let $fks_in_ndb_but_not_in_dd=
     `SELECT COUNT(*)
      FROM test.ndb_fk_info as ndb
        LEFT JOIN information_schema.referential_constraints as dd
        ON ndb.name = dd.constraint_name
      WHERE dd.constraint_name is NULL`;
let $fks_in_dd_but_not_in_ndb=
     `SELECT COUNT(*)
      FROM test.ndb_fk_info as ndb
        RIGHT JOIN information_schema.referential_constraints as dd
        ON ndb.name = dd.constraint_name
      WHERE dd.constraint_schema='$ndb_db_name'
        AND dd.table_name='$ndb_table_name'
        AND ndb.name is NULL`;

if ($fks_in_ndb_but_not_in_dd != 0 ||
    $fks_in_dd_but_not_in_ndb != 0) {
  # There is a foreign key mismatch. Print the list of
  # foreign keys in DD and NDB to help debug.
  eval SELECT name
         AS 'Foreign Keys in NDB on table `$ndb_db_name.$ndb_table_name`'
       FROM ndb_fk_info ORDER BY name ASC;

  eval SELECT constraint_name
         AS 'Foreign Keys in  DD on table `$ndb_db_name.$ndb_table_name`'
       FROM information_schema.referential_constraints
       WHERE constraint_schema='$ndb_db_name'
         AND table_name='$ndb_table_name'
       ORDER BY constraint_name ASC;
  --die Foreign keys mismatch in NDB and DD
}

# Successfully verified
if ($ndb_fk_count == 0) {
  --echo No foreign keys found on table `$ndb_db_name.$ndb_table_name`
}
if ($ndb_fk_count > 0) {
  --echo Foreign keys consistent across NDB and DD
}

# Drop the info table
DROP TABLE test.ndb_fk_info, test.ndb_object_list;
# Reset input parameters
--let $ndb_db_name=
--let $ndb_table_name=

--enable_query_log
