# Test is used to check correctness of allocation failure handling.
# Test sets debug key 'mem_alloc_number' + 'number of allocation',
# executes two statements:
#   connect (con1, localhost, user1);
#   $test_query
# and check if ER_DA_CONN_LIMIT is issued.
#
#
# Arguments:
#   $memory_key    = string  memory key to fail allocation
#   $test_query    = string  testing query
#   $only_test     = number  tests only '$only_test' allocation.
#   $start_from    = number  number of allocation from which test starts.
#   $max_tests     = number  max number of tests.
#   $validate_data = string  table to check diffs, empty to disable
#   $missing_key   = number  number not present on table until command succeeds
#
# Example:
#   --let $only_test = 0
#   --let $start_from = 1
#   --let $max_tests = 1000
#   --let $validate_data = server1:test.t1, server2:test.t1
#   --let $memory_key = memory/group_rpl/write_set_encoded
#   --let $missing_key = 1
#   --let $test_query = INSERT INTO t1 (f0) VALUES ($missing_key)
#   --source ../include/gr_mem_cnt_check_err.inc

--let $_saved_connection= $CURRENT_CONNECTION
connection default;

--disable_query_log
--disable_result_log
CREATE USER 'user1'@localhost;
GRANT USAGE ON *.* TO 'user1'@localhost;
GRANT RELOAD ON *.* TO 'user1'@localhost;
GRANT EXECUTE ON *.* TO 'user1'@localhost;
GRANT FILE ON *.* TO 'user1'@localhost;

let $min_tests = 1;
let $error_count = 0;

let $cnt =  $start_from;
while ($cnt < $max_tests)
{
  let $dbug_name = set global debug="+d, $memory_key, alloc_number$cnt";
  if ($verbose)
  {
    --echo $dbug_name
  }
  if ($only_test)
  {
    let $dbug_name = set global debug="+d, $memory_key, alloc_number$only_test";
  }
  eval $dbug_name;

  connect (con1, localhost, user1);
  if (!$mysql_errno)
  {
    if ($validate_data)
    {
    --let $gtid_executed_before_query= `SELECT @@GLOBAL.GTID_EXECUTED`
    }
    --error ER_DA_CONN_LIMIT,0
    eval $test_query;
    if ($verbose)
    {
      --echo alloc_number = $cnt, error = $mysql_errno;
    }
    if ($mysql_errno)
    {
      inc $error_count;
    }
    if (!$mysql_errno)
    {
      if ($cnt > $min_tests)
      {
        let $cnt = $max_tests;
      }
    }
  }

  disconnect con1;
  connection default;

  if ($mysql_errno)
  {
    if ($validate_data)
    {
      --source include/rpl/sync.inc

      --let $diff_tables = $validate_data
      --source include/diff_tables.inc

      --let $gtid_executed_after_query= `SELECT @@GLOBAL.GTID_EXECUTED`
      --let $assert_text= 'GTID_EXECUTED before and after query must be equal'
      --let $assert_cond= "$gtid_executed_before_query" = "$gtid_executed_after_query"
      --source include/assert.inc

      --let $assert_text= 'Assert key is not present on table'
      --let $assert_cond= [SELECT COUNT(*) AS count FROM t1 WHERE t1.f0 = $missing_key, count, 1] = 0
      --source include/assert.inc
    }
  }
  if ($only_test)
  {
    let $cnt = $max_tests;
  }

  set global debug=default;
  inc $cnt;
}
if ($error_count)
{
  --echo $memory_key test passed.
}
DROP USER 'user1'@localhost;

--enable_query_log
--enable_result_log

--connection $_saved_connection
