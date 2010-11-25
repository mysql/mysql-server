#
# SUMMARY
#   Check that statement reading table '$table' doesn't allow concurrent
#   inserts in it.
#
# PARAMETERS 
#   $table         Table in which concurrent inserts should be disallowed.
#   $con_aux1      Name of the first auxiliary connection to be used by this
#                  script.
#   $con_aux2      Name of the second auxiliary connection to be used by this
#                  script.
#   $statement     Statement to be checked.
#   $restore_table Table which might be modified by statement to be checked
#                  and thus needs backing up before its execution and
#                  restoring after it (can be empty).
#
# EXAMPLE
#    lock_sync.test
#
--disable_result_log
--disable_query_log

# Reset DEBUG_SYNC facility for safety.
set debug_sync= "RESET";

if ($restore_table)
{
--eval create temporary table t_backup select * from $restore_table;
}

connection $con_aux1;
set debug_sync='after_lock_tables_takes_lock SIGNAL parked WAIT_FOR go';
--send_eval $statement;

connection $con_aux2;
set debug_sync='now WAIT_FOR parked';
--send_eval insert into $table (i) values (0);

--enable_result_log
--enable_query_log
connection default;
# Wait until concurrent insert is successfully blocked because
# of our statement.
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Locked" and info = "insert into $table (i) values (0)";
--source include/wait_condition.inc

--disable_result_log
--disable_query_log

set debug_sync= 'now SIGNAL go';
connection $con_aux1;
--reap
connection $con_aux2;
--reap
connection default;

if ($success)
{
--echo Success: '$statement' doesn't allow concurrent inserts into '$table'.
}
if (!$success)
{
--echo Error: '$statement' allows concurrent inserts into '$table'!
}

--eval delete from $table where i = 0;

if ($restore_table)
{
--eval truncate table $restore_table;
--eval insert into $restore_table select * from t_backup;
drop temporary table t_backup;
}

# Clean-up. Reset DEBUG_SYNC facility after use.
set debug_sync= "RESET";

--enable_result_log
--enable_query_log
