if (!$create_table_name)
{
  die Need the name of temporary table where to put list of objects;
}

if (!$list_table_name)
{
  die Need the name of the table to list objects for;
}

--disable_query_log

# Create temporary table for storing the original
# list of objects in the table
eval CREATE TEMPORARY TABLE $create_table_name (
  id INT UNSIGNED PRIMARY KEY,
  type INT UNSIGNED,
  version INT UNSIGNED,
  state INT UNSIGNED,
  parent_obj_type INT  UNSIGNED,
  parent_obj_id INT  UNSIGNED,
  fq_name VARCHAR(512)
);

# Split the table_name variable into db and name part
# since ndbinfo.ndb$dict_obj_info.parent_fq_name uses format
# db/catalog/name
let $_db = `SELECT IF(INSTR('$list_table_name', '.'),
                      SUBSTRING_INDEX('$list_table_name', '.', 1),
                      'ndb_ddl_test')`; # Default to ndb_ddl_test
let $_name = `SELECT SUBSTRING_INDEX('$list_table_name', '.', -1)`;

# Create a view on top of  ndbinfo.ndb$dict_obj_info (as otherwise
# the dollar sign is evaluated as a dollar-variable and there seem
# to be no way to escape it)
CREATE VIEW test.ndbinfo_dict_obj_info as
  SELECT id, type, version, state, parent_obj_type, parent_obj_id, fq_name
    FROM ndbinfo.ndb$dict_obj_info;

let $table_id = `SELECT id FROM test.ndbinfo_dict_obj_info
                   WHERE fq_name = '$_db/def/$_name'`;
eval INSERT INTO $create_table_name
  SELECT * FROM test.ndbinfo_dict_obj_info
    WHERE parent_obj_id = $table_id;

DROP VIEW test.ndbinfo_dict_obj_info;

# Show the object list
#eval select * from $create_table_name;

--enable_query_log

# Reset arguments
let $create_table_name =;
let $list_table_name =;
