# Wait till the action of the connection using the DB = 'mysqltest' or
# 'mysqlsupertest' is finished ( Command = 'Sleep').
let $wait_timeout= 5;
let $wait_condition=
SELECT COUNT(*) = 1
FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE DB IN ('mysqltest','mysqlsupertest') AND Command = 'Sleep';
--source include/wait_condition.inc
if (!$success)
{
  --echo # Error: We did not reach the expected state where Command = 'Sleep'
  --echo #        The current (maybe better) state is:
  SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE DB IN ('mysqltest','mysqlsupertest');
  --echo # abort
  exit;
}

