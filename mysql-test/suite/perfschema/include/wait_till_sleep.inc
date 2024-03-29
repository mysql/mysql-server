# Wait till the action of the connection using the DB = 'mysqltest' or
# 'mysqlsupertest' is finished ( Command = 'Sleep').

let $wait_timeout= 10;
let $wait_condition=
SELECT COUNT(*) = 1
FROM performance_schema.threads
WHERE processlist_db IN ('mysqltest','mysqlsupertest')
  AND processlist_command = 'Sleep';
--source include/wait_condition.inc
if (!$success)
{
   --echo # Error: We did not reach the expected state where processlist_command = 'Sleep'
   SELECT * FROM performance_schema.threads
   WHERE processlist_db IN ('mysqltest','mysqlsupertest');
   --echo # abort
   exit;
}

