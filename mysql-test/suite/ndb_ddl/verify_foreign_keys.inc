#
# Print the foreign key definitions for the given table and then
# verify that they are consistent across all connected mysqld DDs and NDB
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

--disable_query_log

# Create view which displays all user tables and foreign keys.
# NOTE! Uses a format which is backward compatible with the output
# from "ndb_show_tables" previously used by this test
CREATE VIEW test.ndb_object_list AS
  SELECT
    id,
    /* Hardcoded names for the types selected */
    CASE type
      WHEN 2 THEN "UserTable"
      WHEN 25 THEN "ForeignKey"
      ELSE "UnexpectedType"
    END AS type,

    /* Database is first part of fq_name for user tables */
    CASE type
      WHEN 2 THEN SUBSTRING_INDEX(fq_name, "/", 1)
      ELSE ""
    END AS _database,

    /* Table name is last part of fq_name for user tables and fq_name for fk */
    CASE type
      WHEN 2 THEN SUBSTRING_INDEX(fq_name, "/", -1)
      ELSE fq_name
    END AS name
  FROM ndbinfo.dict_obj_info WHERE type IN ( 2 /* User table */,
                                             25 /* Foreign key definition */);

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

# Loop all the connected mysqlds, print and verify foreign keys
let $i = 1;
while($i <= $NUM_MYSQLDS)
{
  --connection mysqld$i

  # Verify that the same set of FKs are present in NDB and DD
  # Do left and right outer joins to detect if either DD or NDB has
  # any foreign key that does not have a match yet.
  let $fks_in_ndb_but_not_in_dd=
       `SELECT COUNT(*)
        FROM test.ndb_fk_info as ndb
          LEFT JOIN information_schema.referential_constraints as dd
          ON ndb.name = dd.constraint_name
        WHERE dd.constraint_name is NULL
          AND dd.referenced_table_name = ndb.parent`;

  let $fks_in_dd_but_not_in_ndb=
       `SELECT COUNT(*)
        FROM test.ndb_fk_info as ndb
          RIGHT JOIN information_schema.referential_constraints as dd
          ON ndb.name = dd.constraint_name
        WHERE dd.constraint_schema='$ndb_db_name'
          AND dd.table_name='$ndb_table_name'
          AND ndb.name is NULL
          AND dd.referenced_table_name = ndb.parent`;

  if ($fks_in_ndb_but_not_in_dd != 0 ||
      $fks_in_dd_but_not_in_ndb != 0) {
    # There is a foreign key mismatch. Print the list of
    # foreign keys in this DD and NDB to help debug.
    eval SELECT name
           AS 'Foreign Keys in NDB on table `$ndb_db_name.$ndb_table_name`'
         FROM ndb_fk_info ORDER BY name ASC;

    eval SELECT constraint_name
           AS 'Foreign Keys in mysqld$i\'s DD on table `$ndb_db_name.$ndb_table_name`'
         FROM information_schema.referential_constraints
         WHERE constraint_schema='$ndb_db_name'
           AND table_name='$ndb_table_name'
         ORDER BY constraint_name ASC;

    --die Foreign keys mismatch in NDB and DD
  }

  inc $i;
}

# Successfully verified
if ($ndb_fk_count == 0) {
  --echo No foreign keys found on table `$ndb_db_name.$ndb_table_name`
}
if ($ndb_fk_count > 0) {
  --echo Foreign keys consistent across NDB and all connected mysqld's DDs
}

# Drop the info table and view
--connection mysqld1
DROP TABLE test.ndb_fk_info;
DROP VIEW test.ndb_object_list;

--enable_query_log

# Reset input parameters
--let $ndb_db_name=
--let $ndb_table_name=
