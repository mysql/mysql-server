# Check  that binlogging are turned on for these tests
--source include/have_log_bin.inc

#
# Run variations of ALTER RENAME based on template query
# on the form ALTER RENAME tx ... ty
#

if (!$query)
{
  die Need the query to test with provided in $query;
}

let $num_tables = `select count(*) from information_schema.tables
                     where TABLE_SCHEMA = 'ndb_ddl_test'`;
if (!$num_tables)
{
  die Could not figure out number of tables in ndb_ddl_test database;
}


let $counter = 1;
while ($counter <= $num_tables)
{
   # Default table name is t$counter, ie. t1, t2, etc
   let $tx=t$counter;
   let $suffix=_new_name; # Small mysqltest trick to facilatate string concat
   let $ty=t$counter$suffix;
   #echo tx: $tx;
   #echo ty: $ty;

   # By default we don't expect any error
   let $expected_error = 0;

  if ($counter == 2)
  {
    # Rename to same name
    #   i.e t2 -> t2
    let $ty = $tx;
    if ($expect_rename_to_same_name_to_fail == 1)
    {
      let $expected_error = ER_TABLE_EXISTS_ERROR;
    }
  }

  if ($counter == 3)
  {

    ##
    # Rename to same name but different db
    #   i.e t3 -> ndb_ddl_test2.t3
    #
    let $ty = ndb_ddl_test2.t3;
  }

  if ($counter == 4)
  {
    ##
    # Rename to different name and different db
    #  ie. t4 -> ndb_ddl_test2.t4
    #
    let $ty = ndb_ddl_test2.t4_new_name;
  }

  if ($counter == 5)
  {
    ##
    # Rename to a existing table name
    #  ie. t5 -> t2
    let $ty = t2;
    # This should fail
    let $expected_error = ER_TABLE_EXISTS_ERROR;
  }

  # Replace ty and tx in the query and run it
  let $replaced_query =
     `select REPLACE(REPLACE('$query', 'tx', '$tx'), 'ty', '$ty')`;
  if ($expected_error != 0)
  {
    # expect the query to fail
    --error $expected_error
  }
  eval $replaced_query;

  # Check that old name table does not exist
  # (if the rename was not expected to fail
  #  and the rename is not to the same name)
  # Note : using multiple ifs as MTR has a weird bug processing
  #        `if ($expected_error == 0 && $tx != $ty)`
  if ($expected_error == 0) {
    if ($tx != $ty) {
      --disable_query_log
      --disable_result_log
      --error ER_NO_SUCH_TABLE
      eval SELECT count(*) FROM $tx;
      --enable_query_log
      --enable_result_log
    }
  }

  if ($expected_error != 0){
    # The rename was expected to fail. Due to this, we can't
    # verify the new table. Rather, check if the rollback was
    # proper by verifying that the old table is still intact.
    # We do this by assigning the old name to $ty and let the
    # verification continue.
    let $ty = $tx;
  }

  # Check that new name table exists
  --disable_query_log
  --disable_result_log
  eval SELECT * FROM $ty;
  --enable_query_log
  --enable_result_log

  # Check that new table contains expected number of rows
  let $ty_count = `SELECT count(*) from $ty`;
  if ($ty_count != 5)
  {
    echo Wrong number of rows, ecpected 5 got $ty_count;
    die Wrong number of rows in renamed table;
  }

  # List all NDB objects for the renamed table and store them
  # in temporary table in the test database
  let $create_table_name = test.post_$tx;
  let $list_table_name = $ty;
  --source list_objects.inc
  #eval select * from test.pre_$tx;
  #eval select * from test.post_$tx;

  # Compare the objects(indexes, blob tables etc.) on the renamed table whith
  # what was there before. Ignore id and version since those are not expected
  # to be the same
  --disable_query_log
  eval CREATE TEMPORARY TABLE test.diff_objects
    SELECT * FROM (
      SELECT type, state, parent_obj_type,
        REPLACE(fq_name, parent_obj_id, 'X') AS masked_fq_name
          FROM test.pre_$tx
      UNION ALL
      SELECT type, state, parent_obj_type,
        REPLACE(fq_name, parent_obj_id, 'X') AS masked_fq_name
          FROM test.post_$tx
    ) t
    /* group by columns to compare */
    GROUP BY type, state, parent_obj_type, masked_fq_name
    /* return only those without match */
    HAVING COUNT(*) = 1;
  --enable_query_log

  let $diff_rows = `SELECT count(*) FROM test.diff_objects`;
  if ($diff_rows)
  {
    echo Detected diff in objects on renamed table;
    eval SELECT * FROM test.pre_$tx ORDER BY ID;
    eval SELECT * FROM test.post_$tx ORDER BY ID;
    SELECT  * FROM test.diff_objects ORDER BY ID;
    die The objects created on the renamed table differed!;
  }
  --disable_query_log
  DROP TABLE test.diff_objects;
  --enable_query_log

  inc $counter;
}

--source verify_mysql_dd.inc


# Reset parameters which should change between each invocation
let $sql =;
let $expect_rename_to_same_name_to_fail =;
