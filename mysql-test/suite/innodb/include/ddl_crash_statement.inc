
let $get_create_template=1;
let $final_table_name = $table_name;
# Test for FREE TREE
if ($test_free)
{
  let $crash_count=1;
  let $did_crash=1;
  while ($did_crash)
  {
    --eval $create_statement
    SET GLOBAL innodb_ddl_log_crash_reset_debug = 1;
    eval SET SESSION DEBUG = '+d,ddl_log_crash_before_free_tree_log_$crash_count';
    --source ../mysql-test/suite/innodb/include/ddl_crash.inc
    eval SET SESSION DEBUG = '-d,ddl_log_crash_before_free_tree_log_$crash_count';
    --eval DROP TABLE $table_name
    inc $crash_count;
  }

  let $crash_count=1;
  let $did_crash=1;
  while ($did_crash)
  {
    --eval $create_statement
    SET GLOBAL innodb_ddl_log_crash_reset_debug = 1;
    eval SET SESSION DEBUG = '+d,ddl_log_crash_after_free_tree_log_$crash_count';
    --source ../mysql-test/suite/innodb/include/ddl_crash.inc
    eval SET SESSION DEBUG = '-d,ddl_log_crash_after_free_tree_log_$crash_count';
    --eval DROP TABLE $table_name
    inc $crash_count;
  }

  let $crash_count=1;
  let $did_crash=1;
  while ($did_crash)
  {
    --eval $create_statement
    SET GLOBAL innodb_ddl_log_crash_reset_debug = 1;
    eval SET SESSION DEBUG = '+d,ddl_log_crash_after_free_tree_delete_$crash_count';
    --source ../mysql-test/suite/innodb/include/ddl_crash.inc
    eval SET SESSION DEBUG = '-d,ddl_log_crash_after_free_tree_delete_$crash_count';
    --eval DROP TABLE $table_name
    inc $crash_count;
  }
}

# Test for DELETE SPACE
if ($test_delete)
{
  let $crash_count=1;
  let $did_crash=1;
  while ($did_crash)
  {
    --eval $create_statement
    SET GLOBAL innodb_ddl_log_crash_reset_debug = 1;
    eval SET SESSION DEBUG = '+d,ddl_log_crash_before_delete_space_log_$crash_count';
    --source ../mysql-test/suite/innodb/include/ddl_crash.inc
    eval SET SESSION DEBUG = '-d,ddl_log_crash_before_delete_space_log_$crash_count';
    --eval DROP TABLE $table_name
    inc $crash_count;
  }

  let $crash_count=1;
  let $did_crash=1;
  while ($did_crash)
  {
    --eval $create_statement
    SET GLOBAL innodb_ddl_log_crash_reset_debug = 1;
    eval SET SESSION DEBUG = '+d,ddl_log_crash_after_delete_space_log_$crash_count';
    --source ../mysql-test/suite/innodb/include/ddl_crash.inc
    eval SET SESSION DEBUG = '-d,ddl_log_crash_after_delete_space_log_$crash_count';
    --eval DROP TABLE $table_name
    inc $crash_count;
  }

  let $crash_count=1;
  let $did_crash=1;
  while ($did_crash)
  {
    --eval $create_statement
    SET GLOBAL innodb_ddl_log_crash_reset_debug = 1;
    eval SET SESSION DEBUG = '+d,ddl_log_crash_after_delete_space_delete_$crash_count';
    --source ../mysql-test/suite/innodb/include/ddl_crash.inc
    eval SET SESSION DEBUG = '-d,ddl_log_crash_after_delete_space_delete_$crash_count';
    --eval DROP TABLE $table_name
    inc $crash_count;
  }
}

# Test for RENAME SPACE
if ($test_rename)
{
  let $crash_count=1;
  let $did_crash=1;
  while ($did_crash)
  {
    --eval $create_statement
    SET GLOBAL innodb_ddl_log_crash_reset_debug = 1;
    eval SET SESSION DEBUG = '+d,ddl_log_crash_before_rename_space_log_$crash_count';
    --source ../mysql-test/suite/innodb/include/ddl_crash.inc
    eval SET SESSION DEBUG = '-d,ddl_log_crash_before_rename_space_log_$crash_count';
    let $drop_table_name = $table_name;
    if ($rename_flag)
    {
      if ($did_crash == 0)
      {
        let $drop_table_name = $rename_table_name;
      }
    }
    --eval DROP TABLE $drop_table_name
    inc $crash_count;
  }

  let $crash_count=1;
  let $did_crash=1;
  while ($did_crash)
  {
    --eval $create_statement
    SET GLOBAL innodb_ddl_log_crash_reset_debug = 1;
    eval SET SESSION DEBUG = '+d,ddl_log_crash_after_rename_space_log_$crash_count';
    --source ../mysql-test/suite/innodb/include/ddl_crash.inc
    eval SET SESSION DEBUG = '-d,ddl_log_crash_after_rename_space_log_$crash_count';
    let $drop_table_name = $table_name;
    if ($rename_flag)
    {
      if ($did_crash == 0)
      {
        let $drop_table_name = $rename_table_name;
      }
    }
    --eval DROP TABLE $drop_table_name
    inc $crash_count;
  }

  let $crash_count=1;
  let $did_crash=1;
  while ($did_crash)
  {
    --eval $create_statement
    SET GLOBAL innodb_ddl_log_crash_reset_debug = 1;
    eval SET SESSION DEBUG = '+d,ddl_log_crash_after_rename_space_delete_$crash_count';
    --source ../mysql-test/suite/innodb/include/ddl_crash.inc
    eval SET SESSION DEBUG = '-d,ddl_log_crash_after_rename_space_delete_$crash_count';
    let $drop_table_name = $table_name;
    if ($rename_flag)
    {
      if ($did_crash == 0)
      {
        let $drop_table_name = $rename_table_name;
      }
    }
    --eval DROP TABLE $drop_table_name
    inc $crash_count;
  }
}


# Test for DROP
if ($test_drop)
{
  let $crash_count=1;
  let $did_crash=1;
  while ($did_crash)
  {
    --eval $create_statement
    SET GLOBAL innodb_ddl_log_crash_reset_debug = 1;
    eval SET SESSION DEBUG = '+d,ddl_log_crash_after_drop_log_$crash_count';
    --source ../mysql-test/suite/innodb/include/ddl_crash.inc
    eval SET SESSION DEBUG = '-d,ddl_log_crash_after_drop_log_$crash_count';
    --eval DROP TABLE $table_name
    inc $crash_count;
  }
}

# Get the table definition
--disable_query_log
--disable_result_log

--eval $create_statement
--eval $insert_statement
--eval SELECT * FROM $table_name LIMIT 1
--eval $crash_statement
if ($rename_flag)
{
  let $final_table_name = $rename_table_name;
}
let $create_template= query_get_value(SHOW CREATE TABLE $final_table_name, Create Table, 1);
--eval DROP TABLE $final_table_name

--enable_result_log
--enable_query_log

let $get_create_template=0;


# Test for POST DDL
--eval $create_statement
SET GLOBAL innodb_ddl_log_crash_reset_debug = 1;
eval SET SESSION DEBUG = '+d,ddl_log_before_post_ddl';
--source ../mysql-test/suite/innodb/include/ddl_crash.inc
eval SET SESSION DEBUG = '-d,ddl_log_before_post_ddl';
--eval DROP TABLE $final_table_name

# Test for REPLAY
let $crash_count=1;
let $did_crash=1;
while ($did_crash)
{
  --eval $create_statement
  SET GLOBAL innodb_ddl_log_crash_reset_debug = 1;
  eval SET SESSION DEBUG = '+d,ddl_log_crash_after_replay_$crash_count';
  --source ../mysql-test/suite/innodb/include/ddl_crash.inc
  eval SET SESSION DEBUG = '-d,ddl_log_crash_after_replay_$crash_count';
  --eval DROP TABLE $final_table_name
  inc $crash_count;
}
