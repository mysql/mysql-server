# This file is used by auth_sec.acl_table_read_concurrency added as part of
# wl#14087. The test focuses on concurrency tests with read row lock on ACL
# tables.
#
# Does following:
# Case 1:
# - Run SELECT on ACL table fetching row related to user 'u2'.
# - Run a concurrent SELECT ... FOR UPDATE fetch the same row for user 'u2'.
#
# Case 2:
# - Run SELECT ... SHARE MODE on ACL table fetching row related to user # 'u2'.
# - Run a concurrent SELECT ... FOR UPDATE fetch the same row for user 'u2'.

echo # Run the test for all isolation level for table $acl_table;
let $c=1;
while($c <= 4)
{
  let isolation_level = `$get_next_isolation`;

  connection read_con;
  echo "Case 1.$c: Start a transaction in $isolation_level";
  eval SET TRANSACTION ISOLATION LEVEL $isolation_level;
  START TRANSACTION;
  eval SELECT $column_name FROM $acl_table WHERE $column_name='u2';

  echo # Make sure no read row lock is held on row 'u2'.
  eval $query_row_locks;

  echo "Allow the same row to be read for update by SELECT FOR UPDATE"
  connection update_con;
  eval SELECT $column_name FROM $acl_table
       WHERE $column_name='u2' FOR UPDATE;

  echo # Make sure X record locks are held on row 'u2';
  eval $query_row_locks;

  connection read_con;
  COMMIT;

  echo "Case 1.$c: Using $isolation_level with SELECT LOCK IN SHARE MODE";
  connection read_con;
  eval SET TRANSACTION ISOLATION LEVEL $isolation_level;
  START TRANSACTION;
  eval SELECT $column_name FROM $acl_table
       WHERE $column_name='u2' LOCK IN SHARE MODE;

  echo # Make sure no read row lock is held on row 'u2'.
  eval $query_row_locks;

  echo "Allow the same row to be read for update by SELECT FOR UPDATE"
  connection update_con;
  eval SELECT $column_name FROM $acl_table
       WHERE $column_name='u2' FOR UPDATE;

  echo # Make sure X record locks are held on row 'u2';
  eval $query_row_locks;

  connection read_con;
  COMMIT;

  inc $c;
}
