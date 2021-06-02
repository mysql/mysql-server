# Test is used to check correctness of allocation failure handling.
# Test sets debug key 'mem_alloc_number' + 'number of allocation',
# executes two statements:
#   connect (con1, localhost, user1);
#   $test_query
# and check if ER_DA_CONN_LIMIT is issued.
#
#
# Arguments:
#   $test_query = string  testing query
#   $only_test  = number  tests only '$only_test' allocation.
#   $start_from = number  number of allocation from which test starts.
#   $max_tests  = number  max number of tests.

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
