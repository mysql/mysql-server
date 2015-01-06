# include/wait_until_count_sessions.inc
#
# SUMMARY
#
#    Waits until the passed number ($count_sessions) of concurrent sessions or
#    a smaller number was observed via
#       SHOW STATUS LIKE 'Threads_connected'
#    or the operation times out.
#    Note:
#    1. We wait for $current_sessions <= $count_sessions because in the use case
#       with count_sessions.inc before and wait_until_count_sessions.inc after
#       the core of the test it could happen that the disconnects of sessions
#       belonging to the preceding test are not finished.
#       sessions at test begin($count_sessions) =  m + n
#       sessions of the previous test which will be soon disconnected = n (n >= 0)
#       sessions at test end ($current sessions, assuming the test disconnects
#          all additional sessions) = m
#    2. Starting with 5.1 we could also use
#       SELECT COUNT(*) FROM information_schema.processlist
#       I stay with "SHOW STATUS LIKE 'Threads_connected'" because this
#       runs in all versions 5.0+
#
#
# USAGE
#
#    let $count_sessions= 3;
#    --source include/wait_until_count_sessions.inc
#
#    OR typical example of a test which uses more than one session
#       Such a test could harm successing tests if there is no server shutdown
#       and start between.
#
#       If the testing box is slow than the disconnect of sessions belonging to
#       the current test might happen when the successing test gets executed.
#       This means the successing test might see activities like unexpected
#       rows within the general log or the PROCESSLIST.
#       Example from bug http://bugs.mysql.com/bug.php?id=40377
#       --- bzr_mysql-6.0-rpl/.../r/log_state.result
#       +++ bzr_mysql-6.0-rpl/.../r/log_state.reject
#       @@ -25,6 +25,7 @@
#        event_time user_host ... command_type argument
#        TIMESTAMP  USER_HOST ... Query create table t1(f1 int)
#        TIMESTAMP  USER_HOST ... Query select * from mysql.general_log
#       +TIMESTAMP  USER_HOST ... Quit
#       ....
#
#       What to do?
#       -----------
#       <start of test>
#       # Determine initial number of connections (set $count_sessions)
#       --source include/count_sessions.inc
#       ...
#       connect (con1,.....)
#       ...
#       connection default;
#       ...
#       disconnect con1;
#       ...
#       # Wait until we have reached the initial number of connections
#       # or more than the sleep time above (10 seconds) has passed.
#       # $count_sessions
#       --source include/wait_until_count_sessions.inc
#       <end of test>
#
#       Important note about tests with unfortunate (= not cooperative
#       to successing tests) architecture:
#          connection con1;
#          send SELECT ..., sleep(10)
#          connection default;
#          ...
#          disconnect con1;
#          <end of test>
#       should be fixed by
#          connection con1;
#          send SELECT ..., sleep(10)
#          connection default;
#          ...
#          connect con1;
#          reap;
#          connection default;
#          disconnect con1;
#          <end of test>
#       and not only by appending include/wait_until_count_sessions.inc etc.
#
#
# EXAMPLE
#
#    backup.test, grant3.test
#
#
# Created:
#    2009-01-14 mleich
# Modified:
#    2009-02-24 mleich Fix Bug#43114 wait_until_count_sessions too restrictive,
#                                    random PB failures
#

let $wait_counter= 100;
if ($wait_timeout)
{
   let $wait_counter= `SELECT $wait_timeout * 10`;
}
# Reset $wait_timeout so that its value won't be used on subsequent
# calls, and default will be used instead.
let $wait_timeout= 0;
while ($wait_counter)
{
   let $current_sessions= query_get_value(SHOW STATUS LIKE 'Threads_connected', Value, 1);
   let $success= `SELECT $current_sessions <= $count_sessions`;
   if ($success)
   {
      let $wait_counter= 0;
   }
   if (!$success)
   {
      real_sleep 0.1;
      dec $wait_counter;
   }
}
if (!$success)
{
   --echo # Timeout in wait_until_count_sessions.inc
   --echo # Number of sessions   expected: <= $count_sessions   found: $current_sessions
   SHOW PROCESSLIST;
   --die Timeout in wait_until_count_sessions.inc
}

